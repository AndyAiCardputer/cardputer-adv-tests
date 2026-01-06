# USB Gamepad Display Test for M5Stack Tab5

Visual USB gamepad test with real-time on-screen display.

## Features

- Real-time gamepad state visualization on Tab5 display
- Button states (green when pressed, gray when released)
- Analog stick positions (visual circles with crosshair)
- Trigger values (progress bars)
- D-Pad direction display
- Gamepad VID/PID information
- Supports DualSense (PS5) and generic USB HID gamepads

## Building

```bash
cd /Users/a15/A_AI_Project/cardputer/tab5/tests/usb_gamepad_display_test
export IDF_PATH=/Users/a15/A_AI_Project/esp-idf-official
source $IDF_PATH/export.sh
idf.py build
```

## Flashing

```bash
idf.py -p /dev/cu.usbmodem1434301 flash
```

## Monitoring

```bash
idf.py -p /dev/cu.usbmodem1434301 monitor
```

## Usage

1. Flash the firmware to your Tab5
2. Connect USB gamepad to USB-A port on Tab5
3. Display will show:
   - Gamepad information (VID, PID)
   - Button states (A, B, X, Y, LB, RB, BACK, START, L3, R3, HOME)
   - Stick positions (left and right)
   - Trigger bars (L2 and R2)
   - D-Pad direction

## Display Layout

- **Top**: Title and gamepad VID/PID
- **Buttons**: 3 rows of buttons (A/B/X/Y, LB/RB/BACK/START, L3/R3/HOME)
- **D-Pad**: Current direction text
- **Sticks**: Visual circles showing stick positions
- **Triggers**: Progress bars with numeric values

## Supported Gamepads

- PlayStation 5 DualSense (VID: 0x054C, PID: 0x0CE6) - Full support
- Generic USB HID gamepads - Basic support

## Notes

- Display updates at ~20 FPS
- Button states are shown in real-time
- Sticks show position relative to center
- Triggers show analog values (0-255)
=======
# M5Stack Cardputer-Adv Tests

**Repository:** Collection of test sketches for M5Stack Cardputer-Adv  
**Author:** AndyAiCardputer  
**License:** MIT  
**Last Updated:** November 28, 2025

---

## 📋 Overview

This repository contains working test sketches for various M5Stack units and components compatible with **M5Stack Cardputer-Adv** (ESP32-S3).

All tests are tested and working. Each test includes:
- ✅ Complete source code
- ✅ README with setup instructions
- ✅ Hardware connection diagrams
- ✅ Troubleshooting guide

---

## 🧪 Available Tests

### 1. M5Unit-Scroll Test
**Location:** `tests/m5unit-scroll/`  
**Status:** ✅ **Working perfectly!**

Test for M5Unit-Scroll encoder/scroll module with external ILI9488 display.

**Features:**
- Rotary encoder navigation
- Button press detection
- RGB LED control
- Scrollable list with optimized display (no flicker)
- I2C error handling and recovery

**Hardware:**
- M5Stack Cardputer-Adv
- M5Unit-Scroll module (I2C, address 0x40)
- ILI9488 external display (480×320, optional)

**Documentation:**
- [User Guide](docs/M5UNIT_SCROLL_USER_GUIDE.md)
- [Technical Guide](docs/M5UNIT_SCROLL_GUIDE.md)

---

### 2. I2C Keyboard Test (CardKeyBoard)
**Location:** `tests/i2c-keyboard/`  
**Status:** ✅ **Working perfectly!**

Python-like terminal with external I2C keyboard (CardKeyBoard) support on external ILI9488 display.

**Features:**
- External I2C keyboard CardKeyBoard (address 0x5F)
- Full key combination support (Key, Sym+Key, Shift+Key, Fn+Key)
- Command history navigation with arrow keys
- Terminal commands (help, clear, info, test, echo, version, print)
- Scrolling support (Fn+. / Fn+;)
- Optimized display updates (no flicker)

**Hardware:**
- M5Stack Cardputer-Adv
- CardKeyBoard module (I2C, address 0x5F)
- ILI9488 external display (480×320, optional)

**Documentation:**
- [Test README](tests/i2c-keyboard/README.md)
- [Key Codes Reference](docs/CARDKEYBOARD_KEYCODES.md)

---

## 🚀 Quick Start

### Prerequisites

- **Hardware:**
  - M5Stack Cardputer-Adv
  - USB-C cable
  - Test module (M5Unit-Scroll, CardKeyBoard, etc.)
  - External ILI9488 display (optional, for display tests)

- **Software:**
  - Arduino IDE 2.0+ or PlatformIO
  - M5Stack libraries:
    - `M5Cardputer` (for Cardputer-Adv)
    - `M5Unified`
    - `M5GFX`

### Installation

1. **Clone this repository:**
   ```bash
   git clone https://github.com/AndyAiCardputer/cardputer-adv-tests.git
   cd cardputer-adv-tests
   ```

