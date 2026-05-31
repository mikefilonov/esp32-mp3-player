# ESP32 Bluetooth MP3 Player (A2DP Source)

An elegant, robust, and feature-rich ESP32-based MP3 player that functions as a **Bluetooth A2DP Source (Transmitter)**. It reads MP3 files from an SD card and streams them to your Bluetooth speaker or headphones, with advanced two-way hardware volume control, absolute volume synchronization, smart SD card hot-swapping, dynamic physical controls, and a multi-color status NeoPixel indicator.

<img width="4032" height="3024" alt="IMG_1467" src="https://github.com/user-attachments/assets/90d8d17b-fb94-4b09-8547-4d11cdcd1ab0" />

---

## 🌟 Features

*   **Bluetooth A2DP Audio Streaming**: Functions as a Bluetooth Transmitter, searching for and connecting directly to nearby Bluetooth speakers or headphones.
*   **Dual-Control Architecture (Two-Way Control)**:
    *   **Physical Controls**: Uses an **Adafruit Seesaw I2C Rotary Encoder** with a built-in pushbutton and NeoPixel LED.
    *   **Remote Controls (AVRC Passthrough)**: Supports Bluetooth AVRCP, allowing you to use the physical Play/Pause, Forward, and Backward buttons on your Bluetooth speaker/headphones to control the ESP32!
*   **Absolute Volume Control (Zero Quality Loss)**:
    *   Integrates direct AVRC absolute volume command synchronization (`esp_avrc_ct_send_set_absolute_volume_cmd`).
    *   **No PCM Software Scaling**: Keeps the digital signal intact at full dynamic range; the volume is adjusted directly on the receiving speaker's hardware DAC.
*   **State-Saving Volume Memory (NVS)**: Uses ESP32 Non-Volatile Storage (NVS) to save your last volume level. Restores it on boot, preventing abrupt volume jumps.
*   **Dynamic LED Status Indicator**: The integrated Seesaw NeoPixel LED communicates player state at a glance:
    *   🔵 **Blue**: Normal boot, searching, or reconnecting to Bluetooth.
    *   🟣 **Purple/Magenta**: Pairing mode active.
    *   🟢 **Green**: Audio track is playing.
    *   🟠 **Orange**: Audio track is paused.
    *   🔴 **Red**: Audio is stopped but connected to Bluetooth.
    *   🟡 **Yellow**: SD card is missing or disconnected.
*   **Smart SD Card Management**:
    *   Fully supports hot-swapping (unplugging/replugging the SD card).
    *   Dynamic background checking halts playback on removal, clears state, and automatically resumes when a card is inserted and Bluetooth is connected.
    *   Recursively scans `.mp3` files in the root folder, filtering out hidden system files (e.g., Mac `.DS_Store` or dotfiles).
*   **Forced Pairing Button**: Hold the rotary encoder button down during bootup to force-clear any cached connection history and pair with a new speaker or headphone instantly.

---

## 🛠️ Hardware Requirements

1.  **ESP32 Development Board** (e.g., ESP32-WROOM-32).
2.  **SPI MicroSD Card Module** + MicroSD Card (formatted as FAT16/FAT32).
3.  **Adafruit Seesaw I2C Rotary Encoder** (e.g., Adafruit I2C QT Rotary Encoder).
4.  **Bluetooth Speaker or Headphones**.

### Schematic & Wiring

#### 1. MicroSD Card Module (SPI Interface)
| SD Card Pin | ESP32 Pin (Default VSPI) | Description |
| :--- | :--- | :--- |
| **CS** | **Pin 5** | Chip Select |
| **MOSI** | **Pin 23** | Master Out Slave In |
| **MISO** | **Pin 19** | Master In Slave Out |
| **SCK** | **Pin 18** | Serial Clock |
| **VCC** | **3.3V / 5V** | Power |
| **GND** | **GND** | Ground |

#### 2. Adafruit Seesaw Rotary Encoder (I2C Interface)
| Seesaw Pin | ESP32 Pin (Default I2C) | Description |
| :--- | :--- | :--- |
| **SDA** | **Pin 21** | I2C Data |
| **SCL** | **Pin 22** | I2C Clock |
| **VCC** | **3.3V / 5V** | Power |
| **GND** | **GND** | Ground |

*Note: The Seesaw I2C address defaults to `0x36` in this project.*

---

