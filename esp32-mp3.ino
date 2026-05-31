#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif
#define CORE_DEBUG_LEVEL 0
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <Arduino.h>
#include "esp32-hal-log.h"

#include "nvs.h"
#include "nvs_flash.h"



#include "sdcard.h"
#include "player.h"
#include "navigation.h"
#include "bt_navigation.h"
#include "controller.h"

Mp3PlayerController ctr;

void setup() {
  Serial.begin(9600);
  while (!Serial) delay(10); // Wait for serial port to connect
  audioLogger = &Serial;

  std::vector<const char*> btNames = {"Beats Solo 4", "Headphones 01", "RX-V385 Yamaha", "NR-103"};

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

  nvs_flash_deinit();
  btNavigationSetup(getA2DPSource(), &ctr); // register AVRC callback before start

  // deinit before playerStart
  playerStart(btNames);                     // now start BT with all callbacks in place
}

void loop() {
  ctr.loop();          // dispatch any pending action on the main task
  navigationLoop(&ctr);
  playerLoop();
  delay(50);
}
