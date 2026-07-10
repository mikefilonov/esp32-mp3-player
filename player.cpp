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

#include "player.h"
#include "esp_a2dp_api.h"
#include <Arduino.h>
#include <Preferences.h>

// ── Ring buffer
// ───────────────────────────────────────────────────────────────
#define RING_BUF_SAMPLES 8192
#define RING_BUF_MASK (RING_BUF_SAMPLES - 1)
static int16_t ring[RING_BUF_SAMPLES];
static volatile int ring_write = 0;
static volatile int ring_read = 0;

static inline int ring_available() {
  return (ring_write - ring_read + RING_BUF_SAMPLES) & RING_BUF_MASK;
}

static inline int ring_free_space() {
  int r = ring_read;
  int w = ring_write;
  if (r > w) {
    return r - w - 2;
  } else {
    return r - w - 2 + RING_BUF_SAMPLES;
  }
}

// ── Custom AudioOutput that writes decoded samples into the ring buffer
// ───────
class RingBufferOutput : public AudioOutput {
private:
  int file_hz = 44100;
  float ratio = 1.0f;
  float inv_ratio = 1.0f;
  float resample_phase = 0.0f;
  int16_t last_sample[2] = {0, 0};

public:
  bool begin() override {
    file_hz = 44100;
    ratio = 1.0f;
    inv_ratio = 1.0f;
    resample_phase = 0.0f;
    last_sample[0] = 0;
    last_sample[1] = 0;
    return true;
  }
  bool stop() override { return true; }

  bool SetRate(int hz) override {
    file_hz = hz;
    ratio = (float)hz / 44100.0f;
    // Pre-calculate inverse ratio to replace slow divisions in ConsumeSample with multiplications
    inv_ratio = ratio > 0.0f ? 1.0f / ratio : 1.0f;
    resample_phase = 0.0f;
    return AudioOutput::SetRate(hz);
  }

  bool SetChannels(int chan) override { return AudioOutput::SetChannels(chan); }

  bool ConsumeSample(int16_t sample[2]) override {
    if (file_hz == 0 || file_hz == 44100) {
      // Direct pass-through if no resampling is needed
      int next_w = (ring_write + 2) & RING_BUF_MASK;
      if (next_w == ring_read)
        return false; // full (safety fallback)
      ring[ring_write] = sample[0];
      ring[(ring_write + 1) & RING_BUF_MASK] = sample[1];
      ring_write = next_w;
      return true;
    }

    // Linear resampler: convert from file_hz to 44100 Hz (causal push model)
    // Interpolates between previous and current sample to prevent high-frequency noise.
    bool success = true;
    while (resample_phase < 1.0f) {
      int next_w = (ring_write + 2) & RING_BUF_MASK;
      if (next_w == ring_read) {
        success = false;
        break;
      }

      float t = resample_phase;
      int16_t out_l = last_sample[0] + t * (sample[0] - last_sample[0]);
      int16_t out_r = last_sample[1] + t * (sample[1] - last_sample[1]);

      ring[ring_write] = out_l;
      ring[(ring_write + 1) & RING_BUF_MASK] = out_r;
      ring_write = next_w;

      resample_phase += ratio;
    }

    resample_phase -= 1.0f;
    last_sample[0] = sample[0];
    last_sample[1] = sample[1];
    return success;
  }
};

// ── Module state
// ──────────────────────────────────────────────────────────────
static BluetoothA2DPSource a2dp_source;
static AudioFileSourceSD *file = nullptr;
static AudioGeneratorMP3 *mp3 = nullptr;
static RingBufferOutput *ringOut = nullptr;

// Pre-allocate the MP3 decoder's internal buffers statically in BSS (around 25
// KB) to prevent heap fragmentation during track transitions.
static uint8_t mp3DecoderBuffer[AudioGeneratorMP3::preAllocSize()];

static PlayerEvents *playerEvents = nullptr;
static volatile bool bt_connected = false;
static volatile bool paused = false;
static volatile uint8_t currentVolume =
    64; // initialized to mid-range (50%) to prevent startup jumps
