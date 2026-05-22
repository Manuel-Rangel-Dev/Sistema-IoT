#include "lte.h"

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
#include "config.h"
#include "display_ui.h"

namespace {
HardwareSerial simSerial(1);
TinyGsm modem(simSerial);
TinyGsmClient client(modem);

String cleanATResponse(const String& cmd, String response) {
  response.replace("\r", "\n");
  response.replace(cmd, "");

  String firstLine = "";
  String finalStatus = "";

  int start = 0;
  while (start < response.length()) {
    int end = response.indexOf('\n', start);
    if (end == -1) end = response.length();

    String line = response.substring(start, end);
    line.trim();

    if (line.length() > 0) {
      if (line.startsWith("+")) {
        return line;
      }

      if (line == "ERROR") {
        return line;
      }

      if (line == "OK") {
        finalStatus = line;
      }

      if (firstLine.length() == 0) {
        firstLine = line;
      }
    }

    start = end + 1;
  }

  if (firstLine.length() > 0) {
    return firstLine;
  }

  if (finalStatus.length() > 0) {
    return finalStatus;
  }

  return "Sin respuesta";
}

String atLabel(const String& cmd) {
  String label = cmd;

  if (label.startsWith("AT+")) {
    label.remove(0, 3);
  } else if (label.startsWith("AT")) {
    label.remove(0, 2);
  }

  label.replace("?", "");

  if (label.length() == 0) {
    return cmd;
  }

  return label;
}

void syncSIM() {
  logState("Sincronizando SIM7670G");

  for (int i = 0; i < 5; i++) {
    simSerial.println("AT");
    delay(500);

    if (simSerial.find("OK")) {
      logState("Modulo respondiendo");
      return;
    }
  }

  logState("Modulo no responde");
}
}

HardwareSerial& simSerialPort() {
  return simSerial;
}

void beginModemSerial() {
  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW);
  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
}

void sendAT(const char* cmd, int waitMs) {
  while (simSerial.available()) simSerial.read();

  simSerial.println(cmd);

  Serial.print(">> ");
  Serial.println(cmd);

  long start = millis();
  String response = "";
  while (millis() - start < waitMs) {
    while (simSerial.available()) {
      char c = simSerial.read();
      response += c;
      Serial.write(c);
    }
  }

  Serial.println();

  String message = atLabel(cmd) + ": ";
  message += cleanATResponse(cmd, response);
  displayState(message);
  delay(AT_OLED_HOLD_MS);
}

String readATResponse(unsigned long timeoutMs) {
  String response = "";
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    while (simSerial.available()) {
      char c = simSerial.read();
      response += c;
    }

    if (response.indexOf("\r\nOK\r\n") != -1 || response.indexOf("\r\nERROR\r\n") != -1) {
      break;
    }
  }

  return response;
}

void connectLTE() {
  logState("Iniciando LTE Claro");

  syncSIM();
  sendAT("ATE0");
  sendAT("AT+CPIN?");

  logState("Esperando red...");
  modem.waitForNetwork(60000UL);

  if (modem.isNetworkConnected()) {
    logState("Red registrada");
  } else {
    logState("Error registro red");
  }

  sendAT("AT+CSQ");
  sendAT("AT+CREG?");
  sendAT("AT+CEREG?");

  logState("Conectando APN...");
  modem.gprsConnect(APN, "", "");

  if (modem.isGprsConnected()) {
    logState("LTE conectado");

    Serial.print("IP asignada: ");
    Serial.println(modem.getLocalIP());
    displayState("IP asignada");

    logState("Verificando ping...");
    while (simSerial.available()) simSerial.read();
    simSerial.println("AT+CPING=\"8.8.8.8\",1,2");

    unsigned long start = millis();
    String response = "";
    bool pingOk = false;

    while (millis() - start < 6000) {
      while (simSerial.available()) {
        char c = simSerial.read();
        response += c;
        Serial.write(c);
      }

      if (response.indexOf("+CPING: 3") != -1) {
        if (response.indexOf(",0,2") == -1 && response.indexOf(",0,1") == -1) {
          pingOk = true;
        }
        break;
      }
    }

    if (pingOk) {
      logState("Datos activos OK");
    } else {
      logState("Error ping");
    }
  } else {
    logState("Error APN");
  }

  logState("Fin config LTE");
}

bool sendHttpMeasurements(const Measurements& measurements) {
  logState("HTTP conectando...");

  if (client.connect(SERVER_HOST, SERVER_PORT)) {
    String body = "{\"rpm\":" + String(measurements.rpm, 2) +
                  ",\"current_A\":" + String(measurements.current_A, 4) +
                  ",\"voltage_V\":" + String(measurements.voltage_V, 3) +
                  ",\"power_W\":" + String(measurements.power_W, 4) +
                  ",\"pwm\":" + String(measurements.pwm) + "}";

    client.println("POST /data HTTP/1.1");
    client.print("Host: ");
    client.println(SERVER_HOST);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(body.length());
    client.println("Connection: close");
    client.println();
    client.println(body);

    delay(500);
    client.stop();

    logState("Datos enviados OK");
    return true;
  }

  logState("Error servidor");
  return false;
}
