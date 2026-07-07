/*
  Monitor de Ruido con ESP32 + KY-037
  -----------------------------------
  Mide el nivel de ruido ambiental con un sensor KY-037, muestra el estado
  en una pantalla LCD I2C (verde / amarillo / rojo según el volumen) y
  publica alertas y lecturas por MQTT para un dashboard externo.

  Requiere un archivo "config.h" (ver config.h.example) con las
  credenciales de WiFi y la configuración del broker MQTT.
*/

#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// --- LCD y pines de los LEDs de estado (RGB) ---
LiquidCrystal_I2C lcd_1(0x27, 16, 2);
const int PIN_LED_ROJO  = 25;
const int PIN_LED_VERDE = 26;
const int PIN_LED_AZUL  = 27;

// --- Pines del sensor de sonido KY-037 ---
const int PIN_SENSOR_SONIDO_A0 = 32; // Salida analógica
const int PIN_SENSOR_SONIDO_D0 = 33; // Salida digital (no utilizada actualmente)

// --- Umbrales de sonido (en unidades de amplitud, no dB) ---
const int UMBRAL_MAXIMO   = 90; // Por encima de esto: alerta roja
const int UMBRAL_ADECUADO = 65; // Por debajo de esto: volumen adecuado

// --- Variables de estado de alerta ---
float amplitudSuavizada = 0;
bool estadoAlerta = false;
bool contandoTiempo = false;
bool mensajeEnviado = false;
unsigned long tiempoInicioRojo = 0;
int totalAlertasSesion = 0;

// --- Temporizador de muestreo del sensor ---
unsigned long ultimaLecturaSensor = 0;
const unsigned long INTERVALO_LECTURA_MS = 500;
const unsigned long DURACION_VENTANA_MUESTREO_MS = 50;
const unsigned long TIEMPO_MINIMO_ALERTA_MS = 2000;

// --- Configuración WiFi y MQTT (definida en config.h) ---
const char* WIFI_SSID     = WIFI_SSID_CONFIG;
const char* WIFI_PASSWORD = WIFI_PASSWORD_CONFIG;
const char* MQTT_BROKER   = MQTT_BROKER_CONFIG;
const int   MQTT_PORT     = MQTT_PORT_CONFIG;
const char* MQTT_CLIENT   = MQTT_CLIENT_CONFIG;

// --- Tópicos MQTT ---
const char* TOPIC_INICIO       = "quiteSystem/monitorRuido/inicioEstadoRojo";
const char* TOPIC_FIN          = "quiteSystem/monitorRuido/terminoEstadoRojo";
const char* TOPIC_REINICIO     = "quiteSystem/monitorRuido/reinicio";
const char* TOPIC_SENSOR       = "quiteSystem/monitorRuido/valorSensor";
const char* TOPIC_CONFIG_GRAFICO = "quiteSystem/monitorRuido/configuracionGrafico";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// --- Temporizadores de reconexión ---
unsigned long ultimaReconexionWiFi = 0;
unsigned long ultimaReconexionMQTT = 0;
const unsigned long INTERVALO_RECONEXION_MS = 10000;

// --- Callback MQTT: recibe la orden de reinicio remoto ---
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Orden de reinicio recibida en el topic: ");
  Serial.println(topic);

  lcd_1.clear();
  lcd_1.setCursor(0, 0);
  lcd_1.print("Reiniciando...");

  delay(1000);
  ESP.restart();
}

// --- Mantiene WiFi y MQTT conectados, con reintentos no bloqueantes ---
void verificarConexiones() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultimaReconexionWiFi > INTERVALO_RECONEXION_MS) {
      Serial.println("Intentando conectar a WiFi...");
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      ultimaReconexionWiFi = millis();
    }
    return;
  }

  if (!mqttClient.connected()) {
    if (millis() - ultimaReconexionMQTT > INTERVALO_RECONEXION_MS) {
      Serial.println("Intentando conectar a MQTT...");
      if (mqttClient.connect(MQTT_CLIENT)) {
        Serial.println("MQTT Conectado");
        mqttClient.subscribe(TOPIC_REINICIO);

        String datosConfig = String(UMBRAL_ADECUADO) + "," + String(UMBRAL_MAXIMO);
        if (mqttClient.publish(TOPIC_CONFIG_GRAFICO, datosConfig.c_str())) {
          Serial.println("Configuración publicada con éxito.");
        } else {
          Serial.println("Error al publicar la configuración.");
        }
      } else {
        Serial.print("Fallo en conexión MQTT, rc=");
        Serial.println(mqttClient.state());
      }
      ultimaReconexionMQTT = millis();
    }
  }
}

