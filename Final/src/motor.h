#pragma once

#include <Arduino.h>

void beginMotor();
void setMotorPwm(uint8_t pwm);
uint8_t getMotorPwm();
float sampleMotorRpm(uint32_t nowMs);
