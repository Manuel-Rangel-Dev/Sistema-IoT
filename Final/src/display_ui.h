#pragma once

#include <Arduino.h>
#include "telemetry.h"

bool beginDisplay();
bool isDisplayReady();
void displayState(const String& message);
void logState(const String& message);
void displayMeasurements(const Measurements& measurements);
