/*
 ============================================================
   T I M E   B O M B   S I M U L A T O R
   Target Board : Arduino Mega 2560
   Display      : SSD1306 OLED 128×64 (I2C)
   Libraries    : Adafruit SSD1306 + Adafruit GFX
 ============================================================

  WIRING GUIDE
  ─────────────────────────────────────────────────────────
  SSD1306 OLED
    VCC  → 3.3 V  (or 5 V if your module supports it)
    GND  → GND
    SDA  → Pin 20  (Mega I²C SDA)
    SCL  → Pin 21  (Mega I²C SCL)

  BUZZER (passive)
    +    → Pin 8
    -    → GND

  BUTTON (momentary, normally open)
    One leg → Pin 2
    Other   → GND   (internal pull-up used)

  RED LED
    Anode   → 1 kΩ resistor → Pin 9
    Cathode → GND

  GREEN LED
    Anode   → 1 kΩ resistor → Pin 10
    Cathode → GND
  ─────────────────────────────────────────────────────────
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Display ──────────────────────────────────────────────
#define SCREEN_W   128
#define SCREEN_H    64
#define OLED_RESET  -1          // shared reset with Mega
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

// ── Pins ──────────────────────────────────────────────────
const uint8_t PIN_BUZZER = 8;
const uint8_t PIN_BUTTON = 2;   // LOW when pressed (INPUT_PULLUP)
const uint8_t PIN_LED_RED   = 9;
const uint8_t PIN_LED_GREEN = 10;

// ── Bomb timing ───────────────────────────────────────────
const unsigned long BOMB_DURATION_MS = 10000UL;  // 10 seconds

// ── Buzzer tones ──────────────────────────────────────────
const int  TONE_BEEP      = 880;   // countdown beep frequency (Hz)
const int  TONE_ARMED     = 1200;  // armed-alert frequency
const int  TONE_EXPLODE_1 = 200;   // explosion sweep start
const int  TONE_EXPLODE_2 = 3000;  // explosion sweep peak

// ── Helpers ──────────────────────────────────────────────
// Calculates X offset to horizontally centre text of a given length / size
int centreX(uint8_t charCount, uint8_t textSize) {
  return (SCREEN_W - (int)charCount * 6 * textSize) / 2;
}

// ─────────────────────────────────────────────────────────
//  SCREEN 1 – Idle / "Press Button"
// ─────────────────────────────────────────────────────────
void showIdleScreen() {
  display.clearDisplay();

  // Outer double border
  display.drawRect(0, 0, SCREEN_W, SCREEN_H, SSD1306_WHITE);
  display.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, SSD1306_WHITE);

  // Header bar
  display.fillRect(3, 3, SCREEN_W - 6, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(centreX(11, 1), 6);   // "[ TIMEBOMB ]" = 12 chars
  display.print(F("[ TIMEBOMB ]"));

  // Divider
  display.setTextColor(SSD1306_WHITE);

  // Bomb icon (simple pixel art)
  // Body
  display.fillCircle(64, 32, 10, SSD1306_WHITE);
  // Fuse line
  display.drawLine(70, 24, 76, 16, SSD1306_WHITE);
  display.drawLine(76, 16, 80, 20, SSD1306_WHITE);
  // Spark dot
  display.fillCircle(81, 19, 2, SSD1306_WHITE);

  // Bottom prompt
  display.setTextSize(1);
  display.setCursor(centreX(16, 1), 50);
  display.print(F("PRESS BTN TO ARM"));

  display.display();
}

// ─────────────────────────────────────────────────────────
//  SCREEN 2 – "The Bomb Is Armed"
// ─────────────────────────────────────────────────────────
void showArmedScreen() {
  // Three rapid alert beeps
  for (uint8_t i = 0; i < 3; i++) {
    tone(PIN_BUZZER, TONE_ARMED, 80);
    digitalWrite(PIN_LED_RED, HIGH);
    delay(120);
    noTone(PIN_BUZZER);
    digitalWrite(PIN_LED_RED, LOW);
    delay(120);
  }

  display.clearDisplay();

  // Outer border (thick feel via two rects)
  display.drawRect(0, 0, SCREEN_W, SCREEN_H, SSD1306_WHITE);
  display.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, SSD1306_WHITE);

  // Flashing header bar
  display.fillRect(3, 3, SCREEN_W - 6, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(centreX(11, 1), 6);
  display.print(F("! WARNING  !"));

  display.setTextColor(SSD1306_WHITE);

  // "THE BOMB IS"
  display.setTextSize(2);
  display.setCursor(centreX(11, 2), 22);
  display.print(F("THE BOMB"));

  // "ARMED!"  – slightly larger for drama
  display.setTextSize(2);
  display.setCursor(centreX(6, 2), 42);
  display.print(F("ARMED!"));

  display.display();
  delay(2200);
}

// ─────────────────────────────────────────────────────────
//  SCREEN 3 – Countdown
// ─────────────────────────────────────────────────────────
void drawCountdownFrame(float secondsLeft, float progress) {
  display.clearDisplay();

  // Outer border
  display.drawRect(0, 0, SCREEN_W, SCREEN_H, SSD1306_WHITE);

  // Header
  display.fillRect(1, 1, SCREEN_W - 2, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(centreX(9, 1), 3);
  display.print(F("COUNTDOWN"));
  display.setTextColor(SSD1306_WHITE);

  // ── Big number ────────────────────────────────────────
  char buf[8];
  if (secondsLeft >= 1.0f) {
    // Integer seconds – use textSize 4 (each char ≈ 24 px wide, 32 tall)
    int secs = (int)ceil(secondsLeft);
    itoa(secs, buf, 10);
    uint8_t len = strlen(buf);
    display.setTextSize(4);
    display.setCursor(centreX(len, 4), 16);
    display.print(buf);
  } else {
    // Sub-second decimals – textSize 3 (each char ≈ 18 px wide, 24 tall)
    // Format as "0.XX"
    int hundredths = (int)(secondsLeft * 100.0f);
    if (hundredths < 0) hundredths = 0;
    snprintf(buf, sizeof(buf), "0.%02d", hundredths);
    uint8_t len = strlen(buf);            // always 4 chars
    display.setTextSize(3);
    display.setCursor(centreX(len, 3), 20);
    display.print(buf);
  }

  // ── Progress bar ─────────────────────────────────────
  //   Sits at the very bottom, inside the border
  const int BAR_X = 4;
  const int BAR_Y = 53;
  const int BAR_W = SCREEN_W - 8;   // 120 px
  const int BAR_H = 8;

  // Empty shell
  display.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, SSD1306_WHITE);

  // Filled portion (progress 0→1 as time runs out)
  int filled = (int)(progress * (BAR_W - 2));
  if (filled > 0) {
    display.fillRect(BAR_X + 1, BAR_Y + 1, filled, BAR_H - 2, SSD1306_WHITE);
  }

  display.display();
}

// ─────────────────────────────────────────────────────────
//  SCREEN 4 – Explosion
// ─────────────────────────────────────────────────────────
void triggerExplosion() {
  // ── Explosion sound sweep ─────────────────────────────
  for (int f = TONE_EXPLODE_1; f <= TONE_EXPLODE_2; f += 60) {
    tone(PIN_BUZZER, f, 10);
    delay(8);
  }
  for (int f = TONE_EXPLODE_2; f >= TONE_EXPLODE_1; f -= 40) {
    tone(PIN_BUZZER, f, 10);
    delay(6);
  }
  noTone(PIN_BUZZER);
  digitalWrite(PIN_LED_RED, LOW);

  // ── Flash display white (shockwave effect) ────────────
  for (uint8_t i = 0; i < 4; i++) {
    display.fillScreen(SSD1306_WHITE);
    display.display();
    delay(60);
    display.clearDisplay();
    display.display();
    delay(60);
  }

  // ── "BOOM!" screen ────────────────────────────────────
  display.clearDisplay();
  display.fillScreen(SSD1306_WHITE);         // white background

  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(4);
  display.setCursor(centreX(4, 4), 8);
  display.print(F("BOOM"));

  display.setTextSize(1);
  display.setCursor(centreX(12, 1), 50);
  display.print(F("DETONATED  :)"));

  display.display();

  // ── Green LED on = "signal sent" ─────────────────────
  digitalWrite(PIN_LED_GREEN, HIGH);

  // Halt forever
  while (true) { /* done */ }
}

