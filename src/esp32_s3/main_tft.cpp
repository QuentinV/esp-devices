/*
 * main_tft.cpp
 * ESP32-S3 + 1.8" 128x160 RGB TFT (ST7735S driver IC), AliExpress module.
 *
 * NOTE: the display is SPI (ST7735S), NOT I2C - despite the "SCL / SDA" module
 * labels those are the SPI clock and data lines. Wiring (module -> GPIO):
 *   SCL (clock) -> 10  (TFT_SCLK)
 *   SDA (data)  ->  9  (TFT_MOSI)
 *   DC          ->  3  (TFT_DC)
 *   CS          ->  8  (TFT_CS)
 *   RST (reset) -> 14  (TFT_RST)  <-- wire module RST to GPIO 14 (floating reset = white screen)
 * Config comes from the [env:esp32-tft] build flags in platformio.ini.
 *
 * What it does: draws one emoji (bit-art from drawing primitives) in the
 * middle of the screen and rotates to the next emoji every 2 seconds.
 * A status bar at the bottom shows the emoji name + frame counter, and the
 * name is also echoed over the serial monitor (115200 baud).
 *
 * If colours look inverted/blue-ish, tweak the ST7735 init tab in the env
 * build flags (ST7735_INITB vs default green tab). If mirrored/offset, change
 * setRotation().
 *
 * Flash with:  pio run -e esp32-tft -t upload
 */

#include <Arduino.h>
#include <TFT_eSPI.h> // config comes from platformio.ini build flags

TFT_eSPI tft = TFT_eSPI();

// ---------------------------------------------------------------------------
// Animation / layout config  (rotation 1 -> landscape 160 x 128)
// ---------------------------------------------------------------------------
#define EMOJI_PERIOD_MS 2000u          // advance to the next emoji every 2 s
#define CLEAR_HALF      34u            // half-size of the box cleared on change

#define SCREEN_W 160
#define SCREEN_H 128

#define HEADER_H 14                    // top header bar height
#define STATUS_H 12                    // bottom status bar height

#define FACE_CX  (SCREEN_W / 2)                                       // 80
#define FACE_CY  ((HEADER_H + SCREEN_H - STATUS_H) / 2)               // 65
#define FACE_R   40                     // base face radius in px

#define NUM_EMOJI 6u

static const char* EMOJI_NAME[NUM_EMOJI] = {
  "HAPPY", "SAD", "SURPRISED", "SLEEPING", "HEART", "STAR"
};

// ---------------------------------------------------------------------------
// RGB565 colours
// ---------------------------------------------------------------------------
#define C_BG     0x18E3   // slate background
#define C_HEAD   0x0010   // header bar (dark navy)
#define C_YELLOW 0xFFE0   // emoji skin
#define C_RED    0xF800
#define C_PINK   0xF81F
#define C_AQUA   0x07FF
#define C_WHITE  0xFFFF
#define C_BLACK  0x0000
// ---------------------------------------------------------------------------
// Emoji bit-art (size-relative so it scales onto the small panel)
// ---------------------------------------------------------------------------
static void drawFaceBase(uint16_t cx, uint16_t cy, uint16_t r) {
  tft.fillCircle(cx, cy, r, C_YELLOW);
}

static void drawHappy(uint16_t cx, uint16_t cy, uint16_t r) {
  drawFaceBase(cx, cy, r);
  uint16_t eo = r / 4;          // horizontal eye offset
  uint16_t ey = cy - r / 6;     // eye height
  tft.fillCircle(cx - eo, ey, r / 10, C_BLACK);
  tft.fillCircle(cx + eo, ey, r / 10, C_BLACK);
  tft.fillCircle(cx, cy + r / 5, r / 2, C_BLACK);          // open smile
  tft.fillCircle(cx - r / 2, cy + r / 6, r / 9, C_PINK);   // blush
  tft.fillCircle(cx + r / 2, cy + r / 6, r / 9, C_PINK);
}

static void drawSad(uint16_t cx, uint16_t cy, uint16_t r) {
  drawFaceBase(cx, cy, r);
  uint16_t eo = r / 4;
  uint16_t ey = cy - r / 6;
  tft.fillCircle(cx - eo, ey, r / 10, C_BLACK);
  tft.fillCircle(cx + eo, ey, r / 10, C_BLACK);
  tft.fillTriangle(cx - r / 5, cy + r / 6, cx + r / 5, cy + r / 6,
                   cx, cy + r / 2, C_BLACK);               // downward frown
  tft.fillCircle(cx + eo, ey, r / 9, C_AQUA);              // a tear
}

static void drawSurprised(uint16_t cx, uint16_t cy, uint16_t r) {
  drawFaceBase(cx, cy, r);
  uint16_t eo = r / 4;
  uint16_t ey = cy - r / 6;
  for (int s = -1; s <= 1; s += 2) {                       // wide "O" eyes
    tft.fillCircle(cx + eo * s, ey, r / 7, C_WHITE);
    tft.fillCircle(cx + eo * s, ey, r / 16, C_BLACK);
  }
  tft.fillCircle(cx, cy + r / 4, r / 4, C_BLACK);          // "O" mouth
}

