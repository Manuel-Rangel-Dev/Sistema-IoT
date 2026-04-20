# 📡 ESP32 + SIM7670G — Proyectos de Conectividad y Control

[![PlatformIO](https://img.shields.io/badge/Built%20with-PlatformIO-orange?logo=platformio)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32-green)](https://www.espressif.com/)
[![Network](https://img.shields.io/badge/Network-LTE%20Claro%20Colombia-red)](https://www.claro.com.co/)

Colección de proyectos funcionales desarrollados con **ESP32** y el módulo **SIM7670G** (LTE Cat-1), orientados a conectividad celular, comunicación serial y recolección de datos en la nube mediante AWS IoT Core. Desarrollados y probados en Colombia con red **Claro**.

---

## 📁 Estructura del Repositorio

```
ESP32-SIM7670G-Projects/01_Pruebas_iniciales/
│
├── RedLTE/                 # Conexión a red LTE (Claro Colombia)
│   ├── src/
│   │   └── main.cpp
│   ├── platformio.ini
│   └── README.md
│
├── Controlar_MotorDC/       # Control de motor DC con PWM
│   ├── src/
│   │   └── main.cpp
│   ├── platformio.ini
│   └── README.md
│
├── Terminal_AT/             # Terminal de comandos AT (bridge serial)
│   ├── src/
│   │   └── main.cpp
│   ├── platformio.ini
│   └── README.md
│
├── Wifi/                    # Gestión de conectividad WiFi
│   ├── src/
│   │   └── main.cpp
│   ├── platformio.ini
│   └── README.md
│
├── .gitignore
├── LICENSE
└── README.md                   ← Estás aquí
```

---

## 🧰 Hardware Utilizado

| Componente | Modelo / Descripción | Cantidad |
|---|---|---|
| Microcontrolador | ESP32-S3 DevKit v1 (o similar, 38 pines) | 1 |
| Módulo LTE/GNSS | SIM7670G (LTE Cat-1, GNSS integrado) | 1 |
| Driver de motor | L293D | 1 |
| Motor DC | Motor de codificador TT de alto par (5-12V) | 1 |
| SIM Card | SIM Claro Colombia (plan con datos activo) | 1 |
| Fuente de alimentación | 5 V / 2 A mínimo recomendado | 1 |
| Cables jumper | Macho-Macho / Macho-Hembra | varios |
| Protoboard | Tamaño estándar (830 puntos) | 1 |

> ⚠️ **Nota de alimentación:** El SIM7670G puede consumir picos de hasta 2 A durante transmisión LTE. Se recomienda usar una fuente dedicada o un capacitor de desacople de al menos 1000 µF en la línea de alimentación del módulo.

---

## 🗂️ Proyectos

### 1. 📶 Conexión a Red LTE — `RedLTE/`
Configura el APN de Claro Colombia y verifica el registro en red LTE usando comandos AT directos al SIM7670G.

**Lo que hace:**
- Configura APN (`internet.comcel.com.co`)
- Verifica señal y registro en red con `AT+CSQ` y `AT+CREG`
- Muestra estado en Serial Monitor

---

### 2. ⚙️ Control de Motor DC — `Controlar_MotorDC/`
Control de velocidad de un motor DC usando el canal LEDC (PWM) del ESP32.

**Lo que hace:**
- Control de velocidad con `ledcWrite()`
- Frenado por software

---

### 3. 🖥️ Terminal de Comandos AT — `Terminal_AT/`
Bridge serial entre el PC y el módulo SIM7670G. Permite enviar comandos AT manualmente desde el Serial Monitor de VS Code.

**Lo que hace:**
- Reenvío bidireccional: `PC → ESP32 → SIM7670G` y viceversa
- Ideal para depuración y exploración del módem
- Baudrate configurable

---

### 4. 📡 Conexión a WiFi — `Wifi/`
Gestión básica de conectividad WiFi con el ESP32.

**Lo que hace:**
- Conexión a red WPA2
- Reconexión automática si se pierde el enlace
- Muestra IP asignada por Serial Monitor

---

## ⚙️ Configuración en PlatformIO

Cada proyecto tiene su propio `platformio.ini`. El siguiente es un ejemplo base para todos los proyectos:

```ini
; platformio.ini — ejemplo base
[env:esp32dev]
platform  = espressif32
board     = esp32dev
framework = arduino
monitor_speed = 115200

; Librerías (agregar según el proyecto)
lib_deps =
    ; Para proyectos LTE con abstracción:
    ; vshymanskyy/TinyGSM @ ^0.11.7
    ; Para el monitoreo eléctrico del motor:
    ; adafruit/Adafruit INA219@^1.2.3
```

### Instalación del entorno

1. Instalar [VS Code](https://code.visualstudio.com/)
2. Instalar la extensión **PlatformIO IDE** desde el Marketplace de VS Code
3. Clonar este repositorio:
   ```bash
   git clone https://github.com/Manuel-Rangel-Dev/Sistema-IoT
   ```
4. Abrir la carpeta del proyecto que quieres usar en VS Code
5. PlatformIO descargará automáticamente las dependencias al compilar (`Ctrl+Alt+B`)

### Librería TinyGSM

Se hace uso de la librería [TinyGSM](https://github.com/vshymanskyy/TinyGSM), que permite usar el SIM7670G como si fuera un cliente de red estándar (HTTP, MQTT, TCP). Para instalarla manualmente:

```ini
lib_deps =
    vshymanskyy/TinyGSM @ ^0.11.7
```

Y agrega al inicio de tu `main.cpp`:
```cpp
#define TINY_GSM_MODEM_SIM7600   // SIM7670G es compatible con este perfil
#include <TinyGsmClient.h>
```

---

## 🔌 Conexiones Típicas (Pinout)

| ESP32 GPIO | SIM7670G Pin | Descripción |
|---|---|---|
| GPIO 17 (TX2) | RX | Datos hacia el módem |
| GPIO 18 (RX2) | TX | Datos desde el módem |
| GPIO 45 | DTR | Encendido/apagado módulo |
| GND | GND | Tierra común |
| — | VCC (4.2 V) | Alimentación externa dedicada |

> Los pines pueden variar según tu breakout board del SIM7670G. Verifica el esquemático de tu módulo.

---

## 🤝 Contribuciones

Este es un repositorio de aprendizaje personal, pero las sugerencias son bienvenidas. Si encuentras un error o tienes una mejora, abre un [Issue](../../issues) o envía un Pull Request.

---

## 📄 Licencia

Distribuido bajo la licencia **MIT**. Consulta el archivo [LICENSE](LICENSE) para más información.

---

*Desarrollado con ☕ y mucha paciencia con el Serial Monitor.*
