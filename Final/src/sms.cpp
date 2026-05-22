#include "sms.h"

#include <Arduino.h>
#include "config.h"
#include "display_ui.h"
#include "lte.h"
#include "motor.h"

namespace {
String simBuffer = "";

bool sendSMS(const String& number, const String& message) {
  HardwareSerial& simSerial = simSerialPort();
  while (simSerial.available()) simSerial.read();

  simSerial.print("AT+CMGS=\"");
  simSerial.print(number);
  simSerial.println("\"");

  unsigned long start = millis();
  bool promptReady = false;
  while (millis() - start < 5000) {
    if (simSerial.available() && simSerial.read() == '>') {
      promptReady = true;
      break;
    }
  }

  if (!promptReady) {
    Serial.println("[SMS] Error: el modulo no entrego prompt para enviar SMS.");
    displayState("SMS error prompt");
    return false;
  }

  simSerial.print(message);
  simSerial.write(26);

  String response = readATResponse(15000);
  Serial.println("[SMS] Respuesta envio:");
  Serial.println(response);

  return response.indexOf("OK") != -1;
}

void deleteSMS(int index) {
  HardwareSerial& simSerial = simSerialPort();
  simSerial.print("AT+CMGD=");
  simSerial.println(index);
  readATResponse(3000);
}

String extractSenderNumber(const String& response) {
  int cmgr = response.indexOf("+CMGR:");
  if (cmgr == -1) return "";

  int firstComma = response.indexOf(',', cmgr);
  if (firstComma == -1) return "";

  int numberStart = response.indexOf('"', firstComma);
  if (numberStart == -1) return "";

  int numberEnd = response.indexOf('"', numberStart + 1);
  if (numberEnd == -1) return "";

  return response.substring(numberStart + 1, numberEnd);
}

String extractSMSText(const String& response) {
  int cmgr = response.indexOf("+CMGR:");
  if (cmgr == -1) return "";

  int headerEnd = response.indexOf('\n', cmgr);
  if (headerEnd == -1) return "";

  String rest = response.substring(headerEnd + 1);
  rest.replace("\r", "");

  int ok = rest.indexOf("\nOK");
  if (ok != -1) {
    rest = rest.substring(0, ok);
  }

  rest.trim();
  return rest;
}

bool isHexadecimal(char c) {
  return isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

String decodeUCS2IfNeeded(String text) {
  text.trim();

  if (text.length() < 4 || text.length() % 4 != 0) {
    return text;
  }

  for (int i = 0; i < text.length(); i++) {
    if (!isHexadecimal(text[i])) {
      return text;
    }
  }

  String decoded = "";
  for (int i = 0; i < text.length(); i += 4) {
    int highByte = hexValue(text[i]) * 16 + hexValue(text[i + 1]);
    int lowByte = hexValue(text[i + 2]) * 16 + hexValue(text[i + 3]);

    if (highByte == 0 && lowByte >= 32 && lowByte <= 126) {
      decoded += char(lowByte);
    } else {
      return text;
    }
  }

  return decoded;
}

String normalizeCommandText(String text) {
  text = decodeUCS2IfNeeded(text);
  text.trim();

  if (text.length() >= 2 && text[0] == '"' && text[text.length() - 1] == '"') {
    text = text.substring(1, text.length() - 1);
    text.trim();
  }

  text.toLowerCase();

  String normalized = "";
  bool pendingSpace = false;

  for (int i = 0; i < text.length(); i++) {
    unsigned char c = text[i];

    if (c <= 32 || c == 160) {
      pendingSpace = normalized.length() > 0;
      continue;
    }

    if (pendingSpace) {
      normalized += ' ';
      pendingSpace = false;
    }

    normalized += char(c);
  }

  normalized.trim();
  return normalized;
}

void handleSMSCommand(int index) {
  HardwareSerial& simSerial = simSerialPort();

  Serial.print("[SMS] Leyendo mensaje en indice ");
  Serial.println(index);
  displayState("SMS recibido");

  while (simSerial.available()) simSerial.read();
  simSerial.print("AT+CMGR=");
  simSerial.println(index);

  String response = readATResponse(5000);
  Serial.println("[SMS] Contenido recibido:");
  Serial.println(response);

  String number = extractSenderNumber(response);
  String text = extractSMSText(response);

  if (number.length() == 0 || text.length() == 0) {
    Serial.println("[SMS] No se pudo leer numero o texto del mensaje.");
    displayState("SMS invalido");
    deleteSMS(index);
    return;
  }

  String command = normalizeCommandText(text);

  Serial.print("[SMS] Texto normalizado: '");
  Serial.print(command);
  Serial.println("'");

  int separator = command.indexOf(' ');
  String commandWord = separator == -1 ? command : command.substring(0, separator);

  if (commandWord != "pwm") {
    sendSMS(number, "Comando no reconocido. Usa: pwm valor");
    displayState("SMS comando invalido");
    deleteSMS(index);
    return;
  }

  String valueText = separator == -1 ? "" : command.substring(separator + 1);
  valueText.trim();

  if (valueText.length() == 0) {
    sendSMS(number, "Valor PWM invalido. Usa un numero entre 0 y 255.");
    displayState("PWM invalido");
    deleteSMS(index);
    return;
  }

  for (int i = 0; i < valueText.length(); i++) {
    if (!isDigit(valueText[i])) {
      sendSMS(number, "Valor PWM invalido. Usa un numero entre 0 y 255.");
      displayState("PWM invalido");
      deleteSMS(index);
      return;
    }
  }

  int newPwm = valueText.toInt();
  if (newPwm < PWM_MIN || newPwm > PWM_MAX) {
    sendSMS(number, "Valor PWM fuera de rango. Usa un numero entre 0 y 255.");
    displayState("PWM fuera rango");
    deleteSMS(index);
    return;
  }

  setMotorPwm((uint8_t)newPwm);
  Serial.print("[SMS] PWM actualizado a ");
  Serial.println(getMotorPwm());

  displayState("PWM SMS: " + String(getMotorPwm()));
  sendSMS(number, "PWM actualizado a " + String(getMotorPwm()));
  deleteSMS(index);
}
}

void beginSMS() {
  HardwareSerial& simSerial = simSerialPort();

  displayState("Config SMS");
  simSerial.println("AT+CSCS=\"GSM\"");
  delay(100);
  simSerial.println("AT+CMGF=1");
  delay(100);
  simSerial.println("AT+CPMS=\"SM\",\"SM\",\"SM\"");
  delay(100);
  simSerial.println("AT+CNMI=2,1,0,0,0");
  delay(100);
}

void processSMS() {
  HardwareSerial& simSerial = simSerialPort();

  while (simSerial.available()) {
    char c = simSerial.read();

    if (c == '\n') {
      simBuffer.trim();

      if (simBuffer.startsWith("+CMTI:")) {
        int comma = simBuffer.lastIndexOf(',');
        if (comma != -1) {
          int index = simBuffer.substring(comma + 1).toInt();
          if (index > 0) {
            handleSMSCommand(index);
          }
        }
      }

      simBuffer = "";
    } else if (c != '\r') {
      simBuffer += c;
    }
  }
}

void pollUnreadSMS() {
  HardwareSerial& simSerial = simSerialPort();

  while (simSerial.available()) simSerial.read();
  simSerial.println("AT+CMGL=\"REC UNREAD\"");

  String response = readATResponse(5000);
  if (response.indexOf("+CMGL:") == -1) {
    if (response.indexOf("ERROR") != -1) {
      Serial.println("[SMS] Error consultando mensajes no leidos:");
      Serial.println(response);
      displayState("Error consulta SMS");
    }
    return;
  }

  Serial.println("[SMS] Mensajes no leidos encontrados:");
  Serial.println(response);
  displayState("SMS no leidos");

  int pos = 0;
  while (true) {
    int cmgl = response.indexOf("+CMGL:", pos);
    if (cmgl == -1) break;

    int indexStart = cmgl + 6;
    while (indexStart < response.length() && response[indexStart] == ' ') {
      indexStart++;
    }

    int indexEnd = response.indexOf(',', indexStart);
    if (indexEnd == -1) break;

    int index = response.substring(indexStart, indexEnd).toInt();
    if (index > 0) {
      handleSMSCommand(index);
    }

    pos = indexEnd + 1;
  }
}
