/*
 * main_screen.cpp
 * ESP32-S3 + 7-pin SPI IPS, driven as a 480x320 landscape ILI9488 panel.
 *
 * NOTE: the seller labels this "ST7735/ST7789V 320x240", but the physical panel
 * is ~6x9cm and is actually a 480x320 ILI9488-class panel (driver set via the
 * `[env:esp32-screen]` build flags in platformio.ini: ILI9488_DRIVER).
 *
 * What it does: draws one emoji (bit-art built from drawing primitives) in
 * the middle of the screen. Emoji advances every second and the current one
 * gently pulses in the meantime, so you can watch it animate live.
 * A status bar at the bottom shows the emoji name + frame counter, and the
 * name is also echoed over the serial monitor (115200 baud).
 *
 * If the colours look inverted/blue-ish, toggle the RGB/BGR and inversion
 * defines further down. If the image is mirrored or offset, tweak setRotation.
 *
 * Flash with:  pio run -e esp32-screen -t upload
 */

#include <Arduino.h>
#include <TFT_eSPI.h> // config comes from platformio.ini build flags

TFT_eSPI tft = TFT_eSPI();

// ---------------------------------------------------------------------------
// Animation / layout config
// ---------------------------------------------------------------------------
#define EMOJI_PERIOD_MS 1000u      // advance to the next emoji every second
#define CLEAR_HALF 82u              // half-size of the small box cleared around the face on change

#define AREA_X0 60u                // emoji render area (cleared each frame)
#define AREA_Y0 30u
#define AREA_W  360u
#define AREA_H  250u
#define FACE_CX (AREA_X0 + AREA_W / 2)
#define FACE_CY (AREA_Y0 + AREA_H / 2)
#define FACE_R  72                 // base face radius in px

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
// Emoji bit-art
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
uint8_t     current = 0;
uint32_t    drawnFrames = 0;
uint32_t    lastAdvance = 0;
uint32_t    lastDraw = 0;

// Draw the current emoji neatly: clear only a small box around the face, then
// draw. Kept to a once-per-second change so there is no fast flicker.
void drawEmojiFrame() {
  tft.fillRect(FACE_CX - CLEAR_HALF, FACE_CY - CLEAR_HALF,
               CLEAR_HALF * 2, CLEAR_HALF * 2, C_BG);
  drawEmoji(current, FACE_CX, FACE_CY, FACE_R);
}

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("[screen] boot");

  tft.init();
  tft.setRotation(1);               // landscape 480 x 320
  tft.fillScreen(C_BG);
  tft.setTextColor(C_WHITE, C_HEAD);
  tft.setTextSize(1);

  tft.fillRect(0, 0, 480, 22, C_HEAD);                        // header bar
  tft.setTextDatum(MC_DATUM);
  tft.drawString("EMOJI TEST - 480x320", 240, 8, 2);

  tft.fillRect(0, 298, 480, 22, C_HEAD);                      // status bar
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_WHITE, C_HEAD);
  tft.drawString(EMOJI_NAME[current], 8, 304, 1);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("0", 472, 304, 1);
  tft.setTextDatum(TL_DATUM);

  drawEmojiFrame();                                          // first emoji now

  Serial.println("[screen] ready - animating emoji, one per second");
}

void loop() {
  uint32_t now = millis();

  // Advance to the next emoji every second (only redraw on change = no flicker).
  if (now - lastAdvance >= EMOJI_PERIOD_MS) {
    lastAdvance = now;
    current = (current + 1) % NUM_EMOJI;
    Serial.printf("[screen] emoji: %s\n", EMOJI_NAME[current]);
    drawnFrames++;

    drawEmojiFrame();

    // Refresh the status bar (name left, frame count right).
    tft.setTextColor(C_WHITE, C_HEAD);
    tft.setTextDatum(TL_DATUM);
    tft.fillRect(0, 298, 480, 22, C_HEAD);
    tft.drawString(EMOJI_NAME[current], 8, 304, 1);
    tft.setTextDatum(TR_DATUM);
    tft.drawString(String(drawnFrames), 472, 304, 1);
    tft.setTextDatum(TL_DATUM);
  }
}
