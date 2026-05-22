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
├── 03_Variable_SMS/             # Control de variable PWM mediante comandos SMS
│   ├── src/
│   │   └── main.cpp
│   └── platformio.ini
│
├── 04_RedLTE-MotorDC-SMS/       # Monitoreo LTE + control PWM del motor por SMS
│   ├── src/
│   │   └── main.cpp
│   └── platformio.ini
│
├── 05_RedLTE_MotorDC_Pantalla/  # Monitoreo LTE + motor DC + pantalla OLED
│   ├── src/
│   │   └── main.cpp
│   └── platformio.ini
│
├── Final/                       # Sistema integrado modular: LTE, SMS, OLED, motor y servidor
│   ├── src/
│   │   ├── config.h
│   │   ├── display_ui.cpp / display_ui.h
│   │   ├── lte.cpp / lte.h
│   │   ├── motor.cpp / motor.h
│   │   ├── sensors.cpp / sensors.h
│   │   ├── sms.cpp / sms.h
│   │   ├── telemetry.h
│   │   └── main.cpp
│   └── platformio.ini
│
├── Servidor/                    # Servidor HTTP y dashboard en Streamlit
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
| Pantalla OLED | SSD1306 128×32 por I²C | 1 |
| Potenciómetro | 10 kΩ (control manual de PWM en `02`; no se usa para control en `04`) | 1 |
| SIM Card | SIM Claro Colombia (plan con datos activo) | 1 |
| Fuente de alimentación | 5 V / 2 A mínimo recomendado | 1 |
| Cables jumper | Macho-Macho / Macho-Hembra | varios |
| Protoboard | Tamaño estándar (830 puntos) | 1 |

> ⚠️ **Nota de alimentación:** El SIM7670G puede consumir picos de hasta 2 A durante transmisión LTE. Se recomienda usar una fuente dedicada o un capacitor de desacople de al menos 1000 µF en la línea de alimentación del módulo.

> **Nota sobre el PWM:** En `02_RedLTE-MotorDC` el PWM se controla con potenciómetro. En `04_RedLTE-MotorDC-SMS` y en `Final` el potenciómetro se eliminó del control y el PWM se ajusta mediante SMS. En `05_RedLTE_MotorDC_Pantalla` se usó un valor de PWM fijo en el código para pruebas de laboratorio.

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

### 📩 03 — Control de Variable por SMS — `03_Variable_SMS/`

Proyecto de validación para controlar una variable interna del ESP32-S3 mediante mensajes SMS recibidos por el SIM7670G. La variable usada para la prueba es `pwm`.

#### Funcionamiento

1. Un usuario envía un SMS al número de la SIM instalada en el SIM7670G.
2. El mensaje debe tener el formato:

```text
pwm valor
```

Ejemplo:

```text
pwm 230
```

3. El ESP32-S3 lee el SMS, extrae el número remitente y normaliza el contenido.
4. Si el comando es válido, actualiza la variable `pwm`.
5. El módulo responde por SMS al remitente confirmando el cambio.

#### Validaciones implementadas

- Acepta `pwm` en mayúsculas o minúsculas (`pwm 200`, `PWM 200`).
- Elimina comillas envolventes cuando el módem entrega el contenido como `"pwm 200"`.
- Tolera espacios extra entre comando y valor.
- Decodifica contenido UCS2 hexadecimal cuando el módem entrega el SMS en ese formato.
- Rechaza comandos desconocidos.
- Valida que el PWM esté entre `0` y `255`.
- Borra el SMS procesado con `AT+CMGD` para evitar llenar la memoria de la SIM.

#### Comandos AT relevantes

```text
AT+CSCS="GSM"
AT+CMGF=1
AT+CPMS="SM","SM","SM"
AT+CNMI=2,1,0,0,0
AT+CMGR=<indice>
AT+CMGD=<indice>
```

---

### 🚀 04 — Monitoreo LTE + Motor DC + Control por SMS — `04_RedLTE-MotorDC-SMS/`

Proyecto final integrado. Combina el monitoreo del motor DC de `02_RedLTE-MotorDC` con el control remoto por SMS validado en `03_Variable_SMS`.

La estructura principal de `02_RedLTE-MotorDC` se conserva: encoder, INA219, conexión LTE, empaquetado JSON y envío HTTP al servidor. La diferencia clave es que el control de PWM por potenciómetro fue eliminado y reemplazado por control mediante SMS.

#### Variables monitoreadas y enviadas

| Variable | Sensor / Fuente | Descripción |
|---|---|---|
| `rpm` | Encoder integrado del motor | Velocidad calculada a partir de pulsos por interrupción |
| `current_A` | INA219 | Corriente instantánea en amperios |
| `voltage_V` | INA219 | Voltaje del motor |
| `power_W` | INA219 | Potencia consumida en watts |
| `pwm` | SMS / `g_pwm_actual` | Duty cycle aplicado al L293D |

#### Formato serial

El monitor serial imprime los datos en formato CSV:

```text
rpm,current_A,voltage_V,power_W,pwm
0.00,-0.0001,2.516,0.0000,0
```

El valor de PWM ya no se imprime en una línea separada como `[PWM] Valor actual`. Se refleja únicamente en la última columna del registro CSV.

#### Formato JSON enviado al servidor

Cada muestra se empaqueta y se envía por HTTP al endpoint `/data`:

```json
{
  "rpm": 0.00,
  "current_A": -0.0001,
  "voltage_V": 2.516,
  "power_W": 0.0000,
  "pwm": 200
}
```

#### Control PWM por SMS

El usuario envía un SMS con el formato:

```text
pwm valor
```

Ejemplo:

```text
pwm 180
```

Si el valor es válido, el ESP32-S3 ejecuta:

