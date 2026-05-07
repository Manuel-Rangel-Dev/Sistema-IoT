#include <Arduino.h>
#include <Wire.h>

#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>

// ─── Pines SIM7670G ──────────────────────────────────────────
#define SIM_RX  17
#define SIM_TX  18
#define SIM_DTR 45

// ─── Objetos ─────────────────────────────────────────────────
HardwareSerial simSerial(1);
TinyGsm        modem(simSerial);
TinyGsmClient client(modem);

int pwm = 0;
const int PWM_MIN = 0;
const int PWM_MAX = 255;

String bufferSIM = "";
unsigned long ultimoReportePWM = 0;

// ─── Enviar AT ───────────────────────────────────────────────
void sendAT(const char* cmd, int waitMs = 2000) {
  while (simSerial.available()) simSerial.read();
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

String leerRespuestaAT(unsigned long timeoutMs = 3000) {
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

bool enviarSMS(const String& numero, const String& mensaje) {
  while (simSerial.available()) simSerial.read();

  simSerial.print("AT+CMGS=\"");
  simSerial.print(numero);
  simSerial.println("\"");

  unsigned long start = millis();
  bool promptListo = false;
  while (millis() - start < 5000) {
    if (simSerial.available() && simSerial.read() == '>') {
      promptListo = true;
      break;
    }
  }

  if (!promptListo) {
    Serial.println("[SMS] Error: el modulo no entrego prompt para enviar SMS.");
    return false;
  }

  simSerial.print(mensaje);
  simSerial.write(26);

  String response = leerRespuestaAT(15000);
  Serial.println("[SMS] Respuesta envio:");
  Serial.println(response);

  return response.indexOf("OK") != -1;
}

void borrarSMS(int index) {
  simSerial.print("AT+CMGD=");
  simSerial.println(index);
  leerRespuestaAT(3000);
}

String extraerNumeroRemitente(const String& response) {
  int cmgr = response.indexOf("+CMGR:");
  if (cmgr == -1) return "";

  int primeraComa = response.indexOf(',', cmgr);
  if (primeraComa == -1) return "";

  int inicioNumero = response.indexOf('"', primeraComa);
  if (inicioNumero == -1) return "";

  int finNumero = response.indexOf('"', inicioNumero + 1);
  if (finNumero == -1) return "";

  return response.substring(inicioNumero + 1, finNumero);
}

String extraerTextoSMS(const String& response) {
  int cmgr = response.indexOf("+CMGR:");
  if (cmgr == -1) return "";

  int finCabecera = response.indexOf('\n', cmgr);
  if (finCabecera == -1) return "";

  String resto = response.substring(finCabecera + 1);
  resto.replace("\r", "");

  int ok = resto.indexOf("\nOK");
  if (ok != -1) {
    resto = resto.substring(0, ok);
  }

  resto.trim();
  return resto;
}

bool esHexadecimal(char c) {
  return isDigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int valorHex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

String decodificarUCS2SiAplica(String texto) {
  texto.trim();

  if (texto.length() < 4 || texto.length() % 4 != 0) {
    return texto;
  }

  for (int i = 0; i < texto.length(); i++) {
    if (!esHexadecimal(texto[i])) {
      return texto;
    }
  }

  String decodificado = "";
  for (int i = 0; i < texto.length(); i += 4) {
    int highByte = valorHex(texto[i]) * 16 + valorHex(texto[i + 1]);
    int lowByte = valorHex(texto[i + 2]) * 16 + valorHex(texto[i + 3]);

    if (highByte == 0 && lowByte >= 32 && lowByte <= 126) {
      decodificado += char(lowByte);
    } else {
      return texto;
    }
  }

  return decodificado;
}

String normalizarTextoComando(String texto) {
  texto = decodificarUCS2SiAplica(texto);
  texto.trim();

  if (texto.length() >= 2 && texto[0] == '"' && texto[texto.length() - 1] == '"') {
    texto = texto.substring(1, texto.length() - 1);
    texto.trim();
  }

  texto.toLowerCase();

  String normalizado = "";
  bool espacioPendiente = false;

  for (int i = 0; i < texto.length(); i++) {
    unsigned char c = texto[i];

    if (c <= 32 || c == 160) {
      espacioPendiente = normalizado.length() > 0;
      continue;
    }

    if (espacioPendiente) {
      normalizado += ' ';
      espacioPendiente = false;
    }

    normalizado += char(c);
  }

  normalizado.trim();
  return normalizado;
}

// ─── Sincronizar módulo ───────────────────────────────────────
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

void gestionarComandosSMS(int index) {
  Serial.print("[SMS] Leyendo mensaje en indice ");
  Serial.println(index);

  while (simSerial.available()) simSerial.read();
  simSerial.print("AT+CMGR=");
  simSerial.println(index);

  String response = leerRespuestaAT(5000);
  Serial.println("[SMS] Contenido recibido:");
  Serial.println(response);

  String numero = extraerNumeroRemitente(response);
  String texto = extraerTextoSMS(response);

  if (numero.length() == 0 || texto.length() == 0) {
    Serial.println("[SMS] No se pudo leer numero o texto del mensaje.");
    borrarSMS(index);
    return;
  }

  String comando = normalizarTextoComando(texto);

  Serial.print("[SMS] Texto normalizado: '");
  Serial.print(comando);
  Serial.println("'");

  int separador = comando.indexOf(' ');
  String palabraComando = separador == -1 ? comando : comando.substring(0, separador);

  if (palabraComando != "pwm") {
    enviarSMS(numero, "Comando no reconocido. Usa: pwm valor");
    borrarSMS(index);
    return;
  }

  String valorTexto = separador == -1 ? "" : comando.substring(separador + 1);
  valorTexto.trim();

  if (valorTexto.length() == 0) {
    enviarSMS(numero, "Valor PWM invalido. Usa un numero entre 0 y 255.");
    borrarSMS(index);
    return;
  }

  for (int i = 0; i < valorTexto.length(); i++) {
    if (!isDigit(valorTexto[i])) {
      enviarSMS(numero, "Valor PWM invalido. Usa un numero entre 0 y 255.");
      borrarSMS(index);
      return;
    }
  }

  int nuevoPWM = valorTexto.toInt();
  if (nuevoPWM < PWM_MIN || nuevoPWM > PWM_MAX) {
    enviarSMS(numero, "Valor PWM fuera de rango. Usa un numero entre 0 y 255.");
    borrarSMS(index);
    return;
  }

  pwm = nuevoPWM;
  Serial.print("[SMS] PWM actualizado a ");
  Serial.println(pwm);

  enviarSMS(numero, "PWM actualizado a " + String(pwm));
  borrarSMS(index);
}



void setup() {
  Serial.begin(115200);
  delay(3000);

  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW); // Mantener módulo activo

  // Inicializar comunicación con el módulo
  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  delay(8000); 

  // Configuración de red (tu función original)
  conectarLTE();

  // Configuración específica para SMS: Notificación inmediata de nuevos mensajes
  simSerial.println("AT+CSCS=\"GSM\""); // Juego de caracteres para SMS en texto
  delay(100);
  simSerial.println("AT+CMGF=1"); // Modo texto
  delay(100);
  simSerial.println("AT+CPMS=\"SM\",\"SM\",\"SM\""); // Usar memoria de la SIM para SMS
  delay(100);
  simSerial.println("AT+CNMI=2,1,0,0,0"); // Avisar por serial al recibir SMS
  delay(100);

  Serial.println("[SISTEMA] Listo para recibir comandos SMS (ej: 'pwm 150')");
}

void loop() {
  while (simSerial.available()) {
    char c = simSerial.read();

    if (c == '\n') {
      bufferSIM.trim();

      if (bufferSIM.startsWith("+CMTI:")) {
        int coma = bufferSIM.lastIndexOf(',');
        if (coma != -1) {
          int index = bufferSIM.substring(coma + 1).toInt();
          if (index > 0) {
            gestionarComandosSMS(index);
          }
        }
      }

      bufferSIM = "";
    } else if (c != '\r') {
      bufferSIM += c;
    }
  }

  if (millis() - ultimoReportePWM >= 2000) {
    ultimoReportePWM = millis();
    Serial.print("[PWM] Valor actual: ");
    Serial.println(pwm);
  }
}
