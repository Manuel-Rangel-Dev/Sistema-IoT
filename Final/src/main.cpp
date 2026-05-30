// ============================================================
//  MotorDC_Monitor.ino  –  Motor DC Monitor (archivo único)
//  Hardware: ESP32 + SIM7600/SIM7670G + INA219 + OLED SSD1306
// ============================================================

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

// ============================================================
// CONFIG
// ============================================================

constexpr uint8_t SIM_RX = 17;
constexpr uint8_t SIM_TX = 18;
constexpr uint8_t SIM_DTR = 45;

constexpr char SERVER_HOST[] = "18.226.103.103";
constexpr uint16_t SERVER_PORT = 5000;
constexpr char APN[] = "internet.comcel.com.co";

constexpr uint8_t ENCODER_PIN_A = 13;
constexpr uint8_t ENCODER_PIN_B = 14;
constexpr uint8_t MOTOR_IN1 = 11;
constexpr uint8_t MOTOR_IN2 = 12;
constexpr uint8_t MOTOR_ENA = 10;

constexpr uint8_t SDA_PIN = 16;
constexpr uint8_t SCL_PIN = 15;

constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 32;
constexpr int8_t OLED_RESET = -1;
constexpr uint8_t OLED_ADDR = 0x3C;

constexpr uint8_t PWM_CHANNEL = 0;     // canal LEDC (API core 2.x)
constexpr uint32_t PWM_FREQ = 1000;
constexpr uint8_t PWM_RESOLUTION = 8;
constexpr int PWM_MIN = 0;
constexpr int PWM_MAX = 255;

// Encoder del motor real (12 V con caja reductora):
//   PPR_MOTOR = pulsos por revolución del eje del motor
//   GEAR_RATIO = relación de la caja reductora
constexpr float PPR_MOTOR = 16.0f;
constexpr float GEAR_RATIO = 270.0f;
constexpr float CPR = PPR_MOTOR * GEAR_RATIO;   // = 4320 cuentas por rev. del eje de salida

// ── Tiempos ──────────────────────────────────────────────────────────────────
// El lazo de control corre rápido y local (50 ms) para un PID estable; la
// telemetría se envía por LTE cada 1 s (un POST/segundo es lo más rápido que la
// LTE Cat-1 sostiene de forma fiable). Los parámetros de red/GPS se refrescan
// cada 3 s porque cambian lento y consultarlos es costoso.
constexpr uint16_t CONTROL_INTERVAL_MS = 50;    // lazo de control + muestreo de RPM
constexpr uint16_t SEND_INTERVAL_MS = 1000;     // envío de telemetría HTTP
constexpr uint16_t LTE_POLL_INTERVAL_MS = 3000; // consulta de parámetros de red
constexpr uint16_t PING_POLL_INTERVAL_MS = 10000; // medición de ping (AT+CPING)
constexpr uint16_t SMS_POLL_INTERVAL_MS = 15000;
constexpr uint16_t AT_OLED_HOLD_MS = 2500;

// ── Constantes de control (replicadas del proyecto de teoría de control) ──────
// Feedforward polinomial calibrado para ESTE motor + puente H.
constexpr float FF_CUBIC     = 0.0416f;
constexpr float FF_QUADRATIC = -2.3343f;
constexpr float FF_LINEAR    = 44.086f;
constexpr float FF_OFFSET    = -210.76f;
constexpr float MIN_SETPOINT_RPM = 11.0f;
constexpr float MAX_SETPOINT_RPM = 35.0f;

// ============================================================
// TELEMETRY
// ============================================================

struct Measurements {
  float rpm = 0.0f;
  float current_A = 0.0f;
  float voltage_V = 0.0f;
  float power_W = 0.0f;
  uint8_t pwm = 0;
  // ── Control (lazo cerrado) ──
  float setpoint = 0.0f;
  float error = 0.0f;
  bool running = false;
  // ── GPS (grados decimales; 0 = sin fix) ──
  float lat = 0.0f;
  float lon = 0.0f;
};

// ── Tipos de controlador ──
enum class ControllerType : uint8_t { kP, kPI, kPD, kPID };

// Configuración de control recibida del servidor (servidor → ESP32).
struct ControlConfig {
  bool running = false;
  bool manual = false;            // true = PWM directo; false = PID
  ControllerType type = ControllerType::kPID;
  float setpoint_rpm = 25.0f;
  float kp = 2.0f;
  float ki = 0.4f;
  float kd = 0.0f;
  uint8_t pwm_manual = 0;
  long version = -1;              // última versión aplicada
};

// ── Parámetros de red LTE (estilo G-NetTrack), refrescados por AT ──
struct LteInfo {
  String oper = "";       // operador (AT+COPS?)
  String tech = "";       // tecnología / system mode (AT+CPSI?)
  int    mcc = 0;
  int    mnc = 0;
  String tac = "";
  String cid = "";
  int    pci = 0;
  String band = "";
  long   earfcn = 0;
  int    rsrp = 0;
  int    rsrq = 0;
  int    rssi = 0;
  int    sinr = 0;
  int    csq = 0;         // 0..31 (AT+CSQ)
  int    csq_dbm = 0;     // convertido a dBm
  String raw_cpsi = "";   // línea cruda de CPSI por si falta parseo
  int    ping_ms = -1;    // RTT medio a 8.8.8.8 (AT+CPING); -1 = sin respuesta
};

