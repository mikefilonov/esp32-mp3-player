/*
 * ESP32 Bluetooth MP3 Player (A2DP Source)
 * Copyright (C) 2026  Mike Filonov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
    Serial.println("[navigation] ERROR: Seesaw Rotary Encoder not found on I2C address 0x36.");
    ss_ok = false;
  }

  Serial.println("[navigation] setup done");
}

void navigationLoop(NavigationController* ctr) {
  if (!ss_ok) return;

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

void navigationSetLedColor(uint8_t r, uint8_t g, uint8_t b) {
  if (ss_ok) {
    sspixel.setPixelColor(0, sspixel.Color(r, g, b));
    sspixel.show();
  }
}

bool navigationIsButtonPressed() {
  if (ss_ok) {
    return !ss.digitalRead(SS_SWITCH);
  }
  return false;
}



