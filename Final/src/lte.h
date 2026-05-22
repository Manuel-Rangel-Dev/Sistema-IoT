#pragma once

#include <Arduino.h>
#include <HardwareSerial.h>
#include "telemetry.h"

HardwareSerial& simSerialPort();
void beginModemSerial();
void connectLTE();
void sendAT(const char* cmd, int waitMs = 2000);
String readATResponse(unsigned long timeoutMs = 3000);
bool sendHttpMeasurements(const Measurements& measurements);
