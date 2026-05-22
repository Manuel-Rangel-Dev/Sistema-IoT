#pragma once

#include <Arduino.h>

constexpr uint8_t SIM_RX = 17;
constexpr uint8_t SIM_TX = 18;
constexpr uint8_t SIM_DTR = 45;

constexpr char SERVER_HOST[] = "18.219.119.53";
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

constexpr uint32_t PWM_FREQ = 1000;
constexpr uint8_t PWM_RESOLUTION = 8;
constexpr int PWM_MIN = 0;
constexpr int PWM_MAX = 255;

constexpr float PPR_MOTOR = 34.02f;
constexpr float GEAR_RATIO = 12.0f;
constexpr float CPR = PPR_MOTOR * GEAR_RATIO;

constexpr uint16_t SAMPLE_INTERVAL_MS = 5000;
constexpr uint16_t SEND_INTERVAL_MS = SAMPLE_INTERVAL_MS;
constexpr uint16_t SMS_POLL_INTERVAL_MS = 2000;
constexpr uint16_t AT_OLED_HOLD_MS = 2500;
