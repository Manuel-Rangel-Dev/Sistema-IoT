#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "display_ui.h"
#include "lte.h"
#include "motor.h"
#include "sensors.h"
#include "sms.h"
#include "telemetry.h"

namespace {
Measurements measurements;
uint32_t previousSample = 0;
uint32_t previousSend = 0;
uint32_t previousSmsPoll = 0;
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Wire.begin(SDA_PIN, SCL_PIN);
  beginDisplay();

  beginModemSerial();
  delay(8000);

  connectLTE();
  beginSMS();

  beginMotor();

  if (!beginSensors()) {
    logState("Error INA219");
    while (1) {
      delay(10);
    }
  }

  logState("Sistema listo");
  Serial.println("rpm,current_A,voltage_V,power_W,pwm");

  uint32_t now = millis();
  previousSample = now;
  previousSend = now;
  previousSmsPoll = now;
}

void loop() {
  processSMS();

  uint32_t now = millis();
  if ((now - previousSmsPoll) >= SMS_POLL_INTERVAL_MS) {
    previousSmsPoll = now;
    pollUnreadSMS();
  }

  if ((now - previousSample) < SAMPLE_INTERVAL_MS) {
    return;
  }

  measurements.rpm = sampleMotorRpm(now);
  readElectricalMeasurements(measurements);
  measurements.pwm = getMotorPwm();

  Serial.print(measurements.rpm, 2);
  Serial.print(",");
  Serial.print(measurements.current_A, 4);
  Serial.print(",");
  Serial.print(measurements.voltage_V, 3);
  Serial.print(",");
  Serial.print(measurements.power_W, 4);
  Serial.print(",");
  Serial.println((int)measurements.pwm);

  displayMeasurements(measurements);
  previousSample = now;

  if ((now - previousSend) >= SEND_INTERVAL_MS) {
    sendAT("AT+CSQ");
    sendHttpMeasurements(measurements);
    previousSend = now;
  }
}