void setup() {
  Serial.begin(115200);

  lcd_1.init();
  lcd_1.backlight();

  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AZUL, OUTPUT);

  pinMode(PIN_SENSOR_SONIDO_A0, INPUT);
  pinMode(PIN_SENSOR_SONIDO_D0, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(callback);
}

// --- Calcula la amplitud del sonido dentro de una ventana de muestreo ---
int obtenerAmplitudSonido() {
  int maxVal = 0;
  int minVal = 4095;
  unsigned long inicioVentana = millis();

  // Ventana de muestreo corta y bloqueante (50 ms) para capturar
  // el pico de amplitud de la señal de audio.
  while (millis() - inicioVentana < DURACION_VENTANA_MUESTREO_MS) {
    int lectura = analogRead(PIN_SENSOR_SONIDO_A0);
    if (lectura > maxVal) maxVal = lectura;
    if (lectura < minVal) minVal = lectura;
  }

  int amplitudActual = maxVal - minVal;

  // Filtro exponencial simple para suavizar la lectura
  amplitudSuavizada = (0.4 * amplitudActual) + (0.6 * amplitudSuavizada);

  return (int)amplitudSuavizada;
}

// --- Lógica principal: lee el sensor, actualiza LCD/LEDs y publica alertas ---
void monitorearRuido() {
  const unsigned long TIEMPO_ESTABILIZACION_MS = 2000;
  if (millis() < TIEMPO_ESTABILIZACION_MS) {
    return;
  }

  if (millis() - ultimaLecturaSensor < INTERVALO_LECTURA_MS) {
    return;
  }
  ultimaLecturaSensor = millis();

  int nivelRuido = obtenerAmplitudSonido();

  Serial.print("Nivel de Ruido (Amplitud A0): ");
  Serial.println(nivelRuido);

  if (mqttClient.connected()) {
    if (!mqttClient.publish(TOPIC_SENSOR, String(nivelRuido).c_str())) {
      Serial.println("Aviso: Fallo al publicar el nivel de ruido.");
    }
  }

  lcd_1.setCursor(0, 0);

  if (nivelRuido < UMBRAL_MAXIMO) {
    if (nivelRuido < UMBRAL_ADECUADO) {
      lcd_1.print("Volumen adecuado");
      lcd_1.setCursor(0, 1);
      lcd_1.print("                ");
      digitalWrite(PIN_LED_VERDE, HIGH);
      digitalWrite(PIN_LED_ROJO, LOW);
      digitalWrite(PIN_LED_AZUL, LOW);
    } else {
      lcd_1.print("Cuidado con el  ");
      lcd_1.setCursor(0, 1);
      lcd_1.print("volumen         ");
      digitalWrite(PIN_LED_VERDE, HIGH);
      digitalWrite(PIN_LED_ROJO, HIGH);
      digitalWrite(PIN_LED_AZUL, LOW);
    }

    if (mensajeEnviado && mqttClient.connected()) {
      if (mqttClient.publish(TOPIC_FIN, "Termino de Estado Rojo")) {
        mensajeEnviado = false;
      } else {
        Serial.println("Error al publicar término de alerta.");
      }
    }
    estadoAlerta = false;
    contandoTiempo = false;

  } else {
    lcd_1.print("Silencio        ");
    lcd_1.setCursor(0, 1);
    lcd_1.print("por favor       ");
    digitalWrite(PIN_LED_VERDE, LOW);
    digitalWrite(PIN_LED_ROJO, HIGH);
    digitalWrite(PIN_LED_AZUL, LOW);

    if (!estadoAlerta) {
      if (!contandoTiempo) {
        tiempoInicioRojo = millis();
        contandoTiempo = true;
      } else if (millis() - tiempoInicioRojo >= TIEMPO_MINIMO_ALERTA_MS) {
        if (!mensajeEnviado && mqttClient.connected()) {
          totalAlertasSesion++;
          String msg = "Inicio Estado Rojo. Alerta #" + String(totalAlertasSesion);

          if (mqttClient.publish(TOPIC_INICIO, msg.c_str())) {
            mensajeEnviado = true;
          } else {
            Serial.println("Error al publicar inicio de alerta.");
            totalAlertasSesion--;
          }
        }
        estadoAlerta = true;
        contandoTiempo = false;
      }
    }
  }
}

void loop() {
  verificarConexiones();
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  monitorearRuido();
}
