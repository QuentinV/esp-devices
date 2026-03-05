# ESP32CardLib

Reusable PlatformIO library for ESP32-C3 IoT cards.  
Drop it in any project's `lib/` folder and define your card in a single config struct.

---

## Features

| Feature  | Details                                              |
| -------- | ---------------------------------------------------- |
| WiFi     | WiFiManager captive-portal, persisted credentials    |
| REST API | ESPAsyncWebServer, extensible via `card.webServer()` |
| Relays   | N relays, pulse or latching, named + indexed routes  |
| MQTT     | PubSubClient, auto-reconnect, config saved in NVS    |
| NeoPixel | Status LED (green=boot, blue=ready, red=error)       |

---

## Project layout

```
your-project/
├── lib/
│   └── ESP32CardLib/
│       ├── library.json
│       └── src/
│           ├── ESP32CardLib.h
│           └── ESP32CardLib.cpp
├── src/
│   └── main.cpp          ← your firmware
└── platformio.ini
```

---

## Minimal usage

```cpp
#include "ESP32CardLib.h"

CardConfig cfg {
    .apName        = "MyCard-Config",
    .apPassword    = "MyCard123",
    .mqttClientId  = "my-card-1",
    .neoPixelPin   = 10,
    .neoPixelCount = 1,
    .relays = {
        { 0, "gate",  500 },   // pin 0, name "gate",  500 ms pulse
        { 1, "light",   0 },   // pin 1, name "light", latching (no pulse)
    }
};

ESP32Card card(cfg);

void setup() { card.begin(); }
void loop()  { card.loop();  }
```

---

## REST endpoints (auto-generated)

```
GET  /                  → help text listing all routes
GET  /reset/wifi        → clear WiFi config and restart
POST /toggle/<index>    → trigger relay by index (0-based)
POST /toggle/<name>     → trigger relay by name
POST /config/mqtt       → configure MQTT (JSON body below)
```

**MQTT config body:**

```json
{ "server": "192.168.1.10", "port": 1883, "topic": "home/card" }
```

Settings are persisted in NVS — survive reboots.

---

## Adding custom routes

```cpp
card.webServer().on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"uptime\":123}");
});

card.begin(); // call begin() AFTER registering extra routes
```

---

## MQTT message callback

```cpp
card.onMqttMessage([](const char* topic, const char* payload) {
    Serial.printf("msg on %s: %s\n", topic, payload);
});
```

---

## RelayConfig fields

```cpp
struct RelayConfig {
    uint8_t      pin;         // GPIO pin
    const char*  name;        // URL-friendly name (e.g. "gate")
    unsigned long pulseMs;    // 0 = latching, >0 = pulse duration in ms
};
```

---

## platformio.ini dependencies

```ini
lib_deps =
    tzapu/WiFiManager @ ^2.0.17
    ESP Async WebServer
    AsyncTCP
    knolleary/PubSubClient @ ^2.8
    bblanchon/ArduinoJson @ ^7.0.0
    adafruit/Adafruit NeoPixel @ ^1.12.0
```
