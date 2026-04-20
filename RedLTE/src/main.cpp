#include <Arduino.h>

// Pines UART hacia el SIM7670G (modo UART, USB DIP en OFF)
#define SIM_RX  17
#define SIM_TX  18
#define SIM_DTR 45

HardwareSerial simSerial(1);

// Enviar comando AT y mostrar respuesta
void sendAT(const char* cmd, int waitMs = 2000) {
  while (simSerial.available()) simSerial.read(); // limpiar buffer

  simSerial.println(cmd);

  Serial.print(">> ");
  Serial.println(cmd);

  long t = millis();
  while (millis() - t < waitMs) {
    while (simSerial.available()) {
      Serial.write(simSerial.read());
    }
  }
  Serial.println();
}

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
  delay(3000);

  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW); // mantener módulo activo

  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);

  delay(8000); // esperar arranque completo del módulo

  Serial.println("=== SIM7670G Claro Test ===");

  syncSIM();                 // sincronizar UART
  sendAT("ATE0");            // desactivar eco

  sendAT("AT+CPIN?");        // verificar SIM
  sendAT("AT+CSQ");          // calidad de señal

  sendAT("AT+COPS=0", 5000); // selección automática de operador (Claro)

  sendAT("AT+CREG=2");       // modo extendido
  sendAT("AT+CREG?");        // estado de registro

  // Configuración APN Claro Colombia
  sendAT("AT+CGDCONT=1,\"IP\",\"internet.comcel.com.co\"");

  sendAT("AT+CGATT=0", 3000); // reset de attach
  sendAT("AT+CGATT=1", 5000); // attach a red de datos

  sendAT("AT+CGATT?");        // verificar attach
  sendAT("AT+CEREG?");        // estado LTE real

  sendAT("AT+CGPADDR");       // obtener IP

  Serial.println("=== Fin ===");
}

void loop() {
  // Modo terminal manual
  if (simSerial.available()) {
    Serial.write(simSerial.read());
  }
  if (Serial.available()) {
    simSerial.write(Serial.read());
  }
}