static Preferences preferences;
static bool volumeSyncedToSpeaker = false;

// ── A2DP data callback — runs on BT task, must not block ─────────────────────
static int32_t get_sound_data(Frame *frames, int32_t num_frames) {
  if (paused) {
    memset(frames, 0, num_frames * sizeof(Frame));
    return num_frames;
  }

  int available_frames = ring_available() / 2;
  int to_send = min((int)num_frames, available_frames);

  for (int i = 0; i < to_send; i++) {
    frames[i].channel1 = ring[ring_read];
    frames[i].channel2 = ring[(ring_read + 1) & RING_BUF_MASK];
    ring_read = (ring_read + 2) & RING_BUF_MASK;
  }
  // No PCM scaling — volume is handled entirely by the device's hardware DAC.

  for (int i = to_send; i < num_frames; i++) {
    frames[i].channel1 = 0;
    frames[i].channel2 = 0;
  }

  return num_frames;
}

static bool filter_devices(const char *ssid, esp_bd_addr_t address, int rssi) {
  Serial.printf("[player] Scanned compatible BT device: %s, RSSI: %d\n",
                ssid ? ssid : "(null)", rssi);
  if (ssid && strlen(ssid) > 0) {
    Serial.printf("[player] Connecting to discovered speaker: %s\n", ssid);
    return true; // Accept this device
  }
  return false;
}

// ── Public API
// ────────────────────────────────────────────────────────────────
bool playerInit(PlayerEvents *events, bool pairingModeRequested) {
  playerEvents = events;

  a2dp_source.set_data_callback_in_frames(get_sound_data);
  a2dp_source.set_ssid_callback(filter_devices);

  a2dp_source.set_auto_reconnect(true);
  a2dp_source.get_last_connection();

  // Restore volume from NVS preferences
  preferences.begin("mp3player", false);
  currentVolume = preferences.getUChar("volume", 64);
  preferences.end();
  Serial.printf("[player] NVS Restored volume: %d\n", currentVolume);

  bool noLastConnection = !a2dp_source.has_last_connection();
  bool activePairing = pairingModeRequested || noLastConnection;

  if (noLastConnection) {
    Serial.println("[player] No connection history found — entering pairing "
                   "mode by default.");
  }

  if (pairingModeRequested) {
    Serial.println("[player] Pairing requested on boot — clearing last paired "
                   "device to allow new pairing.");
    a2dp_source.clean_last_connection();
  }

  a2dp_source.set_on_connection_state_changed(
      [](esp_a2d_connection_state_t state, void *) {
        if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
          Serial.println("[player] BT connected");
          bt_connected = true;
          if (playerEvents)
            playerEvents->onBTConnected();
        } else {
          Serial.println("[player] BT disconnected");
          bt_connected = false;
          volumeSyncedToSpeaker = false; // Reset sync flag on disconnect
          if (playerEvents)
            playerEvents->onBTDisconnected();
        }
      });

  return activePairing;
}

void playerStart() {
  a2dp_source.start();
  Serial.println("[player] A2DP started");
}

void playerLoop() {
  if (mp3 && mp3->isRunning()) {
    // Fill the ring buffer. We decode as long as there is space for at least
    // one full MP3 frame (typically 1152 stereo frames = 2304 samples). This
    // ensures ConsumeSample will never block under normal operation.
    while (ring_free_space() >= 2304) {
      if (!mp3->loop()) {
        mp3->stop();
        Serial.println("[player] track finished");
        if (playerEvents)
          playerEvents->onTrackFinished();
        break;
      }
      yield(); // Yield to feed watchdog and allow other tasks to run
    }
  }
}

