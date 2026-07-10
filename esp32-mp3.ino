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

#include <Arduino.h>
#include "nvs.h"
#include "nvs_flash.h"

// Set to 1 to enable Serial diagnostics & plotter telemetry, or 0 to completely disable both
// (Disabled to prevent Serial print operations from blocking/starving the real-time audio pipeline)
#define ENABLE_DIAGNOSTICS 0

#include "sdcard.h"
#include "player.h"
#include "navigation.h"
#include "bt_navigation.h"
#include "controller.h"

Mp3PlayerController ctr;

void setup() {
#if ENABLE_DIAGNOSTICS
  // Changed to 115200 baud to speed up serial output and prevent buffer blocking
  Serial.begin(115200);
  while (!Serial) delay(10); // Wait for serial port to connect
  audioLogger = &Serial;
#endif

  // needed for bt last connection read
  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    Serial.println("[setup] nvs_flash_init fail");
  }

  sdSetup();
  navigationSetup();                        // Initialize Seesaw knob & button first!
  bool pairingRequested = navigationIsButtonPressed();
  bool activePairing = playerInit(&ctr, pairingRequested); // configure BT, returns true if active pairing mode
  if (activePairing) {
    Serial.println("[setup] Pairing mode active (forced or fresh boot) — Setting NeoPixel to Purple");
    navigationSetLedColor(255, 0, 255);     // Purple/Magenta for Pairing Mode
  } else {
    Serial.println("[setup] Normal boot mode — Setting NeoPixel to Blue");
    navigationSetLedColor(0, 0, 255);       // Blue for Normal Boot (reconnecting)
  }
  ctr.setPairingModeActive(activePairing);  // Notify controller of pairing status to lock boot-up color
  btNavigationSetup(getA2DPSource(), &ctr); // register AVRC callback before start

  playerStart();                            // now start BT with generic open pairing
}

void loop() {
  ctr.loop();          // dispatch any pending action on the main task
  navigationLoop(&ctr);
  playerLoop();

#if ENABLE_DIAGNOSTICS
  // Print memory diagnostics every 1 second to plot real-time memory health in the Arduino IDE Serial Plotter
  static unsigned long lastMemLog = 0;
  unsigned long now = millis();
  if (now - lastMemLog >= 1000) {
    lastMemLog = now;
    Serial.printf("FreeHeap:%d MinFreeHeap:%d MaxAllocHeap:%d\n", 
                  (int)ESP.getFreeHeap(), 
                  (int)ESP.getMinFreeHeap(), 
                  (int)ESP.getMaxAllocHeap());
  }
#endif

  delay(5);
}
