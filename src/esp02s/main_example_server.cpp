#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

ESP8266WebServer server(80);

void setCORSHeaders() {
    server.sendHeader("Access-Control-Allow-Origin",  "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendJSON(int code, const String& payload) {
    setCORSHeaders();
    server.send(code, "application/json", payload);
}

void connectWiFi() {
    WiFiManager wm;
    wm.autoConnect("ESP02S-Setup");
    Serial.println(WiFi.localIP());
}

void welcome() {
    sendJSON(200, "{\"message\":\"Hello!\"}");
}

void handleOTA() {
    if (!server.hasArg("url") || server.arg("url").length() == 0) {
        sendJSON(400, "{\"error\":\"Missing ?url= parameter\"}"); return;
    }
    String url = server.arg("url");
    Serial.printf("[OTA] Updating from: %s\n", url.c_str());
    sendJSON(200, "{\"message\":\"OTA started\",\"url\":\"" + url + "\"}");
    delay(200);

    WiFiClient client;
    ESPhttpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, url);
    if (ret == HTTP_UPDATE_FAILED)
        Serial.printf("[OTA] Failed (%d): %s\n",
            ESPhttpUpdate.getLastError(),
            ESPhttpUpdate.getLastErrorString().c_str());
}


void setup() {
    Serial.begin(115200);
    
    delay(2000);

    connectWiFi();
   
    server.on("/api/welcome", HTTP_GET, welcome);
    server.on("/api/ota", HTTP_GET, handleOTA);

    server.onNotFound([](){
        server.send(404, "application/json", "{\"error\":\"Not found\"}");
    });

    server.begin();
    Serial.printf("[HTTP] http://%s:%d\n", WiFi.localIP().toString().c_str(), 80);
}

void loop() {
    server.handleClient();
}