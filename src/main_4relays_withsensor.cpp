#include <Arduino.h>
#include "ESP32RelayCard.h"
#include <Adafruit_AHTX0.h> 
#include <Adafruit_BMP280.h>

#define PIN_RELAY_TOGGLE_1     2
#define PIN_RELAY_TOGGLE_2     3
#define PIN_RELAY_TOGGLE_3     4
#define PIN_RELAY_TOGGLE_4     5
#define MQTT_CLIENT_ID        "relays-exterior-sensor"

CardConfig cfg;
ESP32RelayCard* card = nullptr;

Adafruit_AHTX0 aht; 
Adafruit_BMP280 bmp;

unsigned long lastMsg = 0;
const long interval = 60000;

void publishMqttState() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  String payload = "{";
  payload +=  "\"deviceId\":\"" + String(MQTT_CLIENT_ID) + "\",";
  payload +=  "\"value\":\"" + String(temp.temperature) + "\",";
  payload +=  "\"additional\": {\"hum\":\"" + String(humidity.relative_humidity) + "\", \"aht_temp\":\"" + String(temp.temperature) + "\", \"bmp_temp\":\"" + String(bmp.readTemperature()) + "\", \"pressure\":\"" + String(bmp.readPressure() / 100.0F) + "\" }";
  payload += "}";

  card->mqttPublish(payload.c_str());
}


void setup() {
  delay(5000);
  cfg.apName = "RelayConfig-1",
  cfg.apPassword = "RelayConfig123",
  cfg.mqttClientId = MQTT_CLIENT_ID,
  cfg.relays = { 
    RelayConfig(PIN_RELAY_TOGGLE_1, "0", 0), 
    RelayConfig(PIN_RELAY_TOGGLE_2, "1", 0), 
    RelayConfig(PIN_RELAY_TOGGLE_3, "2", 0), 
    RelayConfig(PIN_RELAY_TOGGLE_4, "3", 0) 
  };

  if (!Wire.begin(7, 6)) {
    Serial.println("Not successful wiring");
  }

  if (!aht.begin()) {
    Serial.println("Check AHT wiring!");
  }
  if (!bmp.begin(0x77)) {
    Serial.println("Check BMP280 wiring!");
  }

  card = new ESP32RelayCard(cfg);
  card->webServer().on("/states", HTTP_GET, [](AsyncWebServerRequest *request) {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      JsonDocument resp; 
      resp["hum"] = humidity.relative_humidity;
      resp["aht_temp"] = temp.temperature;
      resp["bmp_temp"] = bmp.readTemperature();
      resp["pressure"] = bmp.readPressure() / 100.0F;

      String out; 
      serializeJson(resp, out);
      request->send(200, "application/json", out);
    }
  );
  card->begin();
}

void loop() {
  if (!card->loop()) {
    return;
  }

  unsigned long now = millis();
  if (now - lastMsg > interval) {
    lastMsg = now;
    publishMqttState();
  }
}