#include <Arduino.h>
#include "ESP32RelayCard.h"

#define PIN_RELAY_TOGGLE_1     0
#define PIN_RELAY_TOGGLE_2     1
#define PIN_RELAY_TOGGLE_3     2
#define PIN_RELAY_TOGGLE_4     3
#define MQTT_CLIENT_ID        "relays-toggle-1"

CardConfig cfg;
ESP32RelayCard* card = nullptr;

void setup() {
  cfg.apName = "RelayConfig-1",
  cfg.apPassword = "RelayConfig123",
  cfg.mqttClientId = MQTT_CLIENT_ID,
  cfg.relays = { 
    RelayConfig(PIN_RELAY_TOGGLE_1, "0", 0), 
    RelayConfig(PIN_RELAY_TOGGLE_2, "1", 0), 
    RelayConfig(PIN_RELAY_TOGGLE_3, "2", 0), 
    RelayConfig(PIN_RELAY_TOGGLE_4, "3", 0) 
  };

  card = new ESP32RelayCard(cfg);
  card->begin();
}

void loop() {
  card->loop();
}