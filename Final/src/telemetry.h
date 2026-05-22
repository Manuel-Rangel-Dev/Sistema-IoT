#pragma once

#include <Arduino.h>

struct Measurements {
  float rpm = 0.0f;
  float current_A = 0.0f;
  float voltage_V = 0.0f;
  float power_W = 0.0f;
  uint8_t pwm = 0;
};