// ============================================================
// DISPLAY
// ============================================================

namespace {
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayReady = false;
}

bool beginDisplay() {
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (displayReady) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Estado:");
    display.println("Inicio sistema");
    display.display();
  } else {
    Serial.println("Advertencia: OLED no encontrada.");
  }
  return displayReady;
}

void displayState(const String& message) {
  if (!displayReady) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Estado:");
  display.println(message);
  display.display();
}

void logState(const String& message) {
  Serial.println(message);
  displayState(message);
}

void displayMeasurements(const Measurements& m) {
  if (!displayReady) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("RPM: ");
  display.println(m.rpm, 1);
  display.print("I:");
  display.print(m.current_A, 3);
  display.print("A V:");
  display.println(m.voltage_V, 2);
  display.print("P:");
  display.print(m.power_W, 3);
  display.print("W PWM:");
  display.println(m.pwm);
  display.display();
}

// ============================================================
// MOTOR
// ============================================================

namespace {
volatile int32_t encoderCount = 0;
int32_t previousCount = 0;
uint32_t previousTime = 0;
uint8_t currentPwm = 0;

void IRAM_ATTR handleEncoderInterrupt() {
  if (digitalRead(ENCODER_PIN_B) == LOW) {
    ++encoderCount;
  } else {
    --encoderCount;
  }
}
}

void beginMotor() {
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), handleEncoderInterrupt, RISING);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_ENA, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);

  previousTime = millis();
}

void setMotorPwm(uint8_t pwm) {
  currentPwm = pwm;
  ledcWrite(PWM_CHANNEL, pwm);
}

uint8_t getMotorPwm() {
  return currentPwm;
}

float sampleMotorRpm(uint32_t nowMs) {
  noInterrupts();
  int32_t count = encoderCount;
  interrupts();

  float dt = (nowMs - previousTime) / 1000.0f;
  int32_t delta = count - previousCount;
  float rpm = 0.0f;

  if (dt > 0.0f) {
    rpm = -((delta / CPR) / dt * 60.0f);
  }

  previousCount = count;
  previousTime = nowMs;
  return rpm;
}

// ============================================================
// CONTROL (lazo cerrado PID + feedforward) — replica del
// proyecto de teoría de control, ahora gobernado por el servidor.
// ============================================================

namespace {
ControlConfig controlCfg;
float pidIntegral = 0.0f;
float pidPrevError = 0.0f;

const char* controllerName(ControllerType type) {
  switch (type) {
    case ControllerType::kP:  return "P";
    case ControllerType::kPI: return "PI";
    case ControllerType::kPD: return "PD";
    case ControllerType::kPID:return "PID";
  }
  return "PID";
}

ControllerType parseController(const String& v, ControllerType fallback) {
  if (v == "P")   return ControllerType::kP;
  if (v == "PI")  return ControllerType::kPI;
  if (v == "PD")  return ControllerType::kPD;
  if (v == "PID") return ControllerType::kPID;
  return fallback;
}

void resetPidState() {
  pidIntegral = 0.0f;
  pidPrevError = 0.0f;
}

// Calcula el PWM con feedforward + PID (idéntico al firmware Arduino de control).
int computeControlPwm(float rpm, float dt) {
  const float sp = controlCfg.setpoint_rpm;
  const float error = sp - rpm;
  const bool uses_i = (controlCfg.type == ControllerType::kPI || controlCfg.type == ControllerType::kPID);
  const bool uses_d = (controlCfg.type == ControllerType::kPD || controlCfg.type == ControllerType::kPID);

  if (uses_i) {
    pidIntegral += error * dt;
    pidIntegral = constrain(pidIntegral, -300.0f, 300.0f);
  } else {
    pidIntegral = 0.0f;
  }

  const float derivative = (dt > 0.0f) ? (error - pidPrevError) / dt : 0.0f;
  const float feed_forward = constrain(
      FF_CUBIC * sp * sp * sp + FF_QUADRATIC * sp * sp + FF_LINEAR * sp + FF_OFFSET,
      0.0f, 255.0f);

  float correction = controlCfg.kp * error;
  if (uses_i) correction += controlCfg.ki * pidIntegral;
  if (uses_d) correction += controlCfg.kd * derivative;

  pidPrevError = error;
  return constrain((int)(feed_forward + correction), PWM_MIN, PWM_MAX);
}
}  // namespace

// ============================================================
// SENSORS
// ============================================================

namespace {
Adafruit_INA219 ina219;
}

bool beginSensors() {
  return ina219.begin();
}

void readElectricalMeasurements(Measurements& m) {
  float shuntVoltage_mV = ina219.getShuntVoltage_mV();
  float busVoltage_V = ina219.getBusVoltage_V();
  m.current_A = ina219.getCurrent_mA() / 1000.0f;
  m.power_W = ina219.getPower_mW() / 1000.0f;
  m.voltage_V = busVoltage_V + (shuntVoltage_mV / 1000.0f);
}

// ============================================================
// LTE
// ============================================================

