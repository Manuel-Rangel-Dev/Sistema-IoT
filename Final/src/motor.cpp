#include "motor.h"

#include "config.h"

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

  ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(MOTOR_ENA, 0);
  setMotorPwm(0);

  previousTime = millis();
}

void setMotorPwm(uint8_t pwm) {
  currentPwm = pwm;
  ledcWrite(0, pwm);
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
