# ESP devices

Multi scripts for different type of devices example.
Include an ESP32 relay card library to be reused among devices with relays.

## ESP32RelayCard library

Reusable PlatformIO library for ESP32-C3 IoT cards.  
Drop it in any project's `lib/` folder and define your card in a single config struct.

---

### Features

| Feature  | Details                                              |
| -------- | ---------------------------------------------------- |
| WiFi     | WiFiManager captive-portal, persisted credentials    |
| REST API | ESPAsyncWebServer, extensible via `card->webServer()` |
| Relays   | N relays, pulse or latching, named routes  |
| MQTT     | PubSubClient, auto-reconnect, config saved in NVS    |
| NeoPixel | Status LED (green=boot, blue=ready, red=error)       |

### Minimal usage

```cpp
#include "ESP32CardLib.h"
#define PIN_RELAY_TOGGLE 1

CardConfig cfg;
ESP32RelayCard* card = nullptr;

void setup() {
  cfg.apName = "ESP32Config",
  cfg.apPassword = "ESP32Config123",
  cfg.mqttClientId = "client-toggle-1",
  cfg.relays = { RelayConfig(PIN_RELAY_TOGGLE, "0", 500) };

  card = new ESP32RelayCard(cfg);
  card->begin();
}

void loop() {
  card->loop();
}
```

### REST endpoints (auto-generated)

```
GET  /                  → help text listing all routes
GET  /reset/wifi        → clear WiFi config and restart
POST /toggle/<name>     → trigger relay by name
POST /config/mqtt       → configure MQTT (JSON body below)
```