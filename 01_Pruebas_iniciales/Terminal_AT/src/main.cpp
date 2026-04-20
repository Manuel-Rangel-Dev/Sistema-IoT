#include <Arduino.h>

// UART hacia el SIM7670G
#define SIM_RX  17
#define SIM_TX  18
#define SIM_DTR 45

HardwareSerial simSerial(1);

// Sincronizar comunicación con el módulo
void syncSIM() {
  for (int i = 0; i < 5; i++) {
    simSerial.println("AT");
    delay(500);
    if (simSerial.find("OK")) return;
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW); // mantener módulo activo

  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);

  delay(8000); // esperar arranque del módulo
  syncSIM();

  Serial.println("=== MODO TERMINAL AT ===");
  Serial.println("Escribe comandos AT y presiona ENTER");
}

void loop() {
  // Lo que escribes en el monitor → se envía al módulo
  if (Serial.available()) {
    simSerial.write(Serial.read());
  }

  // Lo que responde el módulo → se muestra en el monitor
  if (simSerial.available()) {
    Serial.write(simSerial.read());
  }
}