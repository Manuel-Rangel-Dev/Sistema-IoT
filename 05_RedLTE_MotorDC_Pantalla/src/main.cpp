#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

// ─── OLED ────────────────────────────────────────────────────
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledOK = false;

// ─── Pines SIM7670G ──────────────────────────────────────────
#define SIM_RX  17
#define SIM_TX  18
#define SIM_DTR 45

// ─── Servidor ────────────────────────────────────────────────
#define SERVER_HOST "18.219.119.53"
#define SERVER_PORT 5000

// ─── Pines Motor ─────────────────────────────────────────────
namespace {
  const uint8_t kEncoderPinA = 13;
  const uint8_t kEncoderPinB = 14;
  const uint8_t kIN1         = 11;
  const uint8_t kIN2         = 12;
  const uint8_t kENA         = 10;
  const uint8_t SDA_PIN      = 16;
  const uint8_t SCL_PIN      = 15;

  const uint32_t kPwmFreq       = 1000;
  const uint8_t  kPwmResolution = 8;
  const uint8_t  kPwmPrueba     = 120;
  const uint16_t kAtOledHoldMs  = 2500;
}

// ─── Parámetros encoder ──────────────────────────────────────
const float PPR_MOTOR  = 34.02f;
const float GEAR_RATIO = 12.0f;
const float CPR        = PPR_MOTOR * GEAR_RATIO;

// ─── Parámetros muestreo ─────────────────────────────────────
const uint16_t INTERVALO_MS  = 5000;
const uint16_t INTERVALO_ENV = INTERVALO_MS;

// ─── Variables globales ──────────────────────────────────────
volatile int32_t g_encoder_count = 0;
int32_t  prev_count   = 0;
uint32_t prev_time    = 0;
uint32_t prev_env     = 0;
uint32_t t_inicio     = 0;
uint8_t  g_pwm_actual = 0;

float g_rpm        = 0;
float g_current_A  = 0;
float g_voltage_V  = 0;
float g_power_W    = 0;

// ─── Objetos ─────────────────────────────────────────────────
HardwareSerial simSerial(1);
TinyGsm        modem(simSerial);
TinyGsmClient  client(modem);
Adafruit_INA219 ina219;

// ─── OLED funciones ──────────────────────────────────────────
void mostrarEstado(const String& mensaje) {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("Estado:");
  display.println(mensaje);

  display.display();
}

void logEstado(const String& mensaje) {
  Serial.println(mensaje);
  mostrarEstado(mensaje);
}

void mostrarMedidas() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.print("RPM: ");
  display.println(g_rpm, 1);

  display.print("I:");
  display.print(g_current_A, 3);
  display.print("A V:");
  display.println(g_voltage_V, 2);

  display.print("P:");
  display.print(g_power_W, 3);
  display.print("W PWM:");
  display.println(g_pwm_actual);

  display.display();
}

// ─── ISR encoder ─────────────────────────────────────────────
void IRAM_ATTR HandleEncoderInterrupt() {
  if (digitalRead(kEncoderPinB) == LOW) {
    ++g_encoder_count;
  } else {
    --g_encoder_count;
  }
}

// ─── PWM ─────────────────────────────────────────────────────
void SetMotorPwm(uint8_t pwm) {
  g_pwm_actual = pwm;
  ledcWrite(0, pwm);
}

// ─── Potenciómetro ───────────────────────────────────────────
void ActualizarPwmPrueba() {
  if (g_pwm_actual != kPwmPrueba) {
    SetMotorPwm(kPwmPrueba);
  }
}

String limpiarRespuestaAT(const String& cmd, String response) {
  response.replace("\r", "\n");
  response.replace(cmd, "");

  String primeraLinea = "";
  String estadoFinal = "";

  int start = 0;
  while (start < response.length()) {
    int end = response.indexOf('\n', start);
    if (end == -1) end = response.length();

    String line = response.substring(start, end);
    line.trim();

    if (line.length() > 0) {
      if (line.startsWith("+")) {
        return line;
      }

      if (line == "ERROR") {
        return line;
      }

      if (line == "OK") {
        estadoFinal = line;
      }

      if (primeraLinea.length() == 0) {
        primeraLinea = line;
      }
    }

    start = end + 1;
  }

  if (primeraLinea.length() > 0) {
    return primeraLinea;
  }

  if (estadoFinal.length() > 0) {
    return estadoFinal;
  }

  return "Sin respuesta";
}

// ─── Enviar AT ───────────────────────────────────────────────
String etiquetaComandoAT(const String& cmd) {
  String etiqueta = cmd;

  if (etiqueta.startsWith("AT+")) {
    etiqueta.remove(0, 3);
  } else if (etiqueta.startsWith("AT")) {
    etiqueta.remove(0, 2);
  }

  etiqueta.replace("?", "");

  if (etiqueta.length() == 0) {
    return cmd;
  }

  return etiqueta;
}

void sendAT(const char* cmd, int waitMs = 2000) {
  while (simSerial.available()) simSerial.read();

  simSerial.println(cmd);

  Serial.print(">> ");
  Serial.println(cmd);

  long t = millis();
  String response = "";
  while (millis() - t < waitMs) {
    while (simSerial.available()) {
      char c = simSerial.read();
      response += c;
      Serial.write(c);
    }
  }

  Serial.println();

  String msg = etiquetaComandoAT(cmd) + ": ";
  msg += limpiarRespuestaAT(cmd, response);
  mostrarEstado(msg);
  delay(kAtOledHoldMs);
}

// ─── Sincronizar módulo ───────────────────────────────────────
void syncSIM() {
  logEstado("Sincronizando SIM7670G");

  for (int i = 0; i < 5; i++) {
    simSerial.println("AT");
    delay(500);

    if (simSerial.find("OK")) {
      logEstado("Modulo respondiendo");
      return;
    }
  }

  logEstado("Modulo no responde");
}

