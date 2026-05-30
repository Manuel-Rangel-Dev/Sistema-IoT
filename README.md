# ESP32-S3 + SIM7670G - Monitoreo, control y telemetria IoT

[![PlatformIO](https://img.shields.io/badge/Built%20with-PlatformIO-orange?logo=platformio)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-green)](https://www.espressif.com/)
[![Network](https://img.shields.io/badge/Network-LTE%20Claro%20Colombia-red)](https://www.claro.com.co/)
[![Cloud](https://img.shields.io/badge/Cloud-AWS-FF9900?logo=amazonaws)](https://aws.amazon.com/)

Repositorio de proyectos para una placa ESP32-S3 con modulo SIM7670G. El sistema evoluciona desde pruebas aisladas de hardware hasta una solucion final que mide un motor DC, envia telemetria por LTE a un servidor Flask, muestra datos en una OLED, obtiene posicion GNSS, consulta parametros de red LTE y permite controlar el motor desde un dashboard Streamlit o por SMS.

Los firmwares estan hechos con Arduino Framework en PlatformIO y fueron configurados para red Claro Colombia con APN `internet.comcel.com.co`.

## Estructura del repositorio

```text
ESP32-S3-SIM7670G-4G/
|-- 01_Pruebas_iniciales/
|   |-- Controlar_MotorDC/        # Motor, encoder, INA219 y PWM por monitor serial
|   |-- RedLTE/                   # Registro LTE, APN, IP y prueba de ping
|   |-- Terminal_AT/              # Puente serial PC <-> SIM7670G
|   `-- Wifi/                     # Plantilla base de PlatformIO, sin WiFi implementado
|-- 02_RedLTE-MotorDC/            # Motor + INA219 + LTE + POST HTTP
|-- 03_Variable_SMS/              # Comandos SMS para actualizar una variable PWM
|-- 04_RedLTE-MotorDC-SMS/        # Motor + mediciones + LTE + control PWM por SMS
|-- 05_RedLTE_MotorDC_Pantalla/   # Motor + mediciones + LTE + OLED, PWM fijo de prueba
|-- 06_GPS/                       # Prueba aislada GNSS con AT+CGNSSINFO
|-- Final/                        # Firmware integrado con PID, GPS, OLED, SMS, LTE y servidor
|-- Servidor/
|   |-- server.py                 # API Flask para datos y control
|   `-- app.py                    # Dashboard Streamlit
|-- LICENSE
`-- README.md
```

## Hardware usado

| Componente | Descripcion |
|---|---|
| Microcontrolador | ESP32-S3 DevKit / placa ESP32-S3-SIM7670G |
| Modulo celular | SIM7670G LTE Cat-1 con GNSS |
| Driver de motor | L293D |
| Motor DC | Motor con encoder integrado |
| Sensor electrico | INA219 por I2C |
| Pantalla | OLED SSD1306 128x32 por I2C |
| SIM | SIM Claro Colombia con datos y SMS |
| Alimentacion | Fuente externa para motor y modulo celular |

> Nota: el SIM7670G puede consumir picos altos durante transmision LTE. Usa una alimentacion estable y tierra comun entre ESP32, modulo, driver y sensores.

## Pinout principal

| Funcion | GPIO ESP32-S3 |
|---|---:|
| SIM7670G RX/TX UART | RX `17`, TX `18` |
| SIM7670G DTR | `45` |
| Encoder A/B | `13`, `14` |
| L293D IN1/IN2/ENA | `11`, `12`, `10` |
| I2C SDA/SCL | `16`, `15` |
| Potenciometro en `02` | `9` |

## Configuracion PlatformIO

Todos los proyectos usan este entorno base:

```ini
[env:esp32-s3-sim7670g]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.flash_size = 16MB
board_build.flash_mode = qio
board_build.psram_type = opi
board_build.arduino.memory_type = qio_opi
upload_port = COM6
monitor_port = COM6
monitor_speed = 115200
upload_speed = 921600
```

Dependencias usadas segun el proyecto:

```ini
lib_deps =
    vshymanskyy/TinyGSM
    adafruit/Adafruit INA219
    adafruit/Adafruit SSD1306
    adafruit/Adafruit GFX Library
```

Para el SIM7670G se usa el perfil compatible de TinyGSM:

```cpp
#define TINY_GSM_MODEM_SIM7600
#include <TinyGsmClient.h>
```

## 01 - Pruebas iniciales

### `Controlar_MotorDC`

Valida el conjunto motor + encoder + INA219. El motor se maneja con PWM por LEDC en el pin `ENA`, la direccion queda fija con `IN1=HIGH` e `IN2=LOW`, y el valor PWM se cambia desde el monitor serial.

- PWM inicial: `255`.
- Entradas seriales validas: `0` para detener, o valores `1..255`.
- Mide RPM cada `500 ms`.
- Lee voltaje, corriente y potencia con INA219.
- Encoder configurado con `PPR_MOTOR=12`, `GEAR_RATIO=90`, `CPR=1080`.

### `RedLTE`

Prueba la conexion celular del SIM7670G:

- Sincroniza el modem con `AT`.
- Desactiva eco con `ATE0`.
- Verifica SIM con `AT+CPIN?`.
- Espera registro de red con TinyGSM.
- Consulta `AT+CSQ`, `AT+CREG?` y `AT+CEREG?`.
- Conecta al APN `internet.comcel.com.co`.
- Muestra IP local y hace ping a `8.8.8.8`.
- Luego queda como terminal manual AT.

### `Terminal_AT`

Puente serial bidireccional entre el monitor serial del PC y el SIM7670G. Sirve para probar comandos AT manualmente.

### `Wifi`

Este proyecto no implementa conexion WiFi. El `main.cpp` es la plantilla inicial generada por PlatformIO con una funcion de ejemplo `myFunction`.

## 02 - `RedLTE-MotorDC`

Integra motor DC, encoder, INA219 y envio HTTP por LTE.

- Controla el PWM con potenciometro en GPIO `9`.
- Mapea ADC `0..4095` a PWM `0..255`.
- Calcula RPM cada `5 s`.
- Lee `current_A`, `voltage_V` y `power_W`.
- Envia JSON por `POST /data` al servidor `18.219.119.53:5000`.
- Imprime CSV por serial:

```text
rpm,current_A,voltage_V,power_W,pwm
```

El `platformio.ini` incluye `monitor_filters = log2file` para guardar el monitor serial en archivo `.log`.

## 03 - `Variable_SMS`

Valida recepcion y respuesta de SMS usando el SIM7670G. El comando soportado es:

```text
pwm valor
```

Ejemplo:

```text
pwm 180
```

Comportamiento:

- Configura SMS con `AT+CSCS="GSM"`, `AT+CMGF=1`, `AT+CPMS="SM","SM","SM"` y `AT+CNMI=2,1,0,0,0`.
- Lee el indice entregado por `+CMTI`.
- Extrae numero remitente y texto con `AT+CMGR`.
- Normaliza mayusculas/minusculas, espacios y comillas.
- Decodifica UCS2 hexadecimal cuando aplica.
- Valida rango `0..255`.
- Responde por SMS y borra el mensaje con `AT+CMGD`.
- Reporta el valor PWM actual por serial cada `2 s`.

## 04 - `RedLTE-MotorDC-SMS`

Combina el envio de mediciones del proyecto `02` con control remoto por SMS.

- El potenciometro ya no controla el PWM.
- El comando `pwm valor` llama a `SetMotorPwm`.
- Consulta `+CMTI` y tambien sondea no leidos con `AT+CMGL="REC UNREAD"` cada `2 s`.
- Envia telemetria cada `5 s` a `18.219.119.53:5000`.
- Imprime y envia:

```json
{
  "rpm": 0.0,
  "current_A": 0.0,
  "voltage_V": 0.0,
  "power_W": 0.0,
  "pwm": 0
}
```

## 05 - `RedLTE_MotorDC_Pantalla`

Agrega una OLED SSD1306 128x32 al flujo de medicion y envio LTE.

- Muestra estados de arranque, red, APN, ping y servidor.
- Muestra respuestas resumidas de comandos AT.
- Muestra RPM, corriente, voltaje, potencia y PWM.
- Usa PWM fijo de prueba:

```cpp
const uint8_t kPwmPrueba = 120;
```

No incluye SMS ni dashboard de control; es una prueba de visualizacion local con OLED.

## 06 - `GPS`

Prueba aislada del GNSS del SIM7670G.

- Enciende GNSS con `AT+CGNSSPWR=1`.
- Consulta posicion cada `3 s` con `AT+CGNSSINFO`.
- Detecta cuando aun no hay fix.
- Interpreta latitud y longitud como grados decimales.
- Aplica signo por hemisferio `S` o `W`.
- Imprime enlace de Google Maps.

No usa motor, INA219, LTE de datos ni servidor.

## Final - Sistema integrado

El firmware en `Final/src/main.cpp` es la version mas completa del proyecto.

### Funciones principales

- Medicion de RPM con encoder cada `50 ms`.
- Lectura de INA219 para corriente, voltaje y potencia.
- Control del motor por PWM con L293D.
- Control en lazo cerrado por P, PI, PD o PID.
- Feedforward polinomial calibrado para el motor.
- Modo manual de PWM desde dashboard.
- Comando SMS `pwm valor` como mecanismo alterno.
- OLED SSD1306 para estado y mediciones.
- GNSS con `AT+CGNSSINFO`.
- Parametros LTE tipo G-NetTrack: operador, tecnologia, MCC, MNC, TAC, Cell ID, PCI, banda, EARFCN, RSRP, RSRQ, RSSI, SINR, CSQ y ping.
- Envio HTTP cada `1 s` a `18.226.103.103:5000`.
- Recepcion de configuracion de control desde el servidor en la respuesta de cada `POST /data`.

### Control del motor

El dashboard puede enviar:

- `running`: arranca o detiene el control.
- `mode`: `control` para PID o `manual` para PWM directo.
- `ctrl_type`: `P`, `PI`, `PD` o `PID`.
- `setpoint`: referencia de RPM.
- `kp`, `ki`, `kd`: ganancias del controlador.
- `pwm_manual`: PWM directo `0..255`.

El firmware aplica cambios solo cuando cambia `version` en el servidor. Si el control se detiene, el PWM se lleva a `0`.

### Telemetria JSON

El firmware final envia un JSON con mediciones, control, GPS y red LTE:

```json
{
  "rpm": 24.8,
  "current_A": 0.1200,
  "voltage_V": 11.850,
  "power_W": 1.4200,
  "pwm": 132,
  "setpoint": 25.0,
  "error": 0.2,
  "controller": "PID",
  "running": true,
  "ip": "10.x.x.x",
  "lat": 10.987654,
  "lon": -74.123456,
  "lte": {
    "operator": "Claro",
    "tech": "LTE",
    "mcc": 732,
    "mnc": 101,
    "tac": "...",
    "cid": "...",
    "pci": 0,
    "band": "EUTRAN-BAND...",
    "earfcn": 0,
    "rsrp": 0,
    "rsrq": 0,
    "rssi": 0,
    "sinr": 0,
    "csq": 0,
    "csq_dbm": 0,
    "ping_ms": -1
  }
}
```

### HTTP en el firmware final

La version final no usa el `TinyGsmClient` manual para el POST principal. Usa la pila HTTP nativa del modem con comandos `AT+HTTPINIT`, `AT+HTTPPARA`, `AT+HTTPDATA`, `AT+HTTPACTION` y `AT+HTTPREAD`, enrutados por TinyGSM. Esto permite leer de forma mas confiable el cuerpo de respuesta del servidor.

## Servidor

La carpeta `Servidor` contiene dos programas:

### `server.py`

API Flask que corre en `0.0.0.0:5000`.

Endpoints:

| Metodo | Ruta | Descripcion |
|---|---|---|
| `POST` | `/data` | Recibe telemetria del ESP32 y devuelve la configuracion de control vigente |
| `GET` | `/readings?n=120` | Devuelve las ultimas `n` lecturas almacenadas en memoria |
| `GET` | `/latest` | Devuelve la ultima lectura o `{"status":"OFF"}` |
| `GET` | `/control` | Devuelve la configuracion de control |
| `POST` | `/control` | Actualiza modo, setpoint, ganancias, PWM manual o estado |
| `GET` | `/health` | Health check con estado y numero de lecturas |

El servidor conserva las ultimas `500` lecturas en memoria (`deque`) y usa zona horaria `America/Bogota`.

### `app.py`

Dashboard Streamlit conectado a:

```text
http://18.226.103.103:5000
```

Vistas principales:

- Monitoreo: estado del motor, PWM, graficas de RPM/corriente/voltaje/potencia, tabla y descarga CSV.
- Control: seleccion de P/PI/PD/PID, setpoint, `Kp`, `Ki`, `Kd`, iniciar/frenar y PWM manual.
- Red LTE: operador, tecnologia, celda, banda, frecuencias estimadas, calidad de senal, ping y posicion GNSS.

Dependencias Python:

```bash
pip install flask streamlit pandas numpy plotly requests pytz
```

Ejecucion local:

```bash
python server.py
streamlit run app.py
```

## Flujo del sistema final

```text
Dashboard Streamlit -- POST /control --> Flask
        ^                              |
        |                              v
 GET /readings, /latest         Respuesta de POST /data
        |                              |
        |                              v
ESP32-S3 + SIM7670G -- LTE HTTP --> POST /data
        |
        |-- Encoder -> RPM
        |-- INA219 -> corriente, voltaje, potencia
        |-- OLED -> estado y mediciones
        |-- GNSS -> latitud/longitud
        |-- AT+CPSI?/CSQ/COPS/CPING -> parametros LTE
        `-- SMS "pwm valor" -> PWM directo
```

## Notas importantes

- Las IP `18.219.119.53` y `18.226.103.103` estan quemadas en distintos firmwares. Si cambia la instancia EC2, actualiza `SERVER_HOST` en el firmware correspondiente y `SERVER_URL` en `Servidor/app.py`.
- Los proyectos `02`, `04` y `05` envian cada `5 s`; el proyecto `Final` envia cada `1 s`.
- El proyecto `Final` limita el setpoint entre `11` y `35 RPM`.
- La posicion GNSS puede tardar varios minutos en tener fix y requiere antena con vista al cielo.
- El almacenamiento del servidor es en memoria; al reiniciar `server.py` se pierden las lecturas acumuladas.

## Licencia

Distribuido bajo licencia MIT. Consulta el archivo `LICENSE`.
