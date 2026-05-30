// ============================================================
//  GPS_Test.ino  –  Prueba AISLADA del GNSS del SIM7670G
//  Placa: Waveshare ESP32-S3-SIM7670G-4G
//
//  Qué hace: enciende el GNSS, consulta la posición cada 3 s
//  con AT+CGNSSINFO y la imprime en el Monitor Serie.
//
//  NO usa servidor, ni INA219, ni motor, ni LTE: solo GPS.
//  Comandos AT basados en los ejemplos de Waveshare y Makerfabs
//  para el módulo SIM7670G.
// ============================================================

#include <Arduino.h>

// --- Pines del módulo SIM7670G (iguales a tu firmware principal) ---
constexpr uint8_t SIM_RX  = 17;   // RX del ESP32 (recibe del módulo)
constexpr uint8_t SIM_TX  = 18;   // TX del ESP32 (envía al módulo)
constexpr uint8_t SIM_DTR = 45;

// UART hacia el SIM7670G
HardwareSerial simSerial(1);

// Cada cuánto consultar la posición
constexpr uint32_t GPS_QUERY_INTERVAL_MS = 3000;
uint32_t lastQuery = 0;

// ------------------------------------------------------------
// Envía un comando AT, espera la respuesta, la imprime y la devuelve.
// ------------------------------------------------------------
String sendAT(const String& cmd, uint32_t timeoutMs = 2000) {
  while (simSerial.available()) simSerial.read();   // limpia buffer de entrada

  Serial.print(">> ");
  Serial.println(cmd);
  simSerial.println(cmd);

  String resp = "";
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (simSerial.available()) resp += (char)simSerial.read();
  }
  Serial.print(resp);
  return resp;
}

// ------------------------------------------------------------
// Espera a que el módulo responda OK a "AT".
// ------------------------------------------------------------
bool syncModule() {
  for (int i = 0; i < 10; i++) {
    simSerial.println("AT");
    delay(500);
    if (simSerial.find("OK")) return true;
  }
  return false;
}

// ------------------------------------------------------------
// Enciende el GNSS y espera a que esté listo.
// ------------------------------------------------------------
void startGNSS() {
  Serial.println("\n[GNSS] Encendiendo el modulo GNSS (AT+CGNSSPWR=1)...");
  sendAT("AT+CGNSSPWR=1", 3000);

  Serial.println("[GNSS] Esperando 'READY' (puede tardar unos segundos)...");
  uint32_t start = millis();
  String buf = "";
  bool ready = false;
  while (millis() - start < 15000) {
    while (simSerial.available()) buf += (char)simSerial.read();
    if (buf.indexOf("READY") != -1) { ready = true; break; }
  }
  if (ready) Serial.println("[GNSS] Modulo GNSS listo.");
  else       Serial.println("[GNSS] No llego 'READY' (quiza ya estaba encendido). Continuo igual.");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Prueba GNSS - SIM7670G ===");

  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, LOW);

  simSerial.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  delay(3000);

  Serial.println("[INIT] Sincronizando con el modulo...");
  if (syncModule()) {
    Serial.println("[INIT] El modulo responde OK.");
  } else {
    Serial.println("[INIT] El modulo NO responde.");
    Serial.println("       Revisa: DIP switch (USB hacia ESP32-S3), alimentacion y conexion.");
  }

  // Apaga el eco para que el Monitor Serie quede mas limpio
  sendAT("ATE0", 1000);

  startGNSS();

  Serial.println("\n[GNSS] Voy a consultar la posicion cada 3 segundos.");
  Serial.println("[IMPORTANTE]");
  Serial.println("  - Conecta la ANTENA GNSS (la ceramica que vino con la placa) al conector GNSS.");
  Serial.println("  - Ponla cerca de una ventana o al aire libre, con vista al cielo.");
  Serial.println("  - El primer 'fix' (cold start) puede tardar de 30 s a varios minutos.\n");
}

void loop() {
  if (millis() - lastQuery < GPS_QUERY_INTERVAL_MS) return;
  lastQuery = millis();

  String resp = sendAT("AT+CGNSSINFO", 2000);

  int idx = resp.indexOf("+CGNSSINFO:");
  if (idx == -1) {
    Serial.println("[GPS] Sin respuesta de CGNSSINFO.\n");
    return;
  }

  // Texto despues de "+CGNSSINFO:"
  String line = resp.substring(idx + 11);
  line.trim();

  // Si empieza con coma o es muy corta -> todavia no hay fix
  if (line.startsWith(",") || line.length() < 10) {
    Serial.println("[GPS] Buscando satelites... (todavia sin fix)\n");
    return;
  }

  // Separar los campos por comas
  String f[16];
  int count = 0, from = 0;
  for (int i = 0; i < (int)line.length() && count < 16; i++) {
    if (line[i] == ',') { f[count++] = line.substring(from, i); from = i + 1; }
  }
  if (count < 16) f[count++] = line.substring(from);

  // Formato real del SIM7670G:
  //   0=modo 1=satGPS 2=GLONASS 3=BEIDOU 4=Galileo
  //   5=lat 6=N/S 7=lon 8=E/W 9=fecha 10=horaUTC 11=altitud 12=velocidad 13=rumbo ...
  // OJO: lat/lon YA vienen en GRADOS DECIMALES (no en formato NMEA ddmm.mmmm)
  if (f[5].length() == 0 || f[7].length() == 0) {
    Serial.println("[GPS] Aun sin coordenadas validas.\n");
    return;
  }

  double lat = f[5].toDouble();
  double lon = f[7].toDouble();
  if (f[6] == "S") lat = -lat;
  if (f[8] == "W") lon = -lon;

  Serial.println("---------- POSICION GPS ----------");
  Serial.print("Latitud:    "); Serial.println(lat, 6);
  Serial.print("Longitud:   "); Serial.println(lon, 6);
  Serial.print("Altitud:    "); Serial.print(f[11]); Serial.println(" m");
  Serial.print("Sat. GPS:   "); Serial.println(f[1]);
  Serial.print("Fecha:      "); Serial.println(f[9]);
  Serial.print("Hora UTC:   "); Serial.println(f[10]);
  Serial.print("Ver en mapa: https://maps.google.com/?q=");
  Serial.print(lat, 6); Serial.print(","); Serial.println(lon, 6);
  Serial.println("----------------------------------\n");
}