static void drawSleeping(uint16_t cx, uint16_t cy, uint16_t r) {
  drawFaceBase(cx, cy, r);
  uint16_t eo = r / 4;
  uint16_t ey = cy - r / 6;
  tft.fillRect(cx - r / 2, ey, r / 4, r / 14, C_BLACK);    // closed eyes
  tft.fillRect(cx + r / 4, ey, r / 4, r / 14, C_BLACK);
  tft.fillCircle(cx, cy + r / 4, r / 7, C_PINK);           // pacifier
  tft.setTextColor(C_WHITE, C_BG);
  tft.drawString("Z", cx + r / 3, cy - r / 2, 2);
  tft.drawString("z", cx + r * 3 / 5, cy - r / 3, 1);
}
static void drawHeart(uint16_t cx, uint16_t cy, uint16_t r) {
  uint16_t q = r / 2;
  uint16_t top = cy - r / 3;
  tft.fillCircle(cx - q / 2, top, q, C_RED);
  tft.fillCircle(cx + q / 2, top, q, C_RED);
  tft.fillTriangle(cx - r / 2, top + q / 2, cx + r / 2, top + q / 2,
                   cx, top + r, C_RED);
}

static void drawStar(uint16_t cx, uint16_t cy, uint16_t r) {
  uint16_t a = r / 2;
  tft.fillTriangle(cx, cy - r / 2, cx - a / 2, cy, cx + a / 2, cy, C_YELLOW);
  tft.fillTriangle(cx, cy + r / 2, cx - a / 2, cy, cx + a / 2, cy, C_YELLOW);
  tft.fillTriangle(cx - r / 2, cy, cx, cy - a / 2, cx, cy + a / 2, C_YELLOW);
  tft.fillTriangle(cx + r / 2, cy, cx, cy - a / 2, cx, cy + a / 2, C_YELLOW);
  tft.fillCircle(cx, cy, r / 8, C_WHITE);
}

static void drawEmoji(uint8_t idx, uint16_t cx, uint16_t cy, uint16_t r) {
  switch (idx) {
    case 0: drawHappy(cx, cy, r); break;
    case 1: drawSad(cx, cy, r); break;
    case 2: drawSurprised(cx, cy, r); break;
    case 3: drawSleeping(cx, cy, r); break;
    case 4: drawHeart(cx, cy, r); break;
    default: drawStar(cx, cy, r); break;
  }
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------
uint8_t  current = 0;
uint32_t drawnFrames = 0;
uint32_t lastAdvance = 0;

// Draw the current emoji: clear only a small box around the face, then draw.
void drawEmojiFrame() {
  tft.fillRect(FACE_CX - CLEAR_HALF, FACE_CY - CLEAR_HALF,
               CLEAR_HALF * 2, CLEAR_HALF * 2, C_BG);
  drawEmoji(current, FACE_CX, FACE_CY, FACE_R);
}

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("[tft] boot");

  tft.init();
  tft.setRotation(1);               // landscape 160 x 128 (native 128x160 portrait)
  tft.fillScreen(C_BG);

  // header bar
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEAD);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_WHITE, C_HEAD);
  tft.setTextSize(1);
  tft.drawString("TFT 128x160", SCREEN_W / 2, HEADER_H / 2, 2);

  // status bar
  tft.fillRect(0, SCREEN_H - STATUS_H, SCREEN_W, STATUS_H, C_HEAD);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_WHITE, C_HEAD);
  tft.drawString(EMOJI_NAME[current], 2, SCREEN_H - STATUS_H + 1, 1);

  drawEmojiFrame();                  // first emoji now

  Serial.println("[tft] ready - rotating emoji, one every 2 s");
}

void loop() {
  uint32_t now = millis();

  // Advance to the next emoji every 2 s (redraw only on change = no flicker).
  if (now - lastAdvance >= EMOJI_PERIOD_MS) {
    lastAdvance = now;
    current = (current + 1) % NUM_EMOJI;
    Serial.printf("[tft] emoji: %s\n", EMOJI_NAME[current]);
    drawnFrames++;

    drawEmojiFrame();

    // Refresh the status bar (name left, frame counter right).
    tft.setTextColor(C_WHITE, C_HEAD);
    tft.setTextDatum(TL_DATUM);
    tft.fillRect(0, SCREEN_H - STATUS_H, SCREEN_W, STATUS_H, C_HEAD);
    tft.drawString(EMOJI_NAME[current], 2, SCREEN_H - STATUS_H + 1, 1);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(String(drawnFrames), SCREEN_W - 2, SCREEN_H - STATUS_H + 1, 1);
    tft.setTextDatum(TL_DATUM);
  }
}
