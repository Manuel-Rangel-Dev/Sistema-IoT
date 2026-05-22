#include "sensors.h"

#include <Adafruit_INA219.h>

namespace {
Adafruit_INA219 ina219;
}

bool beginSensors() {
  return ina219.begin();
}

void readElectricalMeasurements(Measurements& measurements) {
  float shuntVoltage_mV = ina219.getShuntVoltage_mV();
  float busVoltage_V = ina219.getBusVoltage_V();

  measurements.current_A = ina219.getCurrent_mA() / 1000.0f;
  measurements.power_W = ina219.getPower_mW() / 1000.0f;
  measurements.voltage_V = busVoltage_V + (shuntVoltage_mV / 1000.0f);
}