// ─────────────────────────────────────────────────────────
//  MAIN COUNTDOWN LOOP
// ─────────────────────────────────────────────────────────
void runCountdown() {
  unsigned long startMs  = millis();
  unsigned long endMs    = startMs + BOMB_DURATION_MS;

  // State for non-blocking LED / buzzer
  unsigned long lastLedToggle  = 0;
  unsigned long lastBuzzerBeep = 0;
  bool          ledState       = false;

  while (true) {
    unsigned long now       = millis();
    long          remainMs  = (long)(endMs - now);

    if (remainMs <= 0) {
      // Draw one last fully-filled frame then explode
      drawCountdownFrame(0.0f, 1.0f);
      delay(120);
      triggerExplosion();
      return;  // never reached
    }

    float secondsLeft = remainMs / 1000.0f;
    float progress    = 1.0f - (secondsLeft / (BOMB_DURATION_MS / 1000.0f));

    // ── LED blink ──────────────────────────────────────
    // Interval shrinks from 900 ms (10 s left) → 80 ms (0 s left)
    long ledInterval = map(remainMs, 0, (long)BOMB_DURATION_MS, 80L, 900L);
    ledInterval = constrain(ledInterval, 80L, 900L);

    if ((long)(now - lastLedToggle) >= ledInterval) {
      ledState = !ledState;
      digitalWrite(PIN_LED_RED, ledState ? HIGH : LOW);
      lastLedToggle = now;
    }

    // ── Buzzer beep ───────────────────────────────────
    // Interval shrinks from 1000 ms → 100 ms
    long buzzerInterval = map(remainMs, 0, (long)BOMB_DURATION_MS, 100L, 1000L);
    buzzerInterval = constrain(buzzerInterval, 100L, 1000L);

    // Pitch rises as time runs out: 600 Hz → 1200 Hz
    int buzzerFreq = (int)map(remainMs, 0, (long)BOMB_DURATION_MS, 1200, 600);
    buzzerFreq = constrain(buzzerFreq, 600, 1200);

    if ((long)(now - lastBuzzerBeep) >= buzzerInterval) {
      tone(PIN_BUZZER, buzzerFreq, 40);
      lastBuzzerBeep = now;
    }

    // ── Refresh display ───────────────────────────────
    drawCountdownFrame(secondsLeft, progress);
  }
}

// ─────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────
void setup() {
  pinMode(PIN_BUZZER,    OUTPUT);
  pinMode(PIN_BUTTON,    INPUT_PULLUP);
  pinMode(PIN_LED_RED,   OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);

  digitalWrite(PIN_LED_RED,   LOW);
  digitalWrite(PIN_LED_GREEN, LOW);

  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 init failed – check wiring/address"));
    while (true);    // halt
  }

  display.cp437(true);   // use full CP437 char set
  display.clearDisplay();
  showIdleScreen();
}

// ─────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────
void loop() {
  // Only action: wait for button press to arm
  if (digitalRead(PIN_BUTTON) == LOW) {
    delay(50);                              // debounce
    if (digitalRead(PIN_BUTTON) == LOW) {
      showArmedScreen();
      runCountdown();
      // runCountdown() never returns (halts in triggerExplosion)
    }
  }
}
