/*
 * diag_screen.cpp - TEMPORARY display diagnostics for the ST7789V 2.8" 240x320.
 * Cycles rotations 0-3. For each rotation:
 *   - solid RED / GREEN / BLUE / WHITE full-screen fills (tests colour order
 *     RGB-vs-BGR AND whether the whole panel fills solid / only part).
 *   - a frame pattern: white border, corner markers + diagonal + "R<n>" label
 *     (tests scan direction / where pixels land).
 * Watch the screen and report:
 *   - which rotation number looks upright / correct scan direction
 *   - whether each solid colour actually looks like that colour (red=red etc.)
 *   - whether the WHOLE panel turns solid, or only a portion stays banded
 * Everything is also logged on serial (115200).
 *
 * Flash: pio run -e esp32-screen -t upload
 */
#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

const uint16_t SOLIDS[4]     = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE };
const char*    PRI_NAMES[4]  = { "RED", "GREEN", "BLUE", "WHITE" };

void drawFramePattern(uint8_t r) {
  int16_t w = tft.width();
  int16_t h = tft.height();
  tft.fillScreen(TFT_BLACK);
  tft.drawRect(0, 0, w, h, TFT_WHITE);                       // border
  tft.fillRect(4, 4, 12, 12, TFT_RED);                        // TL
  tft.fillRect(w - 16, 4, 12, 12, TFT_GREEN);                 // TR
  tft.fillRect(4, h - 16, 12, 12, TFT_BLUE);                  // BL
  tft.fillRect(w - 16, h - 16, 12, 12, TFT_YELLOW);           // BR
  tft.drawLine(0, 0, w - 1, h - 1, TFT_WHITE);                // diag
  tft.drawLine(w - 1, 0, 0, h - 1, TFT_WHITE);                // diag
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(4);
  tft.setCursor(w / 2 - 24, h / 2 - 14);
  tft.print("R");
  tft.print(r);
}

void setup() {
  Serial.begin(115200);
  delay(10);
  tft.init();
  Serial.println("diag_start");
}

void loop() {
  for (uint8_t r = 0; r < 4; r++) {
    tft.setRotation(r);
    int16_t w = tft.width();
    int16_t h = tft.height();
    Serial.printf("rotation=%d size=%dx%d\n", (int)r, (int)w, (int)h);

    for (int c = 0; c < 4; c++) {
      tft.fillScreen(SOLIDS[c]);
      tft.setTextColor(TFT_BLACK, SOLIDS[c]);
      tft.setTextSize(2);
      tft.setCursor(w / 2 - 34, h / 2 - 8);
      tft.print("R");
      tft.print(r);
      tft.print(" ");
      tft.print(PRI_NAMES[c]);
      Serial.printf("  fill %s\n", PRI_NAMES[c]);
      delay(1600);
    }

    drawFramePattern(r);
    Serial.println("  frame pattern");
    delay(3000);
  }
  Serial.println("cycle_done");
}