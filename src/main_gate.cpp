#include <Arduino.h>
#include "ESP32RelayCard.h"

#define PIN_RELAY_TOGGLE     0
#define MQTT_CLIENT_ID "gate2-toggle-1"

CardConfig cfg;
ESP32RelayCard* card = nullptr;

void setup() {
  cfg.apName = "GateConfig",
  cfg.apPassword = "GateConfig123",
  cfg.mqttClientId = "gate-toggle-1",
  cfg.relays = { RelayConfig(PIN_RELAY_TOGGLE, "0", 500) };

  card = new ESP32RelayCard(cfg);
  card->begin();
}

void loop() {
  card->loop();
}