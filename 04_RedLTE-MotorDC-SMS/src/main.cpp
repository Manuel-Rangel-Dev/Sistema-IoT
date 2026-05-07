#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

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
}

// ─── Parámetros encoder ──────────────────────────────────────
const float PPR_MOTOR  = 34.02f;
const float GEAR_RATIO = 12.0f;
const float CPR        = PPR_MOTOR * GEAR_RATIO;

// ─── Parámetros muestreo ─────────────────────────────────────
const uint16_t INTERVALO_MS  = 5000;  // enviar al servidor cada 5s
const uint16_t INTERVALO_ENV = INTERVALO_MS;  

// ─── Variables globales ──────────────────────────────────────
volatile int32_t g_encoder_count = 0;
int32_t  prev_count   = 0;
uint32_t prev_time    = 0;
uint32_t prev_env     = 0;
uint32_t t_inicio     = 0;
uint8_t  g_pwm_actual = 0;
const int PWM_MIN     = 0;
const int PWM_MAX     = 255;
String bufferSIM      = "";
uint32_t prev_sms_poll = 0;

// Últimos valores medidos
float g_rpm        = 0;
float g_current_A  = 0;
float g_voltage_V  = 0;
float g_power_W    = 0;

// ─── Objetos ─────────────────────────────────────────────────
HardwareSerial simSerial(1);
TinyGsm        modem(simSerial);
TinyGsmClient client(modem);
Adafruit_INA219 ina219;

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

// ─── Enviar AT ───────────────────────────────────────────────
void sendAT(const char* cmd, int waitMs = 2000) {
  while (simSerial.available()) simSerial.read();
  simSerial.println(cmd);
  Serial.print(">> ");
  Serial.println(cmd);
  long t = millis();
  while (millis() - t < waitMs) {
    while (simSerial.available()) {
      Serial.write(simSerial.read());
    }
  }
  Serial.println();
}

String leerRespuestaAT(unsigned long timeoutMs = 3000) {
  String response = "";
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (simSerial.available()) {
      char c = simSerial.read();
      response += c;
    }

    if (response.indexOf("\r\nOK\r\n") != -1 || response.indexOf("\r\nERROR\r\n") != -1) {
      break;
    }
  }

  return response;
}

bool enviarSMS(const String& numero, const String& mensaje) {
  while (simSerial.available()) simSerial.read();

  simSerial.print("AT+CMGS=\"");
  simSerial.print(numero);
  simSerial.println("\"");

  unsigned long start = millis();
  bool promptListo = false;
  while (millis() - start < 5000) {
    if (simSerial.available() && simSerial.read() == '>') {
      promptListo = true;
      break;
    }
  }

  if (!promptListo) {
    Serial.println("[SMS] Error: el modulo no entrego prompt para enviar SMS.");
    return false;
  }

  simSerial.print(mensaje);
  simSerial.write(26);

  String response = leerRespuestaAT(15000);
  Serial.println("[SMS] Respuesta envio:");
  Serial.println(response);

  return response.indexOf("OK") != -1;
}

void borrarSMS(int index) {
  simSerial.print("AT+CMGD=");
  simSerial.println(index);
  leerRespuestaAT(3000);
}

String extraerNumeroRemitente(const String& response) {
  int cmgr = response.indexOf("+CMGR:");
  if (cmgr == -1) return "";

  int primeraComa = response.indexOf(',', cmgr);
  if (primeraComa == -1) return "";

  int inicioNumero = response.indexOf('"', primeraComa);
  if (inicioNumero == -1) return "";

  int finNumero = response.indexOf('"', inicioNumero + 1);
  if (finNumero == -1) return "";

  return response.substring(inicioNumero + 1, finNumero);
}

String extraerTextoSMS(const String& response) {
  int cmgr = response.indexOf("+CMGR:");
  if (cmgr == -1) return "";

  int finCabecera = response.indexOf('\n', cmgr);
  if (finCabecera == -1) return "";

  String resto = response.substring(finCabecera + 1);
  resto.replace("\r", "");

  int ok = resto.indexOf("\nOK");
  if (ok != -1) {
    resto = resto.substring(0, ok);
  }

  resto.trim();
  return resto;
}