2. **Install required libraries:**
   - Open Arduino IDE
   - Go to **Tools → Manage Libraries**
   - Search and install:
     - `M5Cardputer`
     - `M5Unified`
     - `M5GFX`

3. **Select board:**
   - **Tools → Board → M5Stack Cardputer-Adv**

4. **Open test sketch:**
   - Navigate to `tests/m5unit-scroll/`
   - Open `unitscroll_test_external_display.ino`

5. **Upload and test:**
   - Connect Cardputer-Adv via USB-C
   - Click **Upload**
   - Open Serial Monitor (115200 baud)

---

## 📚 Documentation

All documentation is available in the `docs/` folder:

- **[M5Unit-Scroll User Guide](docs/M5UNIT_SCROLL_USER_GUIDE.md)** - User-friendly guide
- **[M5Unit-Scroll Technical Guide](docs/M5UNIT_SCROLL_GUIDE.md)** - Technical reference for developers
- **[CardKeyBoard Key Codes Reference](docs/CARDKEYBOARD_KEYCODES.md)** - Complete key codes table for I2C keyboard
- **[External Display ILI9488 Guide](docs/EXTERNAL_DISPLAY_ILI9488_GUIDE.md)** - Display setup and optimization

---

## 🔧 Hardware Connections

### M5Unit-Scroll

**PORT.A (Grove HY2.0-4P):**
- SDA → GPIO 2 (G2)
- SCL → GPIO 1 (G1)
- GND → GND
- 5V → 5V

### CardKeyBoard (I2C Keyboard)

**PORT.A (Grove HY2.0-4P):**
- SDA → GPIO 2 (G2)
- SCL → GPIO 1 (G1)
- GND → GND (REQUIRED!)
- 5V → 5V

### External ILI9488 Display (Optional)

**EXT 2.54-14P Header:**
- VCC → PIN 2 (5VIN)
- GND → PIN 4 (GND)
- SCK → PIN 7 (GPIO 40)
- MOSI → PIN 9 (GPIO 14)
- MISO → PIN 11 (GPIO 39)
- CS → PIN 13 (GPIO 5)
- DC → PIN 5 (GPIO 6)
- RST → PIN 1 (GPIO 3)

---

## ⚠️ Important Notes

### I2C Bus Sharing

**CRITICAL:** M5Unit-Scroll uses the **same I2C bus (`Wire`) as the Cardputer keyboard**, NOT `Wire1`. Both devices share PORT.A (GPIO 2/1).

**Correct initialization:**
```cpp
Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);  // Use Wire, not Wire1!
```

### Display Initialization Order

**CRITICAL:** External display must be initialized **BEFORE** `M5Cardputer.begin()`:

```cpp
// ✅ CORRECT order
externalDisplay.init();        // Display FIRST
M5Cardputer.begin(true);       // Then Cardputer
M5.Display.setBrightness(0);   // Disable built-in display
```

### STM32F030 Compatibility

M5Unit-Scroll uses STM32F030 MCU which requires:
- I2C speed: 50 kHz (recommended)
- Delays: 500µs before, 300µs after `requestFrom()`
- Pull-up resistors: Enable on ESP32
- Clock stretching: Supported

---

## 🐛 Troubleshooting

### Module Not Found

**Symptoms:** `moduleFound = false`, no I2C response

**Solutions:**
1. Check power (LED should be ON)
2. Verify GND connection (required!)
3. Check I2C pins (GPIO 2/1 for PORT.A)
4. Try different I2C speeds (50kHz, 100kHz)
5. Power cycle module (unplug/replug)

### I2C NACK Errors

**Symptoms:** `I2C transaction unexpected nack detected`

**Solutions:**
1. Increase reading interval (50ms minimum)
2. Add delays for STM32F030 (500µs/300µs)
3. Implement error recovery (`i2cBusReset()`)
4. Add delay after screen switch (500ms)

### Display Flickering

**Symptoms:** Screen flickers on updates

**Solutions:**
1. Use `startWrite()` / `endWrite()` for grouping
2. Use `fillRect()` instead of `fillScreen()`
3. Implement partial updates
4. Track previous values to avoid unnecessary redraws

---

## 📝 Contributing

Found a bug or want to add a test? Feel free to:
- Open an issue
- Submit a pull request
- Share your test results

---

## 🙏 Credits

- **AndyAiCardputer** - Testing, bug reports, ideas
- **AI Assistant** - Code, debugging, documentation
- **M5Stack** - Hardware and libraries

---

## 📄 License

**MIT License** - Feel free to use in your projects!

---

## 🔗 Related Repositories

- [ZX Spectrum Emulator](https://github.com/AndyAiCardputer/zx-spectrum-cardputer)
- [Audio Player](https://github.com/AndyAiCardputer/unit_audio-player-cardputer)

---

**Made with ❤️ by AndyAiCardputer**

**Last Updated:** November 28, 2025
>>>>>>> ae385f652a99408b9efa63b99fcf5d48379cd28e

