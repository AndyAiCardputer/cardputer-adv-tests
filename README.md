# Cardputer-Adv Tests

Test sketches and applications for M5Stack Cardputer-Adv and M5Stack Tab5.

## Overview

This repository contains various test applications and examples for M5Stack Cardputer-Adv (ESP32-S3) and M5Stack Tab5 (ESP32-P4) devices. These tests demonstrate hardware integration, peripheral usage, and development patterns.

## Available Tests

### Cardputer-Adv Tests (Arduino/PlatformIO)

#### [I2C Keyboard Test](tests/i2c-keyboard/)
Python-like terminal with external I2C keyboard (CardKeyBoard) support. Works on external ILI9488 display with full key combination support.

**Features:**
- External I2C keyboard CardKeyBoard (address 0x5F)
- Full key combination support (Sym, Shift, Fn)
- Command history navigation
- Terminal commands (help, clear, info, test, echo, version)

#### [M5Unit Scroll Test](tests/m5unit-scroll/)
Test application for M5Stack Unit Scroll encoder connected via I2C.

#### [PA Hub Test (External Display)](tests/pahub_test_external_display/)
Test application for PA Hub (PCA9548A) with joystick, scroll encoders, and keyboard on external ILI9488 display.

**Connected Devices:**
- PA Hub (PCA9548A) - Address 0x70
  - Channel 0: Joystick2 (0x63)
  - Channel 1: Scroll Button A (0x40)
  - Channel 2: Scroll Button B (0x40)
  - Channel 3: CardKeyBoard (0x5F)

#### [PA Hub Test v1.1](tests/pahub_test_v1.1/)
Alternative PA Hub test implementation.

### Tab5 Tests (ESP-IDF)

#### [USB Gamepad Display Test](tests/usb_gamepad_display_test/)
Visual USB gamepad test with real-time on-screen display for M5Stack Tab5.

**Features:**
- Real-time gamepad state visualization on Tab5 display
- Button states (green when pressed, gray when released)
- Analog stick positions (visual circles with crosshair)
- Trigger values (progress bars)
- D-Pad direction display
- Supports DualSense (PS5) and generic USB HID gamepads

**Hardware:**
- M5Stack Tab5 (ESP32-P4)
- USB-A port for gamepad connection
- MIPI DSI display (ST7123 controller, 720×1280)

## Building and Flashing

### Cardputer-Adv Tests (Arduino/PlatformIO)

1. Open the test sketch in Arduino IDE or PlatformIO
2. Select board: **M5Stack Cardputer-Adv**
3. Upload sketch

### Tab5 Tests (ESP-IDF)

1. Set up ESP-IDF environment:
```bash
export IDF_PATH=/path/to/esp-idf
source $IDF_PATH/export.sh
```

2. Navigate to test directory:
```bash
cd tests/usb_gamepad_display_test
```

3. Build:
```bash
idf.py build
```

4. Flash:
```bash
idf.py -p /dev/cu.usbmodemXXXXX flash
```

5. Monitor:
```bash
idf.py -p /dev/cu.usbmodemXXXXX monitor
```

## Hardware Requirements

### Cardputer-Adv
- M5Stack Cardputer-Adv (ESP32-S3)
- Various M5Stack Units (PA Hub, Scroll, Joystick2, CardKeyBoard)
- External ILI9488 display (480×320, optional)

### Tab5
- M5Stack Tab5 (ESP32-P4)
- USB gamepad (DualSense PS5 or generic USB HID)

## Repository Structure

```
cardputer-adv-tests/
├── tests/
│   ├── i2c-keyboard/              # I2C keyboard test (Arduino)
│   ├── m5unit-scroll/             # Scroll encoder test (Arduino)
│   ├── pahub_test_external_display/  # PA Hub test with display (Arduino)
│   ├── pahub_test_v1.1/           # PA Hub test v1.1 (Arduino)
│   └── usb_gamepad_display_test/  # USB gamepad test for Tab5 (ESP-IDF)
└── README.md                      # This file
```

## Documentation

Each test includes its own README.md with detailed:
- Hardware connections
- Usage instructions
- Troubleshooting guides
- Technical details

## Contributing

These tests are part of ongoing development and experimentation with M5Stack hardware. Feel free to use them as reference or starting points for your own projects.

## License

MIT License

## Author

AndyAiCardputer

---

**Last Updated:** December 9, 2025

