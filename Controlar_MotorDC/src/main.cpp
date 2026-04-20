#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// ─── Pines ───────────────────────────────────────────────────
namespace {
  const uint8_t kEncoderPinA = 13;
  const uint8_t kEncoderPinB = 14;
  const uint8_t kIN1         = 11;
  const uint8_t kIN2         = 12;
  const uint8_t kENA         = 10;

  const uint8_t SDA_PIN      = 16;
  const uint8_t SCL_PIN      = 15;

  const uint32_t kPwmFreq       = 20000;
  const uint8_t  kPwmResolution = 8;
}

// ─── Parámetros del encoder ───────────────────────────────────
const float PPR_MOTOR  = 12.0f;
const float GEAR_RATIO = 90.0f;
const float CPR        = PPR_MOTOR * GEAR_RATIO;  // 1080 cnt/rev

// ─── Parámetros de muestreo ───────────────────────────────────
const uint16_t INTERVALO_MS = 500;

// ─── PWM manual (0-255) ───────────────────────────────────────
uint8_t g_pwm_actual = 255;  // <-- Cambia este valor según necesites

// ─── Variables globales ───────────────────────────────────────
volatile int32_t g_encoder_count = 0;
int32_t  prev_count = 0;
uint32_t prev_time  = 0;
uint32_t t_inicio   = 0;

// ─── Sensor INA219 ────────────────────────────────────────────
Adafruit_INA219 ina219;

// ─── ISR encoder (corregida según datasheet del motor) ────────
void IRAM_ATTR HandleEncoderInterrupt() {
  if (digitalRead(kEncoderPinB) == LOW) {
    ++g_encoder_count;  // Adelante
  } else {
    --g_encoder_count;  // Atrás
  }
}

// ─── Escribir PWM ─────────────────────────────────────────────
void SetMotorPwm(uint8_t pwm) {
  g_pwm_actual = pwm;
  ledcWrite(0, pwm);
}

// ─── Leer PWM desde monitor serial ───────────────────────────
void RevisarSerial() {
  if (Serial.available() > 0) {
    String entrada = Serial.readStringUntil('\n');
    entrada.trim();

    if (entrada.length() == 0) return;

    if (entrada == "0") {
      SetMotorPwm(0);
      Serial.println("Motor detenido");
      return;
    }

    int nuevo_pwm = entrada.toInt();

    if (nuevo_pwm > 0 && nuevo_pwm <= 255) {
      SetMotorPwm((uint8_t)nuevo_pwm);
      Serial.print("Nuevo PWM: ");
      Serial.println(g_pwm_actual);
    } else {
      Serial.println("Entrada no valida. Escribe un valor PWM entre 0 y 255.");
    }
  }
}

// ─── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Inicio del programa");
  Serial.println("Escribe un valor PWM (0-255) y presiona Enter.");

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
  SetMotorPwm(g_pwm_actual);

  // I2C + INA219
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!ina219.begin()) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }

  Serial.println("INA219 inicializado correctamente");

  t_inicio  = millis();
  prev_time = t_inicio;
}

// ─── Loop ─────────────────────────────────────────────────────
void loop() {
  RevisarSerial();

  uint32_t ahora = millis();
  if ((ahora - prev_time) < INTERVALO_MS) return;

  // Lectura encoder
  noInterrupts();
  int32_t count = g_encoder_count;
  interrupts();

  float dt      = (ahora - prev_time) / 1000.0f;
  int32_t delta = count - prev_count;
  float rpm     = (delta / CPR) / dt * 60.0f;
  uint32_t t_rel = ahora - t_inicio;

  // Lectura INA219
  float shuntvoltage_mV = ina219.getShuntVoltage_mV();
  float busvoltage_V    = ina219.getBusVoltage_V();
  float current_mA      = ina219.getCurrent_mA();
  float power_mW        = ina219.getPower_mW();
  float loadvoltage_V   = busvoltage_V + (shuntvoltage_mV / 1000.0f);

  // Salida
  Serial.print("t="); Serial.print(t_rel);
  Serial.print("ms | PWM="); Serial.print(g_pwm_actual);
  Serial.print(" | RPM="); Serial.print(rpm, 2);
  Serial.print(" | BV="); Serial.print(busvoltage_V, 3);
  Serial.print("V | I="); Serial.print(current_mA, 3);
  Serial.print("mA | P="); Serial.print(power_mW, 3);
  Serial.println("mW");

  prev_count = count;
  prev_time  = ahora;
}