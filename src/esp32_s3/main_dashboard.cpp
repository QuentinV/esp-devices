/*
 * main_dashboard.cpp
 * ESP32-S3 smart-mirror dashboard on a 7-pin SPI IPS driven as 480x320 ILI9488.
 *
 * Features:
 *  - WiFiManager captive portal for WiFi credentials
 *  - MQTT (PubSubClient): subscribes  <prefix>/#  ; renders MOCK data until MQTT
 *    connects and real data starts arriving, then LIVE
 *  - REST API (ESPAsyncWebServer) to configure the MQTT broker (saved in NVS)
 *  - SNTP real-time clock (built-in, no extra lib)
 *  - Flicker-free partial rendering (opaque text + small region updates)
 *
 * Flash:  pio run -e esp32-dashboard -t upload
 * Help:   GET http://<device-ip>/
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// ---------------- config ----------------
#define AP_NAME     "SmartMirror-Config"
#define AP_PASS     "SmartMirror123"
#define MQTT_ID     "smartmirror"
#define MQTT_PREFIX "smartmirror"        // default topic base (subs <prefix>/#)

// fonts
#define F1 1   // 6x8
#define F2 2   // 16px
#define F4 4   // 26px
#define F6 6   // 48px

// colours - smart mirror: black bg, white/grey text, coloured accents
#define C_BG    0x0000
#define C_TXT   0xFFFF
#define C_TITLE 0xBDF7
#define C_ACC   0x07FF   // cyan
#define C_OK    0x07E0   // green
#define C_WARN  0xFD20   // orange
#define C_ERR   0xF800   // red
#define C_SUN   0xFFE0   // yellow
#define C_CLOUD 0xC618
#define C_RAIN  0x4DFB

#define WEATHER_X0 8
#define INDOOR_X0  248

// ---------------- system ----------------
Preferences      prefs;
WiFiManager      wm;
AsyncWebServer   server(80);
WiFiClient       espClient;
PubSubClient     mqtt(espClient);

// ---------------- MQTT config (persisted in NVS) ----------------
String  mqServer = "", mqUser = "", mqPass = "", mqPrefix = MQTT_PREFIX;
int     mqPort   = 1883;

// ---------------- dashboard data ----------------
bool    anyRealData = false;   // true once any real MQTT value has arrived
bool    wDirty = false, iDirty = false, clockDirty = true;
bool    ntpReady = false;
bool    wifiOK = false;

String  wCond = "sun", wTemp = "--", wHum = "--", wWind = "--";
String  iTemp = "--",  iHum  = "--", iCo2  = "--";
String  tickerMsg = "connecting to network...";

// timers
unsigned long tClock = 0, tTicker = 0, lastMqtt = 0;
long mockSecs = 9 * 3600;   // demo 09:00 until NTP sync provides real time

// === PART2 ===
// ---------------------------------------------------------------- time -------
void buildTimeStr(char* out) {
  long s = ntpReady ? (long)(time(nullptr) % 86400L) : mockSecs;
  if (s < 0) s += 86400L;
  int hh = (int)((s / 3600L) % 24), mm = (int)((s % 3600L) / 60);
  sprintf(out, "%02d:%02d", hh, mm);
}

void buildDateStr(char* out) {
  if (!ntpReady) { strcpy(out, "SMART MIRROR"); return; }
  time_t tn = time(nullptr);
  struct tm* t = localtime(&tn);
  const char* wd[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday",
                      "Friday","Saturday"};
  const char* mo[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug",
                      "Sep","Oct","Nov","Dec"};
  sprintf(out, "%s %02d %s", wd[t->tm_wday], t->tm_mday, mo[t->tm_mon]);
}

// ---------------------------------------------------------------- drawing ----
void drawTitle(const char* s, int x, int y, int w) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_TITLE, C_BG);
  tft.drawString(s, x, y, F2);
  tft.drawFastHLine(x, y + 20, w, C_TITLE);
}

// Simple weather icon: sun / cloud / rain (drawn from primitives)
void drawWeatherIcon(int cx, int cy, int r, const char* cond) {
  String c = cond; c.toLowerCase();
  if (c.indexOf("rain") >= 0 || c.indexOf("shower") >= 0) {
    tft.fillCircle(cx - r / 2, cy - r / 4, r / 2, C_CLOUD);
    tft.fillCircle(cx + r / 2, cy - r / 4, r / 2, C_CLOUD);
    tft.fillCircle(cx, cy, r / 2, C_CLOUD);
    for (int i = -2; i <= 2; i++)            // rain streaks
      tft.drawFastVLine(cx + i * (r / 3), cy + r / 3, r / 2, C_RAIN);
  } else if (c.indexOf("cloud") >= 0 || c.indexOf("overcast") >= 0) {
    tft.fillCircle(cx - r / 2, cy, r / 2, C_CLOUD);
    tft.fillCircle(cx + r / 2, cy, r / 2, C_CLOUD);
    tft.fillCircle(cx, cy + r / 4, r / 2, C_CLOUD);
  } else {                                   // sun
    tft.fillCircle(cx, cy, r, C_SUN);
    for (int a = 0; a < 360; a += 30) {     // sun rays
      float rad = a * 3.14159f / 180.0f;
      int x1 = cx + (int)(r * 1.1f * cosf(rad));
      int y1 = cy + (int)(r * 1.1f * sinf(rad));
      int x2 = cx + (int)(r * 1.5f * cosf(rad));
      int y2 = cy + (int)(r * 1.5f * sinf(rad));
      tft.drawLine(x1, y1, x2, y2, C_SUN);
    }
  }
}

// ---------------------------------------------------------------- render -----
void renderClock() {
  char buf[40];
  buildTimeStr(buf);
  tft.setTextColor(C_TXT, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(4, 4, 210, 104, C_BG);        // clear header-left region
  tft.drawString(buf, 16, 12, F6);           // big HH:MM

  tft.setTextColor(C_TITLE, C_BG);
  buildDateStr(buf);
  tft.drawString(buf, 16, 66, F2);

  // status chip (top-right)
  String st;
  uint16_t col;
  if (mqServer.length() == 0)      { st = "SETUP"; col = C_WARN; }
  else if (!mqtt.connected())      { st = "MOCK";  col = C_WARN; }
  else if (!anyRealData)           { st = "LIVE.."; col = C_ACC; }
  else                             { st = "LIVE";  col = C_OK; }
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(col, C_BG);
  tft.drawString(st.c_str(), 476, 10, F2);
  tft.setTextColor(C_TITLE, C_BG);
  tft.drawString(WiFi.localIP().toString().c_str(), 476, 34, F1);
  tft.setTextDatum(TL_DATUM);
}

void renderWeather() {
  drawWeatherIcon(58, 168, 16, wCond.c_str());
  tft.setTextColor(C_TXT, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(wCond, 12, 205, F2);
  tft.drawString(wTemp, 150, 175, F4);       // e.g. 21°C
  tft.drawString("Hum  " + wHum + "%",  150, 238, F2);
  tft.drawString("Wind " + wWind,       150, 264, F2);
}

void renderIndoor() {
  tft.setTextColor(C_TXT, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(iTemp, INDOOR_X0 + 4, 175, F4);
  tft.drawString("Hum  " + iHum + "%",    INDOOR_X0 + 4, 238, F2);
  tft.drawString("CO2  " + iCo2 + "ppm", INDOOR_X0 + 4, 264, F2);
}

void renderTicker() {
  int w = tft.textWidth(tickerMsg.c_str(), F1);
  int maxX = w + 480;
  int x = 480 - (int)(millis() / 30 % (unsigned long)maxX);
  tft.fillRect(0, 305, 480, 14, C_BG);      // clear strip
  tft.setTextColor(C_ACC, C_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(tickerMsg.c_str(), x, 306, F1);
}

// === PART3 ===
// ---------------------------------------------------------------- WiFi --------
void setupWifi() {
  WiFi.mode(WIFI_AP_STA);
  wm.setConfigPortalBlocking(true);
  wm.setDebugOutput(true);
  if (!wm.autoConnect(AP_NAME, AP_PASS)) {
    Serial.println("[WiFi] failed - restarting");
    ESP.restart();
  }
  wifiOK = true;
  Serial.printf("[WiFi] connected, IP: %s\n", WiFi.localIP().toString().c_str());
  configTime(0, 0, "pool.ntp.org");   // built-in SNTP (real clock)
}

// ---------------------------------------------------------------- MQTT --------
void mqttReconnect() {
  if (mqServer.length() == 0) return;
  mqtt.setServer(mqServer.c_str(), mqPort);
  Serial.printf("[MQTT] connecting to %s:%d ...\n", mqServer.c_str(), mqPort);
  bool ok = mqUser.length()
              ? mqtt.connect(MQTT_ID, mqUser.c_str(), mqPass.c_str())
              : mqtt.connect(MQTT_ID);
  if (ok) {
    Serial.println("[MQTT] connected");
    String sub = mqPrefix + "/#";
    mqtt.subscribe(sub.c_str());
    Serial.printf("[MQTT] subscribed %s\n", sub.c_str());
  } else {
    Serial.printf("[MQTT] failed rc=%d\n", mqtt.state());
  }
}

void onMqttMessage(char* topic, byte* payload, unsigned int len) {
  String full(topic);
  int slash = full.indexOf('/');             // "prefix/..."
  String seg = (slash >= 0) ? full.substring(slash + 1) : full;
  String val = "";
  if (len > 0 && payload[0] == '{') {        // JSON  {"value": ...}
    JsonDocument doc;
    if (deserializeJson(doc, payload, len) == DeserializationError::Ok) {
      JsonVariant v = doc["value"];
      if (!v.isNull()) {
        if (v.is<float>()) val = String(v.as<float>(), 2);
        else val = v.as<String>();
      }
    }
  } else if (len > 0) {
    val = String((const char*)payload).substring(0, len);
  }
  if (val.length() == 0) return;

  anyRealData = true;
  if (seg == "message") { tickerMsg = val; return; }

  int p = seg.indexOf('/');
  String group = (p >= 0) ? seg.substring(0, p) : seg;
  String key   = (p >= 0) ? seg.substring(p + 1) : "";
  if      (group == "weather") { wDirty = true;
    if (key == "temp")     wTemp = val;
    else if (key == "humidity")  wHum  = val;
    else if (key == "condition") wCond = val;
    else if (key == "wind")      wWind = val;
  }
  else if (group == "indoor") { iDirty = true;
    if (key == "temp")     iTemp = val;
    else if (key == "humidity")  iHum  = val;
    else if (key == "co2")       iCo2  = val;
  }
}

void loadMqttConfig() {
  prefs.begin("mirror", true);
  mqServer = prefs.getString("server", "");
  mqPort   = prefs.getInt("port", 1883);
  mqPrefix = prefs.getString("topic", MQTT_PREFIX);
  mqUser   = prefs.getString("user", "");
  mqPass   = prefs.getString("pass", "");
  prefs.end();
  if (mqServer.length()) {
    mqtt.setCallback(onMqttMessage);
    mqttReconnect();
  }
}

// ---------------------------------------------------------------- REST --------
void handleConfigMqtt(AsyncWebServerRequest* req, uint8_t* data, size_t len,
                      size_t index, size_t total) {
  (void)index; (void)total;
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) {
    req->send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }
  const char* srv   = doc["server"] | "";
  int         port  = doc["port"]   | 1883;
  const char* topic = doc["topic"]  | "";
  const char* user_ = doc["user"]   | "";
  const char* pass_ = doc["pass"]   | "";

  prefs.begin("mirror", false);
  prefs.putString("server", srv);
  prefs.putInt("port", port);
  prefs.putString("topic", topic);
  prefs.putString("user", user_);
  prefs.putString("pass", pass_);
  prefs.end();

  mqServer = String(srv);
  mqPort = port;
  mqPrefix = String(topic);
  mqUser = String(user_);
  mqPass = String(pass_);
  mqttReconnect();
  req->send(200, "application/json", "{\"ok\":true}");
}

void setupWeb() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    String h = "Smart Mirror REST API\n\n";
    h += "GET  /             help\n";
    h += "GET  /status       dashboard values + status (JSON)\n";
    h += "POST /config/mqtt  {server,port,topic,user,pass}\n";
    h += "GET  /reset/wifi   clear WiFi + restart\n";
    req->send(200, "text/plain", h);
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument d;
    d["wifi"] = wifiOK;
    d["ip"] = WiFi.localIP().toString();
    d["mqtt"]["configured"] = mqServer.length() > 0;
    d["mqtt"]["server"] = mqServer;
    d["mqtt"]["port"] = mqPort;
    d["mqtt"]["connected"] = mqtt.connected();
    d["mock"] = !anyRealData;
    d["weather"]["temp"] = wTemp;
    d["weather"]["humidity"] = wHum;
    d["weather"]["condition"] = wCond;
    d["weather"]["wind"] = wWind;
    d["indoor"]["temp"] = iTemp;
    d["indoor"]["humidity"] = iHum;
    d["indoor"]["co2"] = iCo2;
    d["message"] = tickerMsg;
    char buf[1024];
    serializeJson(d, buf);
    req->send(200, "application/json", buf);
  });

  server.on("/config/mqtt", HTTP_POST,
    [](AsyncWebServerRequest* req) {},    // dummy
    nullptr,
    handleConfigMqtt);

  server.on("/reset/wifi", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", "resetting wifi...");
    wm.resetSettings();
    ESP.restart();
  });

  server.begin();
  Serial.println("[HTTP] server started");
  Serial.printf("[HTTP] point a browser at http://%s/\n",
                WiFi.localIP().toString().c_str());
}

// === PART4 ===
// ---------------------------------------------------------------- UI ----------
void setupStatic() {
  drawTitle("WEATHER", 12, 120, 150);            // left panel title
  drawTitle("INDOOR",  INDOOR_X0 + 4, 120, 150); // right panel title
  tft.drawFastVLine(240, 124, 174, C_TITLE);     // divider y124..298
  tft.drawFastHLine(0, 303, 480, C_TITLE);       // ticker top edge
}

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("[0] boot");

  tft.init();
  tft.setRotation(1);               // landscape 480 x 320
  tft.fillScreen(C_BG);
  setupStatic();
  Serial.println("[1] tft ready");

  // draw the mock dashboard immediately (also shows while WiFi connects)
  renderWeather();
  renderIndoor();
  renderTicker();
  renderClock();
  Serial.println("[2] mock rendered");

  Serial.println("[3] wifi manager...");
  setupWifi();       // blocks until connected (or reboots)
  Serial.println("[4] wifi connected");

  loadMqttConfig();  // loads broker from NVS + connects if configured
  Serial.println("[5] mqtt configured");

  setupWeb();        // REST API
  Serial.println("[6] web server started");

  Serial.println("[dash] ready");
}

void loop() {
  uint32_t now = millis();

  if (mqServer.length()) {                 // MQTT keep-alive
    if (!mqtt.connected()) {
      if (now - lastMqtt > 5000) { lastMqtt = now; mqttReconnect(); }
    }
    mqtt.loop();
  }

  if (!ntpReady && wifiOK && time(nullptr) > 5000) ntpReady = true;

  if (now - tClock >= 1000) {
    tClock = now;
    if (!ntpReady) mockSecs++;             // ticking mock clock until NTP
    renderClock();
  }
  if (now - tTicker >= 120) { tTicker = now; renderTicker(); }
  if (wDirty) { wDirty = false; renderWeather(); }
  if (iDirty) { iDirty = false; renderIndoor(); }
}