bool esHexadecimal(char c) {
  return isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int valorHex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

String decodificarUCS2SiAplica(String texto) {
  texto.trim();

  if (texto.length() < 4 || texto.length() % 4 != 0) {
    return texto;
  }

  for (int i = 0; i < texto.length(); i++) {
    if (!esHexadecimal(texto[i])) {
      return texto;
    }
  }

  String decodificado = "";
  for (int i = 0; i < texto.length(); i += 4) {
    int highByte = valorHex(texto[i]) * 16 + valorHex(texto[i + 1]);
    int lowByte = valorHex(texto[i + 2]) * 16 + valorHex(texto[i + 3]);

    if (highByte == 0 && lowByte >= 32 && lowByte <= 126) {
      decodificado += char(lowByte);
    } else {
      return texto;
    }
  }

  return decodificado;
}

String normalizarTextoComando(String texto) {
  texto = decodificarUCS2SiAplica(texto);
  texto.trim();

  if (texto.length() >= 2 && texto[0] == '"' && texto[texto.length() - 1] == '"') {
    texto = texto.substring(1, texto.length() - 1);
    texto.trim();
  }

  texto.toLowerCase();

  String normalizado = "";
  bool espacioPendiente = false;

  for (int i = 0; i < texto.length(); i++) {
    unsigned char c = texto[i];

    if (c <= 32 || c == 160) {
      espacioPendiente = normalizado.length() > 0;
      continue;
    }

    if (espacioPendiente) {
      normalizado += ' ';
      espacioPendiente = false;
    }

    normalizado += char(c);
  }

  normalizado.trim();
  return normalizado;
}

// ─── Sincronizar módulo ───────────────────────────────────────
void syncSIM() {
  Serial.println("Sincronizando con SIM7670G...");
  for (int i = 0; i < 5; i++) {
    simSerial.println("AT");
    delay(500);
    if (simSerial.find("OK")) {
      Serial.println("Módulo respondiendo.");
      return;
    }
  }
  Serial.println("Advertencia: módulo no responde aún.");
}

// ─── Conexión LTE ────────────────────────────────────────────
void conectarLTE() {
  Serial.println("\n=== Iniciando conexión LTE (Claro Colombia) ===");

  syncSIM();
  sendAT("ATE0");           // desactivar eco
  sendAT("AT+CPIN?");       // verificar SIM

  Serial.println("Esperando registro en red...");
  modem.waitForNetwork(60000UL);

  if (modem.isNetworkConnected()) {
    Serial.println("Red registrada correctamente.");
  } else {
    Serial.println("Advertencia: no se registró en red.");
  }

  sendAT("AT+CSQ");         // calidad de señal
  sendAT("AT+CREG?");       // estado registro
  sendAT("AT+CEREG?");      // estado LTE

  Serial.println("Conectando a APN Claro...");
  modem.gprsConnect("internet.comcel.com.co", "", "");

  if (modem.isGprsConnected()) {
    Serial.println("Conexión LTE exitosa.");
    Serial.print("IP asignada: ");
    Serial.println(modem.getLocalIP());

    Serial.println("Verificando conexión con ping a Google DNS...");
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
      Serial.println("\n[SISTEMA] ✓ Datos activos y navegación confirmada.");
    } else {
      Serial.println("\n[SISTEMA] ⚠ Error: IP obtenida pero sin respuesta de Ping.");
    }

  } else {
    Serial.println("Error: no se pudo conectar al APN.");
  }

  Serial.println("=== Fin configuración LTE ===\n");
}

// ─── Enviar datos al servidor ─────────────────────────────────
void gestionarComandosSMS(int index) {
  Serial.print("[SMS] Leyendo mensaje en indice ");
  Serial.println(index);

  while (simSerial.available()) simSerial.read();
  simSerial.print("AT+CMGR=");
  simSerial.println(index);

  String response = leerRespuestaAT(5000);
  Serial.println("[SMS] Contenido recibido:");
  Serial.println(response);

  String numero = extraerNumeroRemitente(response);
  String texto = extraerTextoSMS(response);

  if (numero.length() == 0 || texto.length() == 0) {
    Serial.println("[SMS] No se pudo leer numero o texto del mensaje.");
    borrarSMS(index);
    return;
  }

  String comando = normalizarTextoComando(texto);

  Serial.print("[SMS] Texto normalizado: '");
  Serial.print(comando);
  Serial.println("'");

  int separador = comando.indexOf(' ');
  String palabraComando = separador == -1 ? comando : comando.substring(0, separador);

  if (palabraComando != "pwm") {
    enviarSMS(numero, "Comando no reconocido. Usa: pwm valor");
    borrarSMS(index);
    return;
  }

  String valorTexto = separador == -1 ? "" : comando.substring(separador + 1);
  valorTexto.trim();

  if (valorTexto.length() == 0) {
    enviarSMS(numero, "Valor PWM invalido. Usa un numero entre 0 y 255.");
    borrarSMS(index);
    return;
  }

  for (int i = 0; i < valorTexto.length(); i++) {
    if (!isDigit(valorTexto[i])) {
      enviarSMS(numero, "Valor PWM invalido. Usa un numero entre 0 y 255.");
      borrarSMS(index);
      return;
    }
  }

  int nuevoPWM = valorTexto.toInt();
  if (nuevoPWM < PWM_MIN || nuevoPWM > PWM_MAX) {
    enviarSMS(numero, "Valor PWM fuera de rango. Usa un numero entre 0 y 255.");
    borrarSMS(index);
    return;
  }

  SetMotorPwm((uint8_t)nuevoPWM);
  Serial.print("[SMS] PWM actualizado a ");
  Serial.println(g_pwm_actual);

  enviarSMS(numero, "PWM actualizado a " + String(g_pwm_actual));
  borrarSMS(index);
}

