#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "sensors.h"
#include "control.h"

// -------------------- NASTAVENÍ MQTT --------------------
// Sem dej LOKÁLNÍ IP adresu Synology v tvé LAN, NE Tailscale IP.
// Např. 192.168.1.10 – zjistíš v DSM (Síť / Network).
#define MQTT_HOST "10.0.0.66"   // ← UPRAV!
#define MQTT_PORT 1883

// Identifikace klienta a základ topicu
#define MQTT_CLIENT_ID "FermentorBox-1"
#define MQTT_BASE_TOPIC "fermentor"   // výsledný topic: fermentor/status

// --------------------------------------------------------

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

static unsigned long lastPublishMs = 0;
static const unsigned long PUBLISH_INTERVAL_MS = 10000; // 10 s

// Jednoduchá pomocná funkce pro JSON bool
static String boolToJson(bool v) {
  return v ? "true" : "false";
}

static void mqttPublishStatus() {
  const ProgramSettings &p = getActiveProgram();

  float temp = (sensors.temp1 + sensors.temp2) * 0.5f;
  float hum  = sensors.humidity;
  uint32_t up = millis() / 1000;

  String payload = "{";
  payload += "\"programId\":" + String((int)currentProgram) + ",";
  payload += "\"programName\":\"" + String(p.name) + "\",";
  payload += "\"targetTemp\":" + String(p.targetTempC, 2) + ",";
  payload += "\"targetHumidity\":" + String(p.targetHumidity, 2) + ",";
  payload += "\"temp\":" + String(temp, 2) + ",";
  payload += "\"humidity\":" + String(hum, 2) + ",";
  payload += "\"heaterOn\":" + boolToJson(actuators.heaterOn) + ",";
  payload += "\"humidifierOn\":" + boolToJson(actuators.humidifierOn) + ",";
  payload += "\"fanPercent\":" + String(actuators.fanPercent) + ",";
  payload += "\"coolFanOn\":" + boolToJson(actuators.coolFanOn) + ",";
  payload += "\"uptimeSec\":" + String(up);
  payload += "}";

  String topic = String(MQTT_BASE_TOPIC) + "/status";

  if (mqttClient.publish(topic.c_str(), payload.c_str())) {
    Serial.println("[mqtt] Published status");
  } else {
    Serial.println("[mqtt] Publish failed");
  }
}

static void mqttReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    // nemá cenu se snažit, pokud nejsme na WiFi
    return;
  }
  if (mqttClient.connected()) return;

  Serial.print("[mqtt] Connecting to broker ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.print(MQTT_PORT);
  Serial.println(" ...");

  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    Serial.println("[mqtt] Connected to MQTT");
    // případné subscribe by šlo sem
  } else {
    Serial.print("[mqtt] Failed, rc=");
    Serial.println(mqttClient.state());
  }
}

void initMQTT() {
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  Serial.println("[mqtt] MQTT client initialized");
}

void mqttLoop() {
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (mqttClient.connected() && (now - lastPublishMs > PUBLISH_INTERVAL_MS)) {
    lastPublishMs = now;
    mqttPublishStatus();
  }
}
