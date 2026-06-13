#include <Arduino.h>
#include "ESP32RelayCard.h"

#define PIN_RELAY_TOGGLE     0

CardConfig cfg;
ESP32RelayCard* card = nullptr;

void setup() {
  cfg.apName = "RelayConfig",
  cfg.apPassword = "RelayConfig123",
  cfg.mqttClientId = "relay-toggle-1",
  cfg.relays = { RelayConfig(PIN_RELAY_TOGGLE, "0", 500) };

  card = new ESP32RelayCard(cfg);
  card->begin();
}

void loop() {
  card->loop();
}