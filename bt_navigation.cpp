#include "bt_navigation.h"
#include <Arduino.h>

// ── Volume control — accepts device hardware volume, no PCM scaling ───────────
class OurVolumeControl : public A2DPVolumeControl {
public:
  // Called when the device reports a volume change. Sync our tracked value
  // so physical Up/Down buttons can increment from the correct base.
  void set_volume(uint8_t v) override {
    playerUpdateVolume(v);
  }
  // No PCM scaling — device hardware DAC handles it entirely.
  void update_audio_data(Frame*, uint16_t) override {}
};

static OurVolumeControl     volumeControl;
static NavigationController* btCtr = nullptr;

// ── AVRC passthrough callback ─────────────────────────────────────────────────
static void on_avrc_command(uint8_t key_code, bool key_released) {
  if (key_released || !btCtr) return;

  switch (key_code) {
    case ESP_AVRC_PT_CMD_PLAY:
      Serial.println("[bt_nav] Play");
      btCtr->onSelectButtonPress();
      break;
    case ESP_AVRC_PT_CMD_PAUSE:
      Serial.println("[bt_nav] Pause");
      btCtr->onSelectButtonPress();
      break;
    case ESP_AVRC_PT_CMD_STOP:
      Serial.println("[bt_nav] Stop");
      btCtr->onSelectButtonPress();
      break;
    case ESP_AVRC_PT_CMD_FORWARD:
      Serial.println("[bt_nav] Next");
      btCtr->onRightButtonPress();
      break;
    case ESP_AVRC_PT_CMD_BACKWARD:
      Serial.println("[bt_nav] Previous");
      btCtr->onLeftButtonPress();
      break;
  }
}

void btNavigationSetup(BluetoothA2DPSource* source, NavigationController* ctr) {
  btCtr = ctr;

  source->set_volume_control(&volumeControl);
  source->set_avrc_passthru_command_callback(on_avrc_command);
  Serial.println("[bt_nav] setup done");
}