// ─── Conexión LTE ────────────────────────────────────────────
void conectarLTE() {
  logEstado("Iniciando LTE Claro");

  syncSIM();

  sendAT("ATE0");
  sendAT("AT+CPIN?");

  logEstado("Esperando red...");
  modem.waitForNetwork(60000UL);

  if (modem.isNetworkConnected()) {
    logEstado("Red registrada");
  } else {
    logEstado("Error registro red");
  }

  sendAT("AT+CSQ");
  sendAT("AT+CREG?");
  sendAT("AT+CEREG?");

  logEstado("Conectando APN...");
  modem.gprsConnect("internet.comcel.com.co", "", "");

  if (modem.isGprsConnected()) {
    logEstado("LTE conectado");

    Serial.print("IP asignada: ");
    Serial.println(modem.getLocalIP());

    mostrarEstado("IP asignada");

    logEstado("Verificando ping...");

    while (simSerial.available()) simSerial.read();
    simSerial.println("AT+CPING=\"8.8.8.8\",1,2");

    unsigned long start = millis();
    String response = "";
    bool pingExitoso = false;

    while (millis() - start < 6000) {
      while (simSerial.available()) {
        char c = simSerial.read();
        response += c;
        Serial.write(c);
      }

      if (response.indexOf("+CPING: 3") != -1) {
        if (response.indexOf(",0,2") == -1 && response.indexOf(",0,1") == -1) {
          pingExitoso = true;
        }
        break;
      }
    }

    if (pingExitoso) {
      logEstado("Datos activos OK");
    } else {
      logEstado("Error ping");
    }

  } else {
    logEstado("Error APN");
  }

  logEstado("Fin config LTE");
}

// ─── Enviar datos al servidor ─────────────────────────────────
void enviarDatos(float rpm, float current_A, float voltage_V, float power_W, int pwm) {
  logEstado("HTTP conectando...");

  if (client.connect(SERVER_HOST, SERVER_PORT)) {
    String body = "{\"rpm\":"       + String(rpm, 2) +
                  ",\"current_A\":" + String(current_A, 4) +
                  ",\"voltage_V\":" + String(voltage_V, 3) +
                  ",\"power_W\":"   + String(power_W, 4) +
                  ",\"pwm\":"       + String(pwm) + "}";

    client.println("POST /data HTTP/1.1");
    client.print("Host: ");
    client.println(SERVER_HOST);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(body.length());
    client.println("Connection: close");
    client.println();
    client.println(body);

    delay(500);
    client.stop();

    logEstado("Datos enviados OK");
  } else {
    logEstado("Error servidor");
  }
}

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(3000);

  // I2C primero porque OLED e INA219 usan el mismo bus
  Wire.begin(SDA_PIN, SCL_PIN);

  // OLED
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oledOK = true;
    mostrarEstado("Inicio sistema");
  } else {
    Serial.println("Advertencia: OLED no encontrada.");
  }

  // SIM7670G
  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW);
  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  delay(8000);

  conectarLTE();

  // Encoder
  pinMode(kEncoderPinA, INPUT_PULLUP);
  pinMode(kEncoderPinB, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(kEncoderPinA), HandleEncoderInterrupt, RISING);

  // Puente H
  pinMode(kIN1, OUTPUT);
  pinMode(kIN2, OUTPUT);
  digitalWrite(kIN1, HIGH);
  digitalWrite(kIN2, LOW);

  // PWM
  ledcSetup(0, kPwmFreq, kPwmResolution);
  ledcAttachPin(kENA, 0);
  SetMotorPwm(kPwmPrueba);

  // Potenciómetro
  // INA219
  if (!ina219.begin()) {
    logEstado("Error INA219");
    while (1) {
      delay(10);
    }
  }

  logEstado("Sistema listo");

  Serial.println("rpm,current_A,voltage_V,power_W,pwm");

  t_inicio  = millis();
  prev_time = t_inicio;
  prev_env  = t_inicio;
}

// ─── Loop ────────────────────────────────────────────────────
void loop() {
  ActualizarPwmPrueba();

  uint32_t ahora = millis();
  if ((ahora - prev_time) < INTERVALO_MS) return;

  // Encoder
  noInterrupts();
  int32_t count = g_encoder_count;
  interrupts();

  float dt      = (ahora - prev_time) / 1000.0f;
  int32_t delta = count - prev_count;
  g_rpm         = -((delta / CPR) / dt * 60.0f);

  // INA219
  float shuntvoltage_mV = ina219.getShuntVoltage_mV();
  float busvoltage_V    = ina219.getBusVoltage_V();

  g_current_A = ina219.getCurrent_mA() / 1000.0f;
  g_power_W   = ina219.getPower_mW()   / 1000.0f;
  g_voltage_V = busvoltage_V + (shuntvoltage_mV / 1000.0f);

  // Monitor serial
  Serial.print(g_rpm, 2);        Serial.print(",");
  Serial.print(g_current_A, 4);  Serial.print(",");
  Serial.print(g_voltage_V, 3);  Serial.print(",");
  Serial.print(g_power_W, 4);    Serial.print(",");
  Serial.println((int)g_pwm_actual);

  // OLED con mediciones
  mostrarMedidas();

  prev_count = count;
  prev_time  = ahora;

  // Enviar al servidor cada INTERVALO_ENV ms
  if ((ahora - prev_env) >= INTERVALO_ENV) {
    sendAT("AT+CSQ");
    enviarDatos(g_rpm, g_current_A, g_voltage_V, g_power_W, g_pwm_actual);
    prev_env = ahora;
  }
}
