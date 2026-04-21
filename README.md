# 📡 ESP32-S3 + SIM7670G — Proyectos de Conectividad y Control

[![PlatformIO](https://img.shields.io/badge/Built%20with-PlatformIO-orange?logo=platformio)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-green)](https://www.espressif.com/)
[![Network](https://img.shields.io/badge/Network-LTE%20Claro%20Colombia-red)](https://www.claro.com.co/)
[![Cloud](https://img.shields.io/badge/Cloud-AWS-FF9900?logo=amazonaws)](https://aws.amazon.com/)

Colección de proyectos desarrollados con **ESP32-S3** y el módulo **SIM7670G** (LTE Cat-1), orientados a conectividad celular, control de hardware y recolección de datos en la nube mediante **AWS**. Desarrollados y probados en Colombia con red **Claro**.

---

## 📁 Estructura del Repositorio

```
ESP32-S3-SIM7670G-4G/
│
├── 01_Pruebas_iniciales/        # Scripts independientes de validación de hardware
│   ├── RedLTE/                  # Conexión a red LTE con TinyGSM
│   ├── Controlar_MotorDC/       # Control de velocidad por PWM
│   ├── Terminal_AT/             # Bridge serial PC ↔ SIM7670G
│   └── Wifi/                   # Conectividad WiFi básica
│
├── 02_RedLTE-MotorDC/           # Sistema integrado: monitoreo de motor vía LTE → AWS
│   ├── src/
│   │   └── main.cpp
│   ├── platformio.ini
│   └── README.md
│
├── .gitignore
├── LICENSE
└── README.md                    ← Estás aquí
```

---

## 🧰 Hardware Utilizado

| Componente | Modelo / Descripción | Cantidad |
|---|---|---|
| Microcontrolador | ESP32-S3 DevKit | 1 |
| Módulo LTE/GNSS | SIM7670G (LTE Cat-1, GNSS integrado) | 1 |
| Driver de motor | L293D | 1 |
| Motor DC | Motor TT de alto par con encoder integrado (5–12 V) | 1 |
| Sensor eléctrico | INA219 (voltaje, corriente y potencia) | 1 |
| Potenciómetro | 10 kΩ (control manual de PWM) | 1 |
| SIM Card | SIM Claro Colombia (plan con datos activo) | 1 |
| Fuente de alimentación | 5 V / 2 A mínimo recomendado | 1 |
| Cables jumper | Macho-Macho / Macho-Hembra | varios |
| Protoboard | Tamaño estándar (830 puntos) | 1 |

> ⚠️ **Nota de alimentación:** El SIM7670G puede consumir picos de hasta 2 A durante transmisión LTE. Se recomienda usar una fuente dedicada o un capacitor de desacople de al menos 1000 µF en la línea de alimentación del módulo.

---

## 🗂️ Proyectos

### 📂 01 — Pruebas Iniciales

Scripts independientes desarrollados para validar cada componente por separado antes de integrarlos. Son el punto de partida del proyecto.

#### 📶 Conexión a Red LTE — `01_Pruebas_iniciales/RedLTE/`
Configura el APN de Claro Colombia y verifica el registro en red LTE usando la librería **TinyGSM** con el SIM7670G.

- Configura APN (`internet.comcel.com.co`)
- Verifica señal y registro en red (`AT+CSQ`, `AT+CREG`)
- Muestra estado por Serial Monitor

---

#### ⚙️ Control de Motor DC — `01_Pruebas_iniciales/Controlar_MotorDC/`
Control de velocidad de un motor DC usando el canal LEDC (PWM) del ESP32-S3.

- Control de velocidad con `ledcWrite()`
- Frenado por software

---

#### 🖥️ Terminal de Comandos AT — `01_Pruebas_iniciales/Terminal_AT/`
Bridge serial entre el PC y el módulo SIM7670G para enviar comandos AT manualmente desde el Serial Monitor.

- Reenvío bidireccional: `PC → ESP32-S3 → SIM7670G` y viceversa
- Útil para depuración y exploración del módem
- Baudrate configurable

---

#### 📡 Conexión a WiFi — `01_Pruebas_iniciales/Wifi/`
Gestión básica de conectividad WiFi con el ESP32-S3.

- Conexión a red WPA2
- Reconexión automática si se pierde el enlace
- Muestra IP asignada por Serial Monitor

---

### 🔬 02 — Sistema Integrado: Monitoreo de Motor vía LTE → AWS — `02_RedLTE-MotorDC/`

Proyecto principal que integra todo lo validado en las pruebas iniciales. El ESP32-S3 recolecta variables eléctricas y mecánicas de un motor DC en tiempo real, y las transmite mediante **LTE (TinyGSM)** a un servidor **HTTP en AWS**.

#### Variables monitoreadas

| Variable | Sensor / Fuente | Descripción |
|---|---|---|
| Voltaje (V) | INA219 (I²C) | Tensión en los bornes del motor |
| Corriente (mA) | INA219 (I²C) | Consumo instantáneo del motor |
| Potencia (mW) | INA219 (calculada) | Potencia eléctrica consumida |
| Velocidad (RPM) | Encoder integrado del motor | Pulsos contados por interrupción |
| PWM (%) | Potenciómetro + código | Duty cycle aplicado al L293D |

#### Flujo de datos

```
Potenciómetro ──► PWM ──► L293D ──► Motor DC
                                        │
                              Encoder (RPM) ──────────────┐
                              INA219 (V, I, P) ───────────┤
                                                          ▼
                                               ESP32-S3 (TinyGSM)
                                                          │
                                                    SIM7670G (LTE)
                                                          │
                                                   Claro Colombia
                                                          │
                                              Servidor HTTP en AWS
```

#### Librerías utilizadas

```ini
lib_deps =
    vshymanskyy/TinyGSM @ ^0.11.7
    adafruit/Adafruit INA219 @ ^1.2.3
```

#### 📝 Registro de datos y conversión a CSV

Para poder realizar una comparación entre los datos enviados del motor y los datos recibidos en el servidor **HTTP en AWS**, se implementó un sistema de registro automático del monitor serial y su posterior conversión a `.csv`.

En el archivo `platformio.ini` se agrega una línea de configuración que permite guardar lo que aparezca en el monitor serial como archivo.

```ini
[env:esp32-s3-devkitc-1]
...
monitor_filters = log2file
...
```

Esto genera un archivo con todo el flujo serial, incluyendo mensajes como:

```
[HTTP] Conectando al servidor...
1500.25,2.30,12.50,28.75,200
[HTTP] ✓ Datos enviados correctamente.
```

Dado que el archivo `.log` contiene tanto datos como mensajes de depuración, se desarrolló un script en Python para extraer únicamente las líneas válidas y convertirlas a `.csv`.

```Python
import re
import sys

patron = re.compile(r'^-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+,-?\d+\.\d+,\d+')

archivo_entrada = sys.argv[1]
archivo_salida  = archivo_entrada.replace(".log", ".csv")

with open(archivo_entrada, "r") as f_in, open(archivo_salida, "w") as f_out:
    f_out.write("rpm,current_A,voltage_V,power_W,pwm\n")
    for linea in f_in:
        linea = linea.strip()
        if patron.match(linea):
            f_out.write(linea + "\n")

print(f"CSV guardado en: {archivo_salida}")
```

##### Uso del script

Primero se debe ejecutar cualquier código que tenga visualización de datos en el monitor serial, este genera un archivo que termina en `.log`. Luego, en la terminal se ejecuta `python filtrar_log.py datos.log`, donde el archivo `datos.log` debe ser reemplazado por el nombre del archivo generado. Por último, esto genera un archivo `datos.csv` con la estructura deseada para poder ser usado en comparaciones.

---

## ⚙️ Configuración en PlatformIO

### Instalación del entorno

1. Instalar [VS Code](https://code.visualstudio.com/)
2. Instalar la extensión **PlatformIO IDE** desde el Marketplace de VS Code
3. Clonar este repositorio:
   ```bash
   git clone https://github.com/Manuel-Rangel-Dev/Sistema-IoT
   ```
4. Abrir la carpeta del proyecto que deseas usar en VS Code
5. PlatformIO descargará automáticamente las dependencias al compilar (`Ctrl+Alt+B`)

### `platformio.ini` base

```ini
[env:esp32-s3-devkitc-1]
platform      = espressif32
board         = esp32-s3-devkitc-1
framework     = arduino
monitor_speed = 115200

lib_deps =
    vshymanskyy/TinyGSM @ ^0.11.7
    adafruit/Adafruit INA219 @ ^1.2.3
```

### Configuración de TinyGSM

Agrega al inicio de `main.cpp`, **antes** de cualquier otro include:

```cpp
#define TINY_GSM_MODEM_SIM7600   // SIM7670G es compatible con este perfil
#include <TinyGsmClient.h>
```

---

## 🔌 Pinout — ESP32-S3 ↔ SIM7670G

| ESP32-S3 GPIO | SIM7670G Pin | Descripción |
|---|---|---|
| GPIO 17 (TX) | RX | Datos hacia el módem |
| GPIO 18 (RX) | TX | Datos desde el módem |
| GPIO 45 | DTR | Control de encendido del módulo |
| GND | GND | Tierra común |
| — | VCC (4.2 V) | Alimentación externa dedicada |

> Los pines pueden variar según tu breakout board del SIM7670G. Verifica el esquemático de tu módulo.

---

## 🤝 Contribuciones

Repositorio de aprendizaje personal. Las sugerencias son bienvenidas — abre un [Issue](../../issues) o envía un Pull Request.

---

## 📄 Licencia

Distribuido bajo la licencia **MIT**. Consulta el archivo [LICENSE](LICENSE) para más información.

---

*Desarrollado con ☕ y mucha paciencia con el Serial Monitor.*
