# Monitor de Ruido Ambiental con ESP32 + KY-037

Sistema de monitoreo de ruido en tiempo real construido con un ESP32, un sensor de sonido KY-037, una pantalla LCD I2C y un dashboard web hecho en Node-RED. El dispositivo mide el nivel de ruido ambiental, muestra el estado visualmente (LED RGB + LCD), y envía alertas y telemetría por MQTT a un panel de control donde se pueden visualizar las alertas en un gráfico y descargar un historial completo en formato `.txt`.

Proyecto desarrollado para el curso de Robótica.

## Índice
- [Cómo funciona](#cómo-funciona)
- [Hardware utilizado](#hardware-utilizado)
- [Librerías y tecnologías](#librerías-y-tecnologías)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Instalación y configuración](#instalación-y-configuración)
- [Dashboard](#dashboard)
- [Posibles mejoras futuras](#posibles-mejoras-futuras)
- [Colaboradores](#colaboradores)

## Cómo funciona

1. El ESP32 muestrea continuamente el sensor KY-037 y calcula la amplitud del sonido ambiental (con un filtro de suavizado exponencial).
2. Según el nivel medido, se clasifica en 3 estados y se muestra en el LCD y en el LED RGB:
   - 🟢 **Volumen adecuado**
   - 🟡 **Cuidado con el volumen**
   - 🔴 **Silencio, por favor** (alerta)
3. Si el ruido se mantiene en estado de alerta por más de 2 segundos seguidos, se publica un mensaje MQTT de "Inicio de Alerta" con un número de alerta incremental. Cuando el ruido vuelve a bajar, se publica el "Término de Alerta".
4. Un dashboard en **Node-RED** se suscribe a estos mensajes MQTT, los grafica en tiempo real y los guarda en un log con fecha y hora.
5. Desde el dashboard también se puede:
   - Reiniciar el ESP32 de forma remota (vía MQTT).
   - Descargar el historial completo de alertas en un archivo `.txt`.

## Hardware utilizado

- ESP32 DevKit V1
- Sensor de sonido KY-037
- Pantalla LCD I2C 16x2
- LED RGB (o 3 LEDs individuales: rojo, verde, azul)

## Librerías y tecnologías

**Firmware (Arduino / ESP32):**
- [`LiquidCrystal_I2C`](https://github.com/johnrickman/LiquidCrystal_I2C) — control de la pantalla LCD por I2C
- [`WiFi.h`](https://github.com/espressif/arduino-esp32) — conectividad WiFi del ESP32
- [`PubSubClient`](https://github.com/knolleary/pubsubclient) — cliente MQTT para publicar/suscribirse a los tópicos

**Dashboard:**
- [Node-RED](https://nodered.org/) con [`@flowfuse/node-red-dashboard`](https://dashboard.flowfuse.com/) — interfaz web del panel de control
- Broker MQTT público: [broker.emqx.io](https://www.emqx.com/en/mqtt/public-mqtt5-broker)

## Estructura del proyecto

```
monitor-ruido-esp32/
├── firmware/
│   ├── main.cpp            # Código del ESP32
│   └── config.h.example    # Plantilla de configuración (WiFi + MQTT)
├── dashboard/
│   └── node_red_flow.json  # Flow exportado de Node-RED
├── docs/                   # Fotos, diagramas, capturas de pantalla
└── README.md
```

## Instalación y configuración

### 1. Firmware (ESP32)
1. Instala en el Arduino IDE (o PlatformIO) las librerías: `LiquidCrystal_I2C` y `PubSubClient`.
2. Copia `firmware/config.h.example` como `firmware/config.h` y reemplaza los valores con tu propia red WiFi:
   ```cpp
   #define WIFI_SSID_CONFIG     "tu_red_wifi"
   #define WIFI_PASSWORD_CONFIG "tu_contraseña"
   ```
3. Conecta el hardware según los pines definidos en `main.cpp` (LEDs en 25/26/27, sensor en el pin 32).
4. Compila y sube el código al ESP32.

### 2. Dashboard (Node-RED)
1. Instala [Node-RED](https://nodered.org/docs/getting-started/) y el módulo `@flowfuse/node-red-dashboard`.
2. Importa `dashboard/node_red_flow.json` desde el menú **Import** de Node-RED.
3. Verifica que el broker configurado apunte a `broker.emqx.io` (o cámbialo por uno propio).
4. Accede al panel en `http://localhost:1880/dashboard`.

## Dashboard

El panel incluye:
- Gráfico en tiempo real del nivel de ruido, con líneas de referencia para los umbrales configurados.
- Botón para reiniciar el dispositivo de forma remota.
- Botón de descarga del historial de alertas en `.txt`, con nombre de archivo generado automáticamente según la fecha.

## Colaboradores

Agradecimientos a las personas que formaron parte de este proyecto:

| Foto | Usuario |
|:---:|:---:|
| <img src="https://github.com/Ariel1224L.png" width="50px;"/> | [@Ariel1224L](https://github.com/Ariel1224L) |
| <img src="https://github.com/Kuzub0.png" width="50px;"/> | [@Kuzub0](https://github.com/Kuzub0) |
| <img src="https://github.com/DrrFunky.png" width="50px;"/> | [@DrrFunky](https://github.com/DrrFunky) |
| <img src="https://github.com/ElForceSG.png" width="50px;"/> | [@ElForceSG](https://github.com/ElForceSG) |

## Posibles mejoras futuras

- Migrar de un broker MQTT público a uno privado con autenticación, para evitar que terceros puedan publicar en los mismos tópicos.
- Guardar el historial de alertas en una base de datos en vez de un archivo de texto plano.
- Agregar notificaciones push o por correo cuando se registra una alerta.