void playerStartFile(const char *path) {
  playerStop();

  Serial.printf("[player] opening %s\n", path);
  file = new AudioFileSourceSD(path);
  if (!file->isOpen()) {
    Serial.println("[player] failed to open file");
    delete file;
    file = nullptr;
    return;
  }

  ringOut = new RingBufferOutput();
  mp3 = new AudioGeneratorMP3(mp3DecoderBuffer, sizeof(mp3DecoderBuffer));

  if (!mp3->begin(file, ringOut)) {
    Serial.println("[player] mp3->begin() failed");
    delete mp3;
    mp3 = nullptr;
    delete file;
    file = nullptr;
    delete ringOut;
    ringOut = nullptr; // Fix memory leak
    return;
  }

  // Pre-fill the ring buffer before resuming playback to prevent startup
  // underruns/wobble by ensuring the buffer is full before stream begins.
  int prefill_count = 0;
  while (ring_free_space() >= 2304) {
    if (!mp3->loop()) {
      break;
    }
    prefill_count++;
  }

  paused = false;
  // Send A2DP START command to tell the speaker to resume decoding and reset
  // its clock recovery. This stabilizes pitch/speed immediately.
  esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
  Serial.printf("[player] playing %s\n", path);
}

void playerStop() {
  paused = true;
  delay(30); // Wait for the Bluetooth thread to exit get_sound_data and enter
             // paused state

  // Send A2DP SUSPEND command to tell the speaker to pause its clock recovery
  // during transitions. This prevents the speaker from drifting on packet jitter.
  esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
  delay(50); // Give the BT stack a brief moment to transmit the SUSPEND packet

  if (mp3) {
    if (mp3->isRunning())
      mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (file) {
    delete file;
    file = nullptr;
  }
  if (ringOut) {
    delete ringOut;
    ringOut = nullptr;
  }

  ring_write = 0;
  ring_read = 0;
}

void playerPause(bool p) {
  paused = p;
  // Sync the Bluetooth A2DP stream state with the playback pause state
  esp_a2d_media_ctrl(p ? ESP_A2D_MEDIA_CTRL_SUSPEND : ESP_A2D_MEDIA_CTRL_START);
  Serial.printf("[player] %s\n", p ? "paused" : "resumed");
}

bool playerIsStopped() { return mp3 == nullptr || !mp3->isRunning(); }

bool playerIsPaused() { return paused; }

bool playerIsConnected() { return bt_connected; }

static void saveVolumeToNVS(uint8_t vol) {
  preferences.begin("mp3player", false);
  preferences.putUChar("volume", vol);
  preferences.end();
  Serial.printf("[player] NVS Saved volume: %d\n", vol);
}

// Called by bt_navigation when the device reports a volume change.
// Just tracks the value so the controller can read it for Up/Down increments.
void playerUpdateVolume(uint8_t volume_0_127) {
  if (!volumeSyncedToSpeaker) {
    // Ignore initial speaker volume reports until we successfully push our NVS
    // restored volume
    Serial.printf("[player] Ignored initial speaker volume report of %d to "
                  "protect NVS restored state\n",
                  volume_0_127);
    return;
  }
  if (currentVolume != volume_0_127) {
    currentVolume = volume_0_127;
    saveVolumeToNVS(volume_0_127);
  }
}

// Called by physical buttons via the controller.
// Sends an AVRC "set absolute volume" command to the device — the device's
// hardware DAC applies it. No PCM scaling on our side.
void playerSetVolume(uint8_t volume_0_127) {
  if (currentVolume != volume_0_127 || !volumeSyncedToSpeaker) {
    currentVolume = volume_0_127;
    saveVolumeToNVS(volume_0_127);
  }
  esp_avrc_ct_send_set_absolute_volume_cmd(1 /*TL_RN_VOLUME_CHANGE*/,
                                           volume_0_127);
  volumeSyncedToSpeaker =
      true; // We successfully synchronized our NVS volume to the speaker!
  Serial.printf("[player] set volume: %d\n", volume_0_127);
}

uint8_t playerGetVolume() { return currentVolume; }

BluetoothA2DPSource *getA2DPSource() { return &a2dp_source; }
