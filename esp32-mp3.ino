#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif

#define CORE_DEBUG_LEVEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <SD.h>
#include <SPI.h>
#include "esp32-hal-log.h"

#include "BluetoothA2DPSource.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutput.h"

// ── Ring buffer ───────────────────────────────────────────────────────────────
#define RING_BUF_SAMPLES 8192
int16_t  ring[RING_BUF_SAMPLES];
volatile int ring_write = 0;
volatile int ring_read  = 0;

inline int ring_available() {
  return (ring_write - ring_read + RING_BUF_SAMPLES) % RING_BUF_SAMPLES;
}

// ── Custom AudioOutput ────────────────────────────────────────────────────────
class RingBufferOutput : public AudioOutput {
public:
  bool begin() override { return true; }
  bool stop()  override { return true; }

  bool ConsumeSample(int16_t sample[2]) override {
    int next_w = (ring_write + 2) % RING_BUF_SAMPLES;
    if (next_w == ring_read) return false;
    ring[ring_write]                          = sample[0];
    ring[(ring_write + 1) % RING_BUF_SAMPLES] = sample[1];
    ring_write = next_w;
    return true;
  }
};

// ── Globals ───────────────────────────────────────────────────────────────────
BluetoothA2DPSource  a2dp_source;
AudioFileSourceSD   *file    = nullptr;
AudioGeneratorMP3   *mp3     = nullptr;
RingBufferOutput    *ringOut = nullptr;

volatile bool bt_connected    = false;
bool          decoder_started = false;

// ── A2DP data callback ────────────────────────────────────────────────────────
int32_t get_sound_data(Frame *frames, int32_t num_frames) {
  static bool first = true;
  if (first) {
    first = false;
    Serial.println(">>> get_sound_data CALLED <<<");
  }

  int available_frames = ring_available() / 2;
  int to_send = min((int)num_frames, available_frames);

  for (int i = 0; i < to_send; i++) {
    frames[i].channel1 = ring[ring_read];
    frames[i].channel2 = ring[(ring_read + 1) % RING_BUF_SAMPLES];
    ring_read = (ring_read + 2) % RING_BUF_SAMPLES;
  }

  for (int i = to_send; i < num_frames; i++) {
    frames[i].channel1 = 0;
    frames[i].channel2 = 0;
  }

  return num_frames;
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  audioLogger = &Serial;

  while (!SD.begin(5)) {
    Serial.println("SD mount failed, retrying...");
    delay(3000);
  }
  Serial.println("SD OK");

  a2dp_source.set_auto_reconnect(true);

  a2dp_source.set_on_connection_state_changed([](esp_a2d_connection_state_t state, void*) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
      Serial.println("A2DP media connected");
      bt_connected = true;
    } else {
      Serial.println("A2DP disconnected");
      bt_connected    = false;
      decoder_started = false;
    }
  });

  a2dp_source.start("NR-103", get_sound_data);

  Serial.println("============ After Setup End =============");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  if (bt_connected && !decoder_started) {
    Serial.println("BT connected — starting decoder");
    bt_connected = false;

    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Max alloc: %d\n", ESP.getMaxAllocHeap());


    File root = SD.open("/");

    File entry;

    while (true) {
     entry = root.openNextFile();

      const char* name = entry.name();
      if (name[0] == '.') {
        //ignore files that start with .
        entry.close();
        continue;
      }

      break;


    }
    const char* name = entry.path();

    // No AudioFileSourceBuffer — feed SD directly to avoid queue crash
    file = new AudioFileSourceSD(name);
    if (!file->isOpen()) {
      Serial.println("Failed to open file");
      delete file; file = nullptr;
      return;
    }
    Serial.println("File opened OK");

    ringOut = new RingBufferOutput();
    mp3     = new AudioGeneratorMP3();

    if (!mp3->begin(file, ringOut)) {
      Serial.println("mp3->begin() FAILED");
      delete mp3;  mp3  = nullptr;
      delete file; file = nullptr;
      return;
    }

    Serial.println("mp3->begin() OK — decoder running");
    decoder_started = true;
    return;
  }

  if (decoder_started && mp3 && mp3->isRunning()) {
    static int log_counter = 0;
    if (log_counter++ % 500 == 0) {
      Serial.printf("ring_available=%d  ring_write=%d  ring_read=%d\n",
                    ring_available(), ring_write, ring_read);
    }

    if (!mp3->loop()) {
      mp3->stop();
      Serial.println("MP3 done.");
    }

    // Yield to BT task — prevents watchdog
    delay(1);
  }
}