## 🕹️ Control Layout & UI Behavior

### Physical Controls (Seesaw Rotary Encoder)

*   **Turn Encoder (Normal)**: Adjust volume up or down.
*   **Click Button**: Play/Pause.
*   **Chorded Click + Turn (Hold Button and Spin)**:
    *   Spin Clockwise: Next Track.
    *   Spin Counter-Clockwise: Previous Track.
*   **Hold Button on Bootup**: Clears the pairing history (forces a new pairing session).

### Remote Bluetooth Controls (AVRCP Passthrough)

Using the buttons on your paired Bluetooth headphones or speaker:
*   **Play / Pause / Stop**: Triggers Play/Pause.
*   **Forward / Next**: Next Track.
*   **Backward / Previous**: Previous Track.
*   **Volume Up / Down**: Controls absolute hardware volume synced with the ESP32.

---

## 📁 File Structure

The project code is clean, modular, and organized as follows:

*   [`esp32-mp3.ino`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/esp32-mp3.ino): Setup entry point, task looping, NVS initialization, boot pairing checks, and startup sequence.
*   [`controller.h`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/controller.h): The `Mp3PlayerController` which coordinates input actions, audio state changes, volume management, and LED status lighting. All heavy processing (I/O, memory allocations) is safely deferred from ISR/Bluetooth callbacks to the main loop task.
*   [`navigation.h`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/navigation.h) / [`navigation.cpp`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/navigation.cpp): Manages the I2C connection to the Adafruit Seesaw, reads encoder increments, parses button clicks and chorded spins, and updates the NeoPixel LED.
*   [`player.h`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/player.h) / [`player.cpp`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/player.cpp): Handles the Bluetooth A2DP Source logic, ring buffer queuing, NVS volume persistence, and MP3 decoding.
*   [`sdcard.h`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/sdcard.h) / [`sdcard.cpp`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/sdcard.cpp): Manages physical SD card mounting, dynamic connection status checking, and bidirectional file traversal (.mp3 files only, with wraparound).
*   [`bt_navigation.h`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/bt_navigation.h) / [`bt_navigation.cpp`](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/bt_navigation.cpp): Registers the Bluetooth AVRCP callback events, translating remote speaker clicks into controller navigation calls.

---

## 🚀 Setup & Installation

### 1. Library Dependencies

Ensure the following libraries are installed in your Arduino IDE or PlatformIO project:

*   **[ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP)** by Phil Schatzmann
*   **[ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio)** by Earle F. Philhower, III
*   **[Adafruit Seesaw Library](https://github.com/adafruit/Adafruit_Seesaw)** by Adafruit

### 2. Preparing the SD Card

1.  Format your MicroSD Card to FAT32.
2.  Copy your favorite `.mp3` files directly onto the root of the SD card. Do not place them in subdirectories, as the current system looks in the root directory `/`.
3.  Avoid long filenames exceeding 250 characters.

### 3. Flash the ESP32

1.  Open `esp32-mp3.ino` in the Arduino IDE.
2.  Select your ESP32 Board model and the appropriate COM port.
3.  Upload the sketch to the board.
4.  Open the Serial Monitor at **9600 baud** to view logs.

### 4. Pairing with Bluetooth Speaker

1.  **First Boot**: Since there is no cached speaker history, the ESP32 will automatically start in pairing mode. The NeoPixel will turn **Purple/Magenta**.
2.  Turn on your Bluetooth speaker/headphones and put them in pairing mode. Place them near the ESP32.
3.  The ESP32 will scan, discover, and automatically connect to your device. Once connected, the NeoPixel will turn **Green** and immediately start playing the first track on the SD card.
4.  **subsequent Boots**: The ESP32 will attempt to reconnect to the last paired device automatically (NeoPixel will show **Blue** during search).
5.  **Forcing New Pairing**: To connect a new speaker, turn off the ESP32, hold down the rotary encoder button, and power the ESP32 back on. Keep the button pressed until the Serial Monitor displays pairing reset, and the NeoPixel changes to **Purple**.

---

## 🤝 Contributing

Contributions, bug reports, and pull requests are welcome! If you have suggestions for seeks, playlists, or UI enhancements, feel free to open an issue or submit a PR.

---

## 📄 License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0). See the [LICENSE](file:///Users/mikefilonov/Documents/Arduino/esp32-mp3/LICENSE) file for more information.