namespace {
HardwareSerial simSerial(1);
TinyGsm modem(simSerial);
TinyGsmClient client(modem);
String g_localIP = "";   // IP asignada por la red (se llena en connectLTE)

String cleanATResponse(const String& cmd, String response) {
  response.replace("\r", "\n");
  response.replace(cmd, "");

  String firstLine = "";
  String finalStatus = "";
  int start = 0;

  while (start < (int)response.length()) {
    int end = response.indexOf('\n', start);
    if (end == -1) end = response.length();

    String line = response.substring(start, end);
    line.trim();

    if (line.length() > 0) {
      if (line.startsWith("+")) return line;
      if (line == "ERROR")     return line;
      if (line == "OK")        finalStatus = line;
      if (firstLine.length() == 0) firstLine = line;
    }
    start = end + 1;
  }

  if (firstLine.length() > 0)  return firstLine;
  if (finalStatus.length() > 0) return finalStatus;
  return "Sin respuesta";
}

String atLabel(const String& cmd) {
  String label = cmd;
  if (label.startsWith("AT+"))  label.remove(0, 3);
  else if (label.startsWith("AT")) label.remove(0, 2);
  label.replace("?", "");
  return label.length() == 0 ? cmd : label;
}

void syncSIM() {
  logState("Sincronizando SIM7670G");
  for (int i = 0; i < 5; i++) {
    simSerial.println("AT");
    delay(500);
    if (simSerial.find("OK")) {
      logState("Modulo respondiendo");
      return;
    }
  }
  logState("Modulo no responde");
}
}

HardwareSerial& simSerialPort() {
  return simSerial;
}

void beginModemSerial() {
  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW);
  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
}

void sendAT(const char* cmd, int waitMs = 2000) {
  while (simSerial.available()) simSerial.read();
  simSerial.println(cmd);
  Serial.print(">> ");
  Serial.println(cmd);

  long start = millis();
  String response = "";
  while (millis() - start < waitMs) {
    while (simSerial.available()) {
      char c = simSerial.read();
      response += c;
      Serial.write(c);
    }
  }
  Serial.println();

  String message = atLabel(cmd) + ": ";
  message += cleanATResponse(cmd, response);
  displayState(message);
  delay(AT_OLED_HOLD_MS);
}

String readATResponse(unsigned long timeoutMs = 3000) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (simSerial.available()) {
      char c = simSerial.read();
      response += c;
    }
    if (response.indexOf("\r\nOK\r\n") != -1 || response.indexOf("\r\nERROR\r\n") != -1) break;
  }
  return response;
}

// Envía un comando AT y devuelve la respuesta completa (sin tocar la OLED).
// Se usa en tiempo de ejecución para consultar GPS y parámetros de red.
String queryAT(const char* cmd, unsigned long timeoutMs = 2000) {
  // IMPORTANTE: se enruta por TinyGSM (no se escribe el UART en crudo) para no
  // corromper su buffer interno. Mezclar lecturas crudas con el cliente TCP
  // desincroniza a TinyGSM e impide leer la respuesta HTTP del servidor.
  const char* suffix = cmd;
  if (suffix[0] == 'A' && suffix[1] == 'T') suffix += 2;  // sendAT() ya antepone "AT"
  String response;
  modem.sendAT(suffix);
  modem.waitResponse(timeoutMs, response);
  return response;
}

// Extrae el i-ésimo campo (0-based) de una línea separada por comas.
String csvField(const String& line, int index) {
  int start = 0;
  for (int i = 0; i < index; i++) {
    start = line.indexOf(',', start);
    if (start == -1) return "";
    start++;
  }
  int end = line.indexOf(',', start);
  if (end == -1) end = line.length();
  String f = line.substring(start, end);
  f.trim();
  return f;
}

// ============================================================
// GPS (AT+CGNSSINFO)
// ============================================================

void beginGps() {
  logState("Activando GPS");
  // El SIM7670G enciende el GNSS con un solo AT+CGNSSPWR=1. El primer fix
  // tarda minutos; NO bloqueamos esperando el URC "+CGNSSPWR: READY!".
  queryAT("AT+CGNSSPWR=1", 3000);
  delay(200);
}

// Lee la posición del SIM7670G con AT+CGNSSINFO. En este módulo la lat/lon YA
// vienen en GRADOS DECIMALES (no NMEA): no se convierte, solo se aplica el signo.
// Formato 0-based: 5=lat, 6=N/S, 7=lon, 8=E/O.
bool readGPS(Measurements& m) {
  String resp = queryAT("AT+CGNSSINFO", 2000);   // mismo AT, enrutado por TinyGSM
  int idx = resp.indexOf("+CGNSSINFO:");
  if (idx == -1) { m.lat = 0.0f; m.lon = 0.0f; return false; }

  int lineEnd = resp.indexOf('\n', idx);
  if (lineEnd == -1) lineEnd = resp.length();
  String line = resp.substring(idx + 11, lineEnd);
  line.trim();
  Serial.print("[GPS] CGNSSINFO: ");
  Serial.println(line);   // línea vacía (",,,,") = encendido pero aún SIN fix

  String latRaw = csvField(line, 5);
  String latH   = csvField(line, 6);
  String lonRaw = csvField(line, 7);
  String lonH   = csvField(line, 8);

  if (latRaw.length() == 0 || lonRaw.length() == 0) {
    m.lat = 0.0f; m.lon = 0.0f;
    return false;   // todavía no hay fix
  }

  float lat = latRaw.toFloat();
  float lon = lonRaw.toFloat();
  if (latH == "S") lat = -lat;   // hemisferio sur → negativo
  if (lonH == "W") lon = -lon;   // oeste → negativo
  m.lat = lat;
  m.lon = lon;
  return true;
}

