#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <functional>
#include <vector>

struct RelayConfig {
    uint8_t      pin;
    const char*  name;
    unsigned long pulseMs = 500;

    RelayConfig(uint8_t pin, const char* name, unsigned long pulseMs = 500)
        : pin(pin), name(name), pulseMs(pulseMs) {}
};

struct CardConfig {
    // WiFi / portal
    const char* apName     = "ESP32Card";
    const char* apPassword = "ESP32Card123";

    // MQTT
    const char* mqttClientId = "esp32-card";

    // NeoPixel
    uint8_t neoPixelPin    = 10;
    uint8_t neoPixelCount  = 1;
    uint8_t neoPixelBright = 10;

    // Relays
    std::vector<RelayConfig> relays;
};

class ESP32RelayCard {
public:
    explicit ESP32RelayCard(const CardConfig& cfg);

    // Call once in setup()
    void begin();

    bool loop();

    void setLed(uint8_t r, uint8_t g, uint8_t b);

    // ── MQTT ────────────────────────────────
    bool mqttConnected() const;
    bool mqttPublish(const char* payload);

    // Provide your own MQTT message callback (optional)
    using MqttCallback = std::function<void(const char* topic, const char* payload)>;
    void onMqttMessage(MqttCallback cb);

    // ── REST extension point ─────────────────
    // Register extra routes BEFORE calling begin()
    AsyncWebServer& webServer() { return _server; }

private:
    void _setupWiFi();
    void _setupRelays();
    void _setupWebServer();
    void _setupMqtt();

    // MQTT
    void _mqttReconnect();
    static void _mqttStaticCallback(char* topic, byte* payload, unsigned int len);

    // Relay
    void _triggerRelay(const RelayConfig& relay, int state);
    RelayConfig* _findRelay(String relayId);

    // State
    CardConfig          _cfg;
    Preferences         _prefs;
    WiFiManager         _wm;
    AsyncWebServer      _server;
    WiFiClient          _espClient;
    PubSubClient        _mqtt;
    Adafruit_NeoPixel*  _pixels = nullptr;

    MqttCallback        _mqttCb;

    // MQTT connection settings (persisted in NVS)
    String _mqttServer;
    int    _mqttPort = 1883;
    String _mqttTopic;

    unsigned long _lastMqttAttempt = 0;

    // Singleton pointer for static MQTT callback
    static ESP32RelayCard* _instance;
};
