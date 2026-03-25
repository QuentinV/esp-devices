#include "ESP32RelayCard.h"
#include <esp_wifi.h>

ESP32RelayCard* ESP32RelayCard::_instance = nullptr;

ESP32RelayCard::ESP32RelayCard(const CardConfig& cfg)
    : _cfg(cfg), _server(80), _mqtt(_espClient)
{
    _instance = this;
}

void ESP32RelayCard::begin() {
    Serial.begin(115200);

    // NeoPixel init
    _pixels = new Adafruit_NeoPixel(
        _cfg.neoPixelCount, _cfg.neoPixelPin, NEO_GRB + NEO_KHZ800);
    _pixels->begin();
    _pixels->setBrightness(_cfg.neoPixelBright);
    setLed(0, 255, 0); // green = booting

    _setupRelays();
    _setupWiFi();       // blocks until connected (or reboots)
    _setupMqtt();
    _setupWebServer();

    setLed(0, 0, 255); // blue = ready
}

bool ESP32RelayCard::loop() {
     if (_mqttServer.length() == 0) {
        delay(60000);
        return false;
    }
    
    if (_mqtt.connected()) {
        _mqtt.loop();
        delay(50);
        return true;
    }
    
    unsigned long now = millis();
    if (now - _lastMqttAttempt > 5000) {
        _lastMqttAttempt = now;
        _mqttReconnect();
    }

    return false;
}

void ESP32RelayCard::setLed(uint8_t r, uint8_t g, uint8_t b) {
    if (!_pixels) return;
    _pixels->setPixelColor(0, _pixels->Color(r, g, b));
    _pixels->show();
}

bool ESP32RelayCard::mqttPublish(const char* payload) {
    if (!_mqtt.connected()) return false;
    return _mqtt.publish(_mqttTopic.c_str(), payload);
}

void ESP32RelayCard::_setupRelays() {
    for (auto& r : _cfg.relays) {
        pinMode(r.pin, OUTPUT);
        digitalWrite(r.pin, LOW);
    }
}

void ESP32RelayCard::_setupWiFi() {
    WiFi.mode(WIFI_AP_STA);
    esp_wifi_set_max_tx_power(30);

    _wm.setConfigPortalBlocking(true);
    _wm.setDebugOutput(true);

    bool res = _wm.autoConnect(_cfg.apName, _cfg.apPassword);

    if (!res) {
        setLed(255, 0, 0); // red = error
        delay(3000);
        ESP.restart();
    }
}

void ESP32RelayCard::_setupMqtt() {
    _prefs.begin("mqtt", true);
    _mqttServer = _prefs.getString("server", "");
    _mqttPort   = _prefs.getInt("port", 1883);
    _mqttTopic  = _prefs.getString("topic", "");
    _prefs.end();

    if (_mqttServer.length() > 0) {
        _mqtt.setServer(_mqttServer.c_str(), _mqttPort);
        _mqttReconnect();
    }
}

void ESP32RelayCard::_mqttReconnect() {
    if (_mqttServer.length() == 0) return;
    _mqtt.setServer(_mqttServer.c_str(), _mqttPort);

    Serial.printf("[MQTT] Connecting to %s:%d as %s ...\n",
        _mqttServer.c_str(), _mqttPort, _cfg.mqttClientId);

    if (_mqtt.connect(_cfg.mqttClientId)) {
        Serial.println("[MQTT] Connected");
    } else {
        Serial.printf("[MQTT] Failed, rc=%d\n", _mqtt.state());
    }
}

void ESP32RelayCard::_triggerRelay(const RelayConfig& relay) {
    int read = digitalRead(relay.pin);
    digitalWrite(relay.pin, read == HIGH ? LOW : HIGH);
    if (relay.pulseMs > 0) {
        delay(relay.pulseMs);
        digitalWrite(relay.pin, LOW);
    }
}

void ESP32RelayCard::_setupWebServer() {
    // POST /toggle/<name>
    for (size_t i = 0; i < _cfg.relays.size(); i++) {
        const RelayConfig& relay = _cfg.relays[i];

        // Route by name  (e.g. /toggle/gate)
        String routeName = "/toggle/" + String(relay.name);
        _server.on(routeName.c_str(), HTTP_POST,
            [this, i](AsyncWebServerRequest* req) {
                _triggerRelay(_cfg.relays[i]);
                int read = digitalRead(_cfg.relays[i].pin);
                String state = read == HIGH ? "high" : "low";
                req->send(200, "application/json", "{\"ok\":true, \"state\": \"" + state + "\"}");
            });
    }

    _server.on("/config/mqtt", HTTP_POST,
        [](AsyncWebServerRequest* req) {},          // dummy handler
        nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<256> doc;
            if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            const char* srv   = doc["server"] | "";
            int          port = doc["port"]   | 1883;
            const char* topic = doc["topic"]  | "";

            _prefs.begin("mqtt", false);
            _prefs.putString("server", srv);
            _prefs.putInt("port", port);
            _prefs.putString("topic", topic);
            _prefs.end();

            _mqttServer = srv;
            _mqttPort   = port;
            _mqttTopic  = topic;

            _mqttReconnect();
            req->send(200, "application/json", "{\"ok\":true}");
        });

    _server.on("/reset/wifi", HTTP_GET, [this](AsyncWebServerRequest* req) {
        req->send(200, "text/plain", "Resetting WiFi and restarting...");
        _wm.resetSettings();
        ESP.restart();
    });

    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        String help = "ESP32Card REST API\n\n";
        help += "GET  /             → this help\n";
        help += "GET  /reset/wifi   → reset WiFi and restart\n";
        help += "POST /config/mqtt  → {\"server\":\"\",\"port\":1883,\"topic\":\"\"}\n\n";
        help += "Relays:\n";
        for (size_t i = 0; i < _cfg.relays.size(); i++) {
            help += "  POST /toggle/" + String(_cfg.relays[i].name) + "\n";
        }
        req->send(200, "text/plain", help);
    });

    _server.begin();
    Serial.println("[HTTP] Server started");
}