// ============================================================
// PARÁMETROS DE RED LTE (AT+CPSI? / AT+CSQ / AT+COPS?)
// ============================================================

void pollLte(LteInfo& lte) {
  // ── Calidad de señal (AT+CSQ) ──
  String csq = queryAT("AT+CSQ", 1500);
  int ci = csq.indexOf("+CSQ:");
  if (ci != -1) {
    String rest = csq.substring(ci + 5);
    rest.trim();
    int comma = rest.indexOf(',');
    int rssiRaw = (comma == -1 ? rest : rest.substring(0, comma)).toInt();
    lte.csq = rssiRaw;
    lte.csq_dbm = (rssiRaw == 99) ? 0 : (-113 + 2 * rssiRaw);
  }

  // ── Operador (AT+COPS?) ──
  String cops = queryAT("AT+COPS?", 1500);
  int q1 = cops.indexOf('"');
  if (q1 != -1) {
    int q2 = cops.indexOf('"', q1 + 1);
    if (q2 != -1) lte.oper = cops.substring(q1 + 1, q2);
  }

  // ── Info de celda (AT+CPSI?) ──
  // LTE: <mode>,<op>,<MCC-MNC>,<TAC>,<SCellID>,<PCID>,<band>,<earfcn>,<dlbw>,<ulbw>,<RSRQ>,<RSRP>,<RSSI>,<RSSNR>
  String cpsi = queryAT("AT+CPSI?", 2000);
  int pi = cpsi.indexOf("+CPSI:");
  if (pi != -1) {
    int lineEnd = cpsi.indexOf('\n', pi);
    if (lineEnd == -1) lineEnd = cpsi.length();
    String line = cpsi.substring(pi + 6, lineEnd);
    line.trim();
    lte.raw_cpsi = line;

    lte.tech = csvField(line, 0);
    String mccmnc = csvField(line, 2);
    int dash = mccmnc.indexOf('-');
    if (dash != -1) {
      lte.mcc = mccmnc.substring(0, dash).toInt();
      lte.mnc = mccmnc.substring(dash + 1).toInt();
    }
    lte.tac    = csvField(line, 3);
    lte.cid    = csvField(line, 4);
    lte.pci    = csvField(line, 5).toInt();
    lte.band   = csvField(line, 6);
    lte.earfcn = csvField(line, 7).toInt();
    lte.rsrq   = csvField(line, 10).toInt();
    lte.rsrp   = csvField(line, 11).toInt();
    lte.rssi   = csvField(line, 12).toInt();
    lte.sinr   = csvField(line, 13).toInt();
  }
}

// Mide la latencia (RTT) con AT+CPING a 8.8.8.8. El módem responde OK al instante
// y MÁS TARDE envía el resumen "+CPING: 3,<env>,<recib>,<perd>,<min>,<max>,<avg>".
void pollPing(LteInfo& lte) {
  String resp;
  modem.sendAT("+CPING=\"8.8.8.8\",1,2");
  modem.waitResponse(8000UL, resp, GF("+CPING: 3"));   // espera el resumen
  int pi = resp.indexOf("+CPING: 3");
  if (pi == -1) { lte.ping_ms = -1; return; }
  // Tras el match, el resto del resumen sigue en el stream: ",<env>,<recib>,...,<avg>"
  String line = modem.stream.readStringUntil('\n');
  line.trim();
  int recv = csvField(line, 2).toInt();                // paquetes recibidos
  lte.ping_ms = (recv > 0) ? csvField(line, 6).toInt() : -1;  // RTT medio (ms)
}

// ============================================================
// JSON: parseo de la configuración de control en la respuesta HTTP
// ============================================================

namespace {
String jsonExtractRaw(const String& src, const String& key) {
  int k = src.indexOf("\"" + key + "\"");
  if (k == -1) return "";
  int colon = src.indexOf(':', k);
  if (colon == -1) return "";
  int i = colon + 1;
  while (i < (int)src.length() && (src[i] == ' ' || src[i] == '"')) i++;
  int start = i;
  while (i < (int)src.length() && src[i] != ',' && src[i] != '}' && src[i] != '"') i++;
  String v = src.substring(start, i);
  v.trim();
  return v;
}

float  jsonFloat(const String& s, const String& k) { return jsonExtractRaw(s, k).toFloat(); }
long   jsonInt(const String& s, const String& k)   { return jsonExtractRaw(s, k).toInt(); }
String jsonStr(const String& s, const String& k)   { return jsonExtractRaw(s, k); }
bool   jsonBool(const String& s, const String& k)  { return jsonExtractRaw(s, k) == "true"; }
}

