#include <Arduino.h>
#include "ESP32RelayCard.h"

#define PIN_RELAY_TOGGLE_1     2
#define PIN_RELAY_TOGGLE_2     3
#define PIN_RELAY_TOGGLE_3     4
#define PIN_RELAY_TOGGLE_4     5

CardConfig cfg;
ESP32RelayCard* card = nullptr;


void setup() {
  cfg.apName = "RelayConfig-1",
  cfg.apPassword = "RelayConfig123",
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