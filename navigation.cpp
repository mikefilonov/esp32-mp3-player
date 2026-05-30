#include "navigation.h"
#include <Wire.h>
#include "Adafruit_seesaw.h"
#include <seesaw_neopixel.h>

#define SS_SWITCH        24
#define SS_NEOPIX        6
#define SEESAW_ADDR      0x36

static Adafruit_seesaw ss;
static seesaw_NeoPixel sspixel = seesaw_NeoPixel(1, SS_NEOPIX, NEO_GRB + NEO_KHZ800);
static bool ss_ok = false;
static int32_t lastSeesawPos = 0;
static bool lastSeesawBtn = false;
static bool wasRotatedWhilePressed = false;

// ── Pin configuration for traditional controls (fallback) ──────────────────────
#define BTN_SELECT_PIN  14
#define BTN_LEFT_PIN    13
#define BTN_RIGHT_PIN   12
#define BTN_UP_PIN      27
#define BTN_DOWN_PIN    26
#define ENCODER_CLK_PIN 32
#define ENCODER_DT_PIN  33

struct ButtonState {
  uint8_t pin;
  int     lastState;
};

static ButtonState selectBtn, leftBtn, rightBtn, upBtn, downBtn;
static int lastEncoderClk = HIGH;

static ButtonState setupButton(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  return { pin, HIGH };
}

static bool buttonPressed(ButtonState& btn) {
  int current = digitalRead(btn.pin);
  if (current != btn.lastState) {
    btn.lastState = current;
    return (current == LOW); // active-low: LOW = pressed
  }
  return false;
}

void navigationSetup() {
  // Try to initialize Seesaw I2C encoder
  Serial.println("[navigation] Initializing Seesaw Rotary Encoder...");
  if (ss.begin(SEESAW_ADDR) && sspixel.begin(SEESAW_ADDR)) {
    Serial.println("[navigation] Seesaw started successfully!");
    sspixel.setBrightness(255);
    // Blue for initial state (BT disconnected)
    sspixel.setPixelColor(0, sspixel.Color(0, 0, 255));
    sspixel.show();
    
    ss.pinMode(SS_SWITCH, INPUT_PULLUP);
    lastSeesawPos = ss.getEncoderPosition();
    lastSeesawBtn = !ss.digitalRead(SS_SWITCH);
    ss_ok = true;
  } else {
    Serial.println("[navigation] WARNING: Seesaw Rotary Encoder not found on I2C address 0x36. Falling back to discrete inputs.");
    ss_ok = false;
  }

  // Fallback / standard physical button configuration
  selectBtn = setupButton(BTN_SELECT_PIN);
  leftBtn   = setupButton(BTN_LEFT_PIN);
  rightBtn  = setupButton(BTN_RIGHT_PIN);
  upBtn     = setupButton(BTN_UP_PIN);
  downBtn   = setupButton(BTN_DOWN_PIN);

  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN,  INPUT_PULLUP);
  lastEncoderClk = digitalRead(ENCODER_CLK_PIN);

  Serial.println("[navigation] setup done");
}

void navigationLoop(NavigationController* ctr) {
  if (ss_ok) {
    // 1. Read button state (active low on Seesaw)
    bool isPressed = !ss.digitalRead(SS_SWITCH);
    
    // 2. Read encoder position
    int32_t newPos = ss.getEncoderPosition();
    int32_t delta = newPos - lastSeesawPos;
    
    if (delta != 0) {
      if (isPressed) {
        // Chorded action: holding button + rotating = track skip
        if (delta > 0) {
          Serial.println("[navigation] Seesaw chorded next");
          ctr->onRightButtonPress();
        } else {
          Serial.println("[navigation] Seesaw chorded prev");
          ctr->onLeftButtonPress();
        }
        wasRotatedWhilePressed = true;
      } else {
        // Normal action: just rotating = volume control
        // Note: multiple ticks might occur, we send increments per tick
        int32_t ticks = abs(delta);
        for (int32_t i = 0; i < ticks; i++) {
          if (delta > 0) {
            ctr->onUpButtonPress();
          } else {
            ctr->onDownButtonPress();
          }
        }
      }
      lastSeesawPos = newPos;
    }
    
    // 3. Handle button transition logic
    if (isPressed && !lastSeesawBtn) {
      // Button was just pressed down
      wasRotatedWhilePressed = false;
    } else if (!isPressed && lastSeesawBtn) {
      // Button was just released
      if (!wasRotatedWhilePressed) {
        // Only trigger play/pause if the button wasn't held down for a track skip
        Serial.println("[navigation] Seesaw click play/pause");
        ctr->onSelectButtonPress();
      }
    }
    lastSeesawBtn = isPressed;
  }

  // ── Traditional physical buttons / encoder fallback ──
  if (buttonPressed(selectBtn)) ctr->onSelectButtonPress();
  if (buttonPressed(leftBtn))   ctr->onLeftButtonPress();
  if (buttonPressed(rightBtn))  ctr->onRightButtonPress();
  if (buttonPressed(upBtn))     ctr->onUpButtonPress();
  if (buttonPressed(downBtn))   ctr->onDownButtonPress();

  int clk = digitalRead(ENCODER_CLK_PIN);
  if (clk != lastEncoderClk && clk == LOW) {
    int8_t delta = (digitalRead(ENCODER_DT_PIN) != clk) ? 1 : -1;
    ctr->onWheelSpin(delta);
  }
  lastEncoderClk = clk;
}

void navigationSetLedColor(uint8_t r, uint8_t g, uint8_t b) {
  if (ss_ok) {
    sspixel.setPixelColor(0, sspixel.Color(r, g, b));
    sspixel.show();
  }
}