void connectLTE() {
  logState("Iniciando LTE Claro");
  syncSIM();
  sendAT("ATE0");
  sendAT("AT+CPIN?");

  logState("Esperando red...");
  modem.waitForNetwork(60000UL);

  if (modem.isNetworkConnected()) {
    logState("Red registrada");
  } else {
    logState("Error registro red");
  }

  sendAT("AT+CSQ");
  sendAT("AT+CREG?");
  sendAT("AT+CEREG?");

  logState("Conectando APN...");
  modem.gprsConnect(APN, "", "");

  if (modem.isGprsConnected()) {
    logState("LTE conectado");
    g_localIP = modem.getLocalIP();
    Serial.print("IP asignada: ");
    Serial.println(g_localIP);
    displayState("IP asignada");

    logState("Verificando ping...");
    while (simSerial.available()) simSerial.read();
    simSerial.println("AT+CPING=\"8.8.8.8\",1,2");

    unsigned long start = millis();
    String response = "";
    bool pingOk = false;

    while (millis() - start < 6000) {
      while (simSerial.available()) {
        char c = simSerial.read();
        response += c;
        Serial.write(c);
      }
      if (response.indexOf("+CPING: 3") != -1) {
        if (response.indexOf(",0,2") == -1 && response.indexOf(",0,1") == -1) pingOk = true;
        break;
      }
    }

    logState(pingOk ? "Datos activos OK" : "Error ping");
  } else {
    logState("Error APN");
  }

  logState("Fin config LTE");
}

// Construye el cuerpo JSON con telemetría + control + GPS + parámetros LTE.
String buildTelemetryJson(const Measurements& m, const LteInfo& lte) {
  String body = "{";
  body += "\"rpm\":" + String(m.rpm, 2);
  body += ",\"current_A\":" + String(m.current_A, 4);
  body += ",\"voltage_V\":" + String(m.voltage_V, 3);
  body += ",\"power_W\":" + String(m.power_W, 4);
  body += ",\"pwm\":" + String(m.pwm);
  body += ",\"setpoint\":" + String(m.setpoint, 2);
  body += ",\"error\":" + String(m.error, 2);
  body += ",\"controller\":\"" + String(controllerName(controlCfg.type)) + "\"";
  body += ",\"running\":" + String(m.running ? "true" : "false");
  body += ",\"ip\":\"" + g_localIP + "\"";

  // GPS (siempre presente; 0.000000 = sin fix)
  body += ",\"lat\":" + String(m.lat, 6);
  body += ",\"lon\":" + String(m.lon, 6);

  // Parámetros de red LTE
  body += ",\"lte\":{";
  body += "\"operator\":\"" + lte.oper + "\"";
  body += ",\"tech\":\"" + lte.tech + "\"";
  body += ",\"mcc\":" + String(lte.mcc);
  body += ",\"mnc\":" + String(lte.mnc);
  body += ",\"tac\":\"" + lte.tac + "\"";
  body += ",\"cid\":\"" + lte.cid + "\"";
  body += ",\"pci\":" + String(lte.pci);
  body += ",\"band\":\"" + lte.band + "\"";
  body += ",\"earfcn\":" + String(lte.earfcn);
  body += ",\"rsrp\":" + String(lte.rsrp);
  body += ",\"rsrq\":" + String(lte.rsrq);
  body += ",\"rssi\":" + String(lte.rssi);
  body += ",\"sinr\":" + String(lte.sinr);
  body += ",\"csq\":" + String(lte.csq);
  body += ",\"csq_dbm\":" + String(lte.csq_dbm);
  body += ",\"ping_ms\":" + String(lte.ping_ms);
  body += "}";

  body += "}";
  return body;
}

// Envía la telemetría por HTTP usando la pila HTTP NATIVA del SIM7670G
// (AT+HTTPINIT/HTTPPARA/HTTPDATA/HTTPACTION/HTTPREAD), enrutada por TinyGSM.
// Devuelve el cuerpo de la respuesta del servidor (que contiene la config de
// control) o cadena vacía si falla. Este método sí lee la respuesta de forma
// determinista (a diferencia del cliente TCP manual, que perdía el cuerpo).
String sendHttpMeasurements(const Measurements& m, const LteInfo& lte) {
  String body = buildTelemetryJson(m, lte);
  String url  = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/data";

  // Cierra cualquier sesión HTTP previa que hubiera quedado abierta.
  modem.sendAT("+HTTPTERM");
  modem.waitResponse(2000);

  modem.sendAT("+HTTPINIT");
  if (modem.waitResponse(5000) != 1) {
    Serial.println("[HTTP] HTTPINIT fallo");
    return "";
  }

  modem.sendAT("+HTTPPARA=\"URL\",\"" + url + "\"");
  modem.waitResponse(3000);
  modem.sendAT("+HTTPPARA=\"CONTENT\",\"application/json\"");
  modem.waitResponse(3000);

  // Carga el cuerpo: AT+HTTPDATA=<len>,<ms> → prompt "DOWNLOAD" → enviar body.
  modem.sendAT("+HTTPDATA=" + String(body.length()) + ",10000");
  if (modem.waitResponse(5000, GF("DOWNLOAD")) != 1) {
    Serial.println("[HTTP] Sin prompt DOWNLOAD");
    modem.sendAT("+HTTPTERM"); modem.waitResponse(2000);
    return "";
  }
  modem.stream.print(body);
  modem.waitResponse(10000);   // OK tras recibir los datos

  // POST = 1. El resultado llega como URC "+HTTPACTION: 1,<status>,<len>".
  modem.sendAT("+HTTPACTION=1");
  if (modem.waitResponse(20000UL, GF("+HTTPACTION:")) != 1) {
    Serial.println("[HTTP] Sin +HTTPACTION (timeout)");
    modem.sendAT("+HTTPTERM"); modem.waitResponse(2000);
    return "";
  }
  String actionLine = modem.stream.readStringUntil('\n');  // " 1,200,123"
  int statusCode = 0, dataLen = 0;
  int c1 = actionLine.indexOf(',');
  int c2 = actionLine.lastIndexOf(',');
  if (c1 != -1 && c2 != -1 && c2 > c1) {
    statusCode = actionLine.substring(c1 + 1, c2).toInt();
    dataLen    = actionLine.substring(c2 + 1).toInt();
  }
  Serial.print("[HTTP] status="); Serial.print(statusCode);
  Serial.print(" len="); Serial.println(dataLen);

  // Lee el cuerpo de la respuesta con AT+HTTPREAD=0,<len>.
  String resp = "";
  if (dataLen > 0) {
    modem.sendAT("+HTTPREAD=0," + String(dataLen));
    String raw = "";
    unsigned long t0 = millis();
    while (millis() - t0 < 5000) {
      while (modem.stream.available()) { raw += (char)modem.stream.read(); t0 = millis(); }
      if (raw.indexOf("+HTTPREAD: 0") != -1) break;   // marcador de fin de datos
    }
    // El cuerpo viene tras la cabecera "+HTTPREAD: DATA,<len>" (o "+HTTPREAD: <len>").
    int hdr = raw.indexOf("+HTTPREAD:");
    if (hdr != -1) {
      int nl = raw.indexOf('\n', hdr);
      if (nl != -1) {
        int endTag = raw.indexOf("+HTTPREAD:", nl + 1);  // el "+HTTPREAD: 0" final
        resp = (endTag != -1) ? raw.substring(nl + 1, endTag) : raw.substring(nl + 1);
      }
    }
    resp.trim();
  }

  modem.sendAT("+HTTPTERM");
  modem.waitResponse(2000);
  return resp;
}

