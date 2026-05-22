#include "display_ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

namespace {
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayReady = false;
}

bool beginDisplay() {
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);

  if (displayReady) {
    displayState("Inicio sistema");
  } else {
    Serial.println("Advertencia: OLED no encontrada.");
  }

  return displayReady;
}

bool isDisplayReady() {
  return displayReady;
}

void displayState(const String& message) {
  if (!displayReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Estado:");
  display.println(message);
  display.display();
}

void logState(const String& message) {
  Serial.println(message);
  displayState(message);
}

void displayMeasurements(const Measurements& measurements) {
  if (!displayReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.print("RPM: ");
  display.println(measurements.rpm, 1);

  display.print("I:");
  display.print(measurements.current_A, 3);
  display.print("A V:");
  display.println(measurements.voltage_V, 2);

  display.print("P:");
  display.print(measurements.power_W, 3);
  display.print("W PWM:");
  display.println(measurements.pwm);

  display.display();
}
