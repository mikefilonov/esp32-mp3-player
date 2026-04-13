#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif
#define CORE_DEBUG_LEVEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <Arduino.h>
#include "esp32-hal-log.h"

#include "sdcard.h"
#include "player.h"
#include "navigation.h"
#include "bt_navigation.h"
#include "controller.h"

Mp3PlayerController ctr;

void setup() {
  Serial.begin(115200);
  audioLogger = &Serial;

  sdSetup();
  playerInit("NR-103", &ctr);       // configure BT, don't start yet
  btNavigationSetup(getA2DPSource(), &ctr); // register AVRC callback before start
  navigationSetup();
  playerStart();                     // now start BT with all callbacks in place
}

void loop() {
  ctr.loop();          // dispatch any pending action on the main task
  navigationLoop(&ctr);
  playerLoop();
  delay(10);
}