// Aplica la configuración de control que viene en la respuesta del servidor.
// Solo reacciona cuando cambia la versión, y reinicia el PID al (re)arrancar.
void applyControlConfig(const String& response) {
  int bodyIdx = response.indexOf("\r\n\r\n");
  String body = (bodyIdx == -1) ? response : response.substring(bodyIdx + 4);
  int ctrlIdx = body.indexOf("\"control\"");
  if (ctrlIdx == -1) {
    Serial.print("[CTRL] Respuesta sin 'control' (len=");
    Serial.print(response.length());
    Serial.println("). El servidor no respondio o se perdio la lectura TCP.");
    return;
  }
  String cfg = body.substring(ctrlIdx);

  long version = jsonInt(cfg, "version");
  if (version == controlCfg.version) return;   // sin cambios

  bool wasRunning = controlCfg.running;

  controlCfg.version    = version;
  controlCfg.running    = jsonBool(cfg, "running");
  controlCfg.manual     = (jsonStr(cfg, "mode") == "manual");
  controlCfg.type       = parseController(jsonStr(cfg, "ctrl_type"), controlCfg.type);
  controlCfg.setpoint_rpm = constrain(jsonFloat(cfg, "setpoint"), MIN_SETPOINT_RPM, MAX_SETPOINT_RPM);
  controlCfg.kp         = jsonFloat(cfg, "kp");
  controlCfg.ki         = jsonFloat(cfg, "ki");
  controlCfg.kd         = jsonFloat(cfg, "kd");
  controlCfg.pwm_manual = (uint8_t)constrain(jsonInt(cfg, "pwm_manual"), PWM_MIN, PWM_MAX);

  if (controlCfg.running && !wasRunning) resetPidState();
  if (!controlCfg.running) setMotorPwm(0);
  if (controlCfg.running && controlCfg.manual) setMotorPwm(controlCfg.pwm_manual);

  Serial.print("[CTRL] Config v");
  Serial.print(version);
  Serial.print(" run="); Serial.print(controlCfg.running);
  Serial.print(" mode="); Serial.print(controlCfg.manual ? "manual" : "PID");
  Serial.print(" sp="); Serial.print(controlCfg.setpoint_rpm);
  Serial.print(" tipo="); Serial.println(controllerName(controlCfg.type));
}

// ============================================================
// SMS
// ============================================================

