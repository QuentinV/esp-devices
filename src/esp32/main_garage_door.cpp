#include <Arduino.h>
#include "ESP32RelayCard.h"
#include <VL53L0X.h>

#define PIN_LASER_SDA        8
#define PIN_LASER_SCL        6
#define PIN_RELAY_TOGGLE     5
#define MQTT_CLIENT_ID       "garagedoor-toggle-1"

#define LASER_INTERVAL 3000
#define LASER_DETECT_RANGE 500

CardConfig cfg;
ESP32RelayCard* card = nullptr;

unsigned long lastLaserCheck = 0;
bool lastState = false;

uint16_t distance = 0;
VL53L0X sensor;

void publishMqttState(boolean value) {
  //Serial.print("publish state...");

  String deviceId = MQTT_CLIENT_ID;
  String payload = "{";
  payload +=  "\"deviceId\":\"" + deviceId + "\",";
  payload +=  "\"value\":\"" + String(value) + "\"";
  payload += "}";

  card->mqttPublish(payload.c_str());
}

void setup() {
  delay(5000);

  cfg.apName = "GarageDoorConfig",
  cfg.apPassword = "GarageDoorConfig123",
  cfg.mqttClientId = MQTT_CLIENT_ID,
  cfg.relays = { RelayConfig(PIN_RELAY_TOGGLE, "0", 500) };

  card = new ESP32RelayCard(cfg);
  card->webServer().on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
      JsonDocument resp; 
      resp["value"] = lastState; 
      resp["distance"] = distance;
      String out; 
      serializeJson(resp, out);
      request->send(200, "application/json", out);
    }
  );

  Wire.begin(PIN_LASER_SDA, PIN_LASER_SCL, 100000);
  if (!sensor.init()) {
    Serial.println("Failed to detect VL53L0X!");
    while (1);
  }
  
  card->begin();

  sensor.setTimeout(500);
}

void loop() {
  if (!card->loop()) {
    return;
  }

  unsigned long now = millis();
  if (now - lastLaserCheck >= LASER_INTERVAL) {
      lastLaserCheck = now;

      distance = sensor.readRangeSingleMillimeters();
      if (!sensor.timeoutOccurred() ) {
          bool opened = distance <= LASER_DETECT_RANGE;
          bool diff = opened != lastState;
          lastState = opened;
          if (diff) {
            publishMqttState(opened);
          }
          
      }
  }
}