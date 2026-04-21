#include <Arduino.h>

//TinyGSM 
#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

// Pines UART hacia el SIM7670G (modo UART, USB DIP en OFF)
#define SIM_RX  17
#define SIM_TX  18
#define SIM_DTR 45

HardwareSerial simSerial(1);
TinyGsm modem(simSerial);

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
  Serial.println("Sincronizando con SIM7670G...");
  for (int i = 0; i < 5; i++) {
    simSerial.println("AT");
    delay(500);
    if (simSerial.find("OK")) {
      Serial.println("Módulo respondiendo.");
      return;
    }
  }
  Serial.println("Advertencia: módulo no responde aún.");
}

void conectarLTE() {
  Serial.println("\n=== Iniciando conexión LTE (Claro Colombia) ===");

  syncSIM();
  sendAT("ATE0");           // desactivar eco
  sendAT("AT+CPIN?");       // verificar SIM

  Serial.println("Esperando registro en red...");
  modem.waitForNetwork(60000UL);

  if (modem.isNetworkConnected()) {
    Serial.println("Red registrada correctamente.");
  } else {
    Serial.println("Advertencia: no se registró en red.");
  }

  sendAT("AT+CSQ");         // calidad de señal
  sendAT("AT+CREG?");       // estado registro
  sendAT("AT+CEREG?");      // estado LTE

  Serial.println("Conectando a APN Claro...");
  modem.gprsConnect("internet.comcel.com.co", "", "");

  if (modem.isGprsConnected()) {
    Serial.println("Conexión LTE exitosa.");
    Serial.print("IP asignada: ");
    Serial.println(modem.getLocalIP());

    Serial.println("Verificando conexión con ping a Google DNS...");
    while (simSerial.available()) simSerial.read();
    simSerial.println("AT+CPING=\"8.8.8.8\",1,2");
    unsigned long start = millis();
    String response = "";
    bool pingExitoso = false;
    while (millis() - start < 6000) {
      while (simSerial.available()) {
        char c = simSerial.read();
        response += c;
        Serial.write(c); 
      }
      if (response.indexOf("+CPING: 3") != -1) {
          if (response.indexOf(",0,2") == -1 && response.indexOf(",0,1") == -1) {
              pingExitoso = true;
          }
          break;
      }
    }

    if (pingExitoso) {
      Serial.println("\n[SISTEMA] ✓ Datos activos y navegación confirmada.");
    } else {
      Serial.println("\n[SISTEMA] ⚠ Error: IP obtenida pero sin respuesta de Ping.");
    }

  } else {
    Serial.println("Error: no se pudo conectar al APN.");
  }

  Serial.println("=== Fin configuración LTE ===\n");
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW); // mantener módulo activo

  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);

  delay(8000); // esperar arranque completo del módulo

  conectarLTE();

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