namespace {
String simBuffer = "";

bool sendSMS(const String& number, const String& message) {
  while (simSerial.available()) simSerial.read();

  simSerial.print("AT+CMGS=\"");
  simSerial.print(number);
  simSerial.println("\"");

  unsigned long start = millis();
  bool promptReady = false;
  while (millis() - start < 5000) {
    if (simSerial.available() && simSerial.read() == '>') {
      promptReady = true;
      break;
    }
  }

  if (!promptReady) {
    Serial.println("[SMS] Error: el modulo no entrego prompt para enviar SMS.");
    displayState("SMS error prompt");
    return false;
  }

  simSerial.print(message);
  simSerial.write(26);

  String response = readATResponse(15000);
  Serial.println("[SMS] Respuesta envio:");
  Serial.println(response);
  return response.indexOf("OK") != -1;
}

void deleteSMS(int index) {
  simSerial.print("AT+CMGD=");
  simSerial.println(index);
  readATResponse(3000);
}

String extractSenderNumber(const String& response) {
  int cmgr = response.indexOf("+CMGR:");
  if (cmgr == -1) return "";
  int firstComma = response.indexOf(',', cmgr);
  if (firstComma == -1) return "";
  int numberStart = response.indexOf('"', firstComma);
  if (numberStart == -1) return "";
  int numberEnd = response.indexOf('"', numberStart + 1);
  if (numberEnd == -1) return "";
  return response.substring(numberStart + 1, numberEnd);
}

String extractSMSText(const String& response) {
  int cmgr = response.indexOf("+CMGR:");
  if (cmgr == -1) return "";
  int headerEnd = response.indexOf('\n', cmgr);
  if (headerEnd == -1) return "";
  String rest = response.substring(headerEnd + 1);
  rest.replace("\r", "");
  int ok = rest.indexOf("\nOK");
  if (ok != -1) rest = rest.substring(0, ok);
  rest.trim();
  return rest;
}

bool isHexadecimal(char c) {
  return isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

String decodeUCS2IfNeeded(String text) {
  text.trim();
  if (text.length() < 4 || text.length() % 4 != 0) return text;
  for (int i = 0; i < (int)text.length(); i++) {
    if (!isHexadecimal(text[i])) return text;
  }
  String decoded = "";
  for (int i = 0; i < (int)text.length(); i += 4) {
    int highByte = hexValue(text[i]) * 16 + hexValue(text[i + 1]);
    int lowByte  = hexValue(text[i + 2]) * 16 + hexValue(text[i + 3]);
    if (highByte == 0 && lowByte >= 32 && lowByte <= 126) {
      decoded += char(lowByte);
    } else {
      return text;
    }
  }
  return decoded;
}

String normalizeCommandText(String text) {
  text = decodeUCS2IfNeeded(text);
  text.trim();
  if (text.length() >= 2 && text[0] == '"' && text[text.length() - 1] == '"') {
    text = text.substring(1, text.length() - 1);
    text.trim();
  }
  text.toLowerCase();

  String normalized = "";
  bool pendingSpace = false;
  for (int i = 0; i < (int)text.length(); i++) {
    unsigned char c = text[i];
    if (c <= 32 || c == 160) {
      pendingSpace = normalized.length() > 0;
      continue;
    }
    if (pendingSpace) {
      normalized += ' ';
      pendingSpace = false;
    }
    normalized += char(c);
  }
  normalized.trim();
  return normalized;
}

void handleSMSCommand(int index) {
  Serial.print("[SMS] Leyendo mensaje en indice ");
  Serial.println(index);
  displayState("SMS recibido");

  while (simSerial.available()) simSerial.read();
  simSerial.print("AT+CMGR=");
  simSerial.println(index);

  String response = readATResponse(5000);
  Serial.println("[SMS] Contenido recibido:");
  Serial.println(response);

  String number  = extractSenderNumber(response);
  String text    = extractSMSText(response);

  if (number.length() == 0 || text.length() == 0) {
    Serial.println("[SMS] No se pudo leer numero o texto del mensaje.");
    displayState("SMS invalido");
    deleteSMS(index);
    return;
  }

  String command = normalizeCommandText(text);
  Serial.print("[SMS] Texto normalizado: '");
  Serial.print(command);
  Serial.println("'");

  int separator   = command.indexOf(' ');
  String commandWord = separator == -1 ? command : command.substring(0, separator);

  if (commandWord != "pwm") {
    sendSMS(number, "Comando no reconocido. Usa: pwm valor");
    displayState("SMS comando invalido");
    deleteSMS(index);
    return;
  }

  String valueText = separator == -1 ? "" : command.substring(separator + 1);
  valueText.trim();

  if (valueText.length() == 0) {
    sendSMS(number, "Valor PWM invalido. Usa un numero entre 0 y 255.");
    displayState("PWM invalido");
    deleteSMS(index);
    return;
  }

  for (int i = 0; i < (int)valueText.length(); i++) {
    if (!isDigit(valueText[i])) {
      sendSMS(number, "Valor PWM invalido. Usa un numero entre 0 y 255.");
      displayState("PWM invalido");
      deleteSMS(index);
      return;
    }
  }

  int newPwm = valueText.toInt();
  if (newPwm < PWM_MIN || newPwm > PWM_MAX) {
    sendSMS(number, "Valor PWM fuera de rango. Usa un numero entre 0 y 255.");
    displayState("PWM fuera rango");
    deleteSMS(index);
    return;
  }

  setMotorPwm((uint8_t)newPwm);
  Serial.print("[SMS] PWM actualizado a ");
  Serial.println(getMotorPwm());

  displayState("PWM SMS: " + String(getMotorPwm()));
  sendSMS(number, "PWM actualizado a " + String(getMotorPwm()));
  deleteSMS(index);
}
}

void beginSMS() {
  displayState("Config SMS");
  simSerial.println("AT+CSCS=\"GSM\"");
  delay(100);
  simSerial.println("AT+CMGF=1");
  delay(100);
  simSerial.println("AT+CPMS=\"SM\",\"SM\",\"SM\"");
  delay(100);
  simSerial.println("AT+CNMI=2,1,0,0,0");
  delay(100);
}

void processSMS() {
  while (simSerial.available()) {
    char c = simSerial.read();
    if (c == '\n') {
      simBuffer.trim();
      if (simBuffer.startsWith("+CMTI:")) {
        int comma = simBuffer.lastIndexOf(',');
        if (comma != -1) {
          int index = simBuffer.substring(comma + 1).toInt();
          if (index > 0) handleSMSCommand(index);
        }
      }
      simBuffer = "";
    } else if (c != '\r') {
      simBuffer += c;
    }
  }
}

void pollUnreadSMS() {
  while (simSerial.available()) simSerial.read();
  simSerial.println("AT+CMGL=\"REC UNREAD\"");

  String response = readATResponse(5000);
  if (response.indexOf("+CMGL:") == -1) {
    if (response.indexOf("ERROR") != -1) {
      Serial.println("[SMS] Error consultando mensajes no leidos:");
      Serial.println(response);
      displayState("Error consulta SMS");
    }
    return;
  }

  Serial.println("[SMS] Mensajes no leidos encontrados:");
  Serial.println(response);
  displayState("SMS no leidos");

  int pos = 0;
  while (true) {
    int cmgl = response.indexOf("+CMGL:", pos);
    if (cmgl == -1) break;

    int indexStart = cmgl + 6;
    while (indexStart < (int)response.length() && response[indexStart] == ' ') indexStart++;

    int indexEnd = response.indexOf(',', indexStart);
    if (indexEnd == -1) break;

    int index = response.substring(indexStart, indexEnd).toInt();
    if (index > 0) handleSMSCommand(index);

    pos = indexEnd + 1;
  }
}

// ============================================================
// MAIN
// ============================================================

namespace {
Measurements measurements;
LteInfo lteInfo;
uint32_t previousControl = 0;
uint32_t previousSend = 0;
uint32_t previousSmsPoll = 0;
uint32_t previousLtePoll = 0;
uint32_t previousPingPoll = 0;
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Wire.begin(SDA_PIN, SCL_PIN);
  beginDisplay();