```cpp
SetMotorPwm((uint8_t)nuevoPWM);
```

y responde al remitente:

```text
PWM actualizado a 180
```

Si el comando no es válido, responde con un mensaje de error indicando el formato correcto o el rango permitido.

#### Recepción robusta de SMS

Además de escuchar la notificación inmediata `+CMTI`, el firmware consulta periódicamente mensajes no leídos:

```text
AT+CMGL="REC UNREAD"
```

Esto permite procesar comandos SMS incluso si el SIM7670G no entrega la notificación instantánea por UART o si esta se pierde mientras el sistema está enviando datos HTTP.

#### Flujo general

```text
SMS "pwm valor" ──► SIM7670G ──► ESP32-S3 ──► SetMotorPwm()
                                               │
                                               ▼
                                            L293D ──► Motor DC
                                               │
                         Encoder (RPM) ◄──────┘
                         INA219 (V, I, P)
                                               │
                                               ▼
                              JSON vía HTTP sobre LTE
                                               │
                                               ▼
                          Servidor + dashboard Streamlit
```

---

### 🖥️ 05 — Monitoreo LTE + Motor DC + Pantalla OLED — `05_RedLTE_MotorDC_Pantalla/`

Este proyecto agrega una pantalla **OLED SSD1306 128×32** al sistema de monitoreo LTE del motor DC. La pantalla permite observar el flujo del programa sin depender únicamente del monitor serial.

#### Funciones principales

- Muestra estados del sistema durante el arranque, la conexión LTE, el registro en red y el envío de datos.
- Muestra mediciones del motor: RPM, corriente, voltaje, potencia y PWM aplicado.
- Muestra respuestas de comandos AT relevantes del SIM7670G, por ejemplo:

```text
ATE0: OK
CPIN: +CPIN: READY
CSQ: +CSQ: ...
CREG: +CREG: ...
CEREG: +CEREG: ...
```

#### PWM de prueba

En este proyecto se reemplazó temporalmente el control por potenciómetro por un valor fijo definido en el código. Esto se hizo para facilitar pruebas controladas del motor sin depender de una entrada analógica externa.

El valor se ajusta en:

```cpp
const uint8_t kPwmPrueba = 120;
```

El rango permitido sigue siendo de `0` a `255`, donde `0` apaga el motor y `255` aplica el máximo PWM.

---

### ✅ Final — Sistema Integrado Modular — `Final/`

El proyecto `Final` combina las funcionalidades desarrolladas en `04_RedLTE-MotorDC-SMS` y `05_RedLTE_MotorDC_Pantalla`. El resultado es un sistema completo que mide variables del motor, envía datos por LTE a un servidor, muestra el estado del proceso en una pantalla OLED y permite cambiar el PWM mediante mensajes SMS.

#### Funcionalidades integradas

- Lectura de RPM mediante encoder.
- Lectura de voltaje, corriente y potencia mediante el INA219.
- Control del motor DC mediante PWM aplicado al driver L293D.
- Conexión LTE con el SIM7670G usando TinyGSM.
- Envío de datos al servidor HTTP en formato JSON.
- Visualización en pantalla OLED del flujo del código, respuestas AT y mediciones.
- Cambio remoto del PWM mediante SMS con el formato `pwm valor`.
- Consulta periódica de SMS no leídos para evitar pérdida de comandos.

#### Comando SMS para cambiar el PWM

El usuario debe enviar un SMS a la SIM instalada en el SIM7670G con el siguiente formato:

```text
pwm valor
```

Ejemplo:

```text
pwm 180
```

Si el valor es válido, el sistema actualiza el PWM del motor, muestra el cambio en la OLED y responde al remitente:

```text
PWM actualizado a 180
```

Si el valor está fuera de rango o el comando no es reconocido, el sistema responde con un mensaje de error. El rango válido es de `0` a `255`.

#### Estructura modular del firmware final

El proyecto `Final` fue dividido en varios archivos para mejorar la organización del código y facilitar el mantenimiento:

| Archivo | Responsabilidad |
|---|---|
| `main.cpp` | Coordina el arranque, el ciclo principal, el muestreo, el envío de datos y la gestión de SMS. |
| `config.h` | Centraliza pines, constantes, APN, servidor, tiempos de muestreo y límites de PWM. |
| `motor.cpp` / `motor.h` | Inicializa el motor, controla el PWM y calcula las RPM a partir del encoder. |
| `sensors.cpp` / `sensors.h` | Inicializa el INA219 y lee corriente, voltaje y potencia. |
| `display_ui.cpp` / `display_ui.h` | Gestiona la pantalla OLED, los estados del sistema y las mediciones mostradas. |
| `lte.cpp` / `lte.h` | Gestiona el SIM7670G, los comandos AT, la conexión LTE y el envío HTTP. |
| `sms.cpp` / `sms.h` | Configura SMS, lee mensajes, valida comandos y actualiza el PWM. |
| `telemetry.h` | Define la estructura de datos usada para transportar las mediciones del sistema. |

#### Flujo general del proyecto final

```text
SMS "pwm valor" ──► SIM7670G ──► ESP32-S3 ──► Control PWM ──► L293D ──► Motor DC
                                      │
                                      ├──► OLED: estados, respuestas AT y mediciones
                                      │
                                      ├──► Encoder: cálculo de RPM
                                      │
                                      ├──► INA219: voltaje, corriente y potencia
                                      │
                                      └──► LTE: envío JSON al servidor HTTP
```

#### Formato JSON enviado por `Final`

```json
{
  "rpm": 0.00,
  "current_A": 0.0000,
  "voltage_V": 0.000,
  "power_W": 0.0000,
  "pwm": 180
}
```

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
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
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
