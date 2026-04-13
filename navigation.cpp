#include "navigation.h"

// ── Pin configuration ─────────────────────────────────────────────────────────
// TODO: adjust these to match your physical wiring
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
static int encoderPos = 0;

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
  if (buttonPressed(selectBtn)) ctr->onSelectButtonPress();
  if (buttonPressed(leftBtn))   ctr->onLeftButtonPress();
  if (buttonPressed(rightBtn))  ctr->onRightButtonPress();
  if (buttonPressed(upBtn))     ctr->onUpButtonPress();
  if (buttonPressed(downBtn))   ctr->onDownButtonPress();

  // Rotary encoder: detect direction on CLK falling edge
  int clk = digitalRead(ENCODER_CLK_PIN);
  if (clk != lastEncoderClk && clk == LOW) {
    int8_t delta = (digitalRead(ENCODER_DT_PIN) != clk) ? 1 : -1;
    ctr->onWheelSpin(delta);
  }
  lastEncoderClk = clk;
}