  beginModemSerial();
  delay(8000);

  connectLTE();
  beginSMS();
  beginGps();

  beginMotor();

  if (!beginSensors()) {
    logState("Error INA219");
    while (1) delay(10);
  }

  logState("Sistema listo");
  Serial.println("rpm,current_A,voltage_V,power_W,pwm,setpoint,error,controlador");

  uint32_t now = millis();
  previousControl  = now;
  previousSend     = now;
  previousSmsPoll  = now;
  previousLtePoll  = now - LTE_POLL_INTERVAL_MS;   // poll en el primer envío
  previousPingPoll = now - PING_POLL_INTERVAL_MS;
}

void loop() {
  // ── URCs de SMS entrantes (barato, cada iteración) ──
  processSMS();

  uint32_t now = millis();

  // ── Lazo de control + muestreo de RPM (rápido, local) ──
  if ((now - previousControl) >= CONTROL_INTERVAL_MS) {
    float dt = (now - previousControl) / 1000.0f;
    previousControl = now;

    measurements.rpm = sampleMotorRpm(now);

    if (controlCfg.running && !controlCfg.manual) {
      // Lazo cerrado PID + feedforward
      int pwm = computeControlPwm(measurements.rpm, dt);
      setMotorPwm((uint8_t)pwm);
    }
    // En modo manual el PWM ya quedó fijado al aplicar la config.
    // Si no está corriendo, applyControlConfig() ya puso el motor en 0.

    measurements.pwm = getMotorPwm();
    measurements.setpoint = controlCfg.setpoint_rpm;
    measurements.error = controlCfg.setpoint_rpm - measurements.rpm;
    measurements.running = controlCfg.running;
  }

  // ── Sondeo de SMS no leídos (poco frecuente) ──
  if ((now - previousSmsPoll) >= SMS_POLL_INTERVAL_MS) {
    previousSmsPoll = now;
    pollUnreadSMS();
  }

  // ── Envío de telemetría por HTTP ──
  if ((now - previousSend) >= SEND_INTERVAL_MS) {
    previousSend = now;

    readElectricalMeasurements(measurements);

    // IMPORTANTE: las consultas AT (GPS/red) y la conexión TCP comparten el
    // mismo UART del módem, así que se hacen SECUENCIALMENTE, nunca a la vez.
    if ((now - previousLtePoll) >= LTE_POLL_INTERVAL_MS) {
      previousLtePoll = now;
      pollLte(lteInfo);
    }
    if ((now - previousPingPoll) >= PING_POLL_INTERVAL_MS) {
      previousPingPoll = now;
      pollPing(lteInfo);
    }
    // Lee la posición GPS justo antes de enviar.
    readGPS(measurements);

    String resp = sendHttpMeasurements(measurements, lteInfo);
    applyControlConfig(resp);   // servidor → ESP: aplica config de control

    Serial.print(measurements.rpm, 2);     Serial.print(",");
    Serial.print(measurements.current_A, 4); Serial.print(",");
    Serial.print(measurements.voltage_V, 3); Serial.print(",");
    Serial.print(measurements.power_W, 4);   Serial.print(",");
    Serial.print((int)measurements.pwm);     Serial.print(",");
    Serial.print(measurements.setpoint, 2);  Serial.print(",");
    Serial.print(measurements.error, 2);     Serial.print(",");
    Serial.println(controllerName(controlCfg.type));

    displayMeasurements(measurements);
  }
}