void ProcesarSMS() {
  while (simSerial.available()) {
    char c = simSerial.read();

    if (c == '\n') {
      bufferSIM.trim();

      if (bufferSIM.startsWith("+CMTI:")) {
        int coma = bufferSIM.lastIndexOf(',');
        if (coma != -1) {
          int index = bufferSIM.substring(coma + 1).toInt();
          if (index > 0) {
            gestionarComandosSMS(index);
          }
        }
      }

      bufferSIM = "";
    } else if (c != '\r') {
      bufferSIM += c;
    }
  }
}

void ConsultarSMSNoLeidos() {
  while (simSerial.available()) simSerial.read();
  simSerial.println("AT+CMGL=\"REC UNREAD\"");

  String response = leerRespuestaAT(5000);
  if (response.indexOf("+CMGL:") == -1) {
    if (response.indexOf("ERROR") != -1) {
      Serial.println("[SMS] Error consultando mensajes no leidos:");
      Serial.println(response);
    }
    return;
  }

  Serial.println("[SMS] Mensajes no leidos encontrados:");
  Serial.println(response);

  int pos = 0;
  while (true) {
    int cmgl = response.indexOf("+CMGL:", pos);
    if (cmgl == -1) break;

    int inicioIndice = cmgl + 6;
    while (inicioIndice < response.length() && response[inicioIndice] == ' ') {
      inicioIndice++;
    }

    int finIndice = response.indexOf(',', inicioIndice);
    if (finIndice == -1) break;

    int index = response.substring(inicioIndice, finIndice).toInt();
    if (index > 0) {
      gestionarComandosSMS(index);
    }

    pos = finIndice + 1;
  }
}

void enviarDatos(float rpm, float current_A, float voltage_V, float power_W, int pwm) {
  Serial.println("[HTTP] Conectando al servidor...");

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
    Serial.println("[HTTP] ✓ Datos enviados correctamente.");
  } else {
    Serial.println("[HTTP] ✗ Error al conectar con el servidor.");
  }
}

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(3000);

  // SIM7670G
  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW);
  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  delay(8000);

  conectarLTE();

  // SMS
  simSerial.println("AT+CSCS=\"GSM\"");
  delay(100);
  simSerial.println("AT+CMGF=1");
  delay(100);
  simSerial.println("AT+CPMS=\"SM\",\"SM\",\"SM\"");
  delay(100);
  simSerial.println("AT+CNMI=2,1,0,0,0");
  delay(100);

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
  SetMotorPwm(0);

  // Potenciómetro
  // I2C + INA219
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!ina219.begin()) {
    Serial.println("Error: INA219 no encontrado.");
    while (1) { delay(10); }
  }

  Serial.println("rpm,current_A,voltage_V,power_W,pwm");

  t_inicio  = millis();
  prev_time = t_inicio;
  prev_env  = t_inicio;
}

// ─── Loop ────────────────────────────────────────────────────
void loop() {
  ProcesarSMS();

  uint32_t ahora = millis();
  if ((ahora - prev_sms_poll) >= 2000) {
    prev_sms_poll = ahora;
    ConsultarSMSNoLeidos();
  }

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
  g_current_A           = ina219.getCurrent_mA() / 1000.0f;
  g_power_W             = ina219.getPower_mW()   / 1000.0f;
  g_voltage_V           = busvoltage_V + (shuntvoltage_mV / 1000.0f);

  // Monitor serial
  Serial.print(g_rpm, 2);           Serial.print(",");
  Serial.print(g_current_A, 4);     Serial.print(",");
  Serial.print(g_voltage_V, 3);     Serial.print(",");
  Serial.print(g_power_W, 4);       Serial.print(",");
  Serial.println((int)g_pwm_actual, 0);

  prev_count = count;
  prev_time  = ahora;

  // Enviar al servidor cada INTERVALO_ENV ms
  if ((ahora - prev_env) >= INTERVALO_ENV) {
  sendAT("AT+CSQ");  // calidad de señal antes de enviar datos
  enviarDatos(g_rpm, g_current_A, g_voltage_V, g_power_W, g_pwm_actual);
  prev_env = ahora;
  }
}
