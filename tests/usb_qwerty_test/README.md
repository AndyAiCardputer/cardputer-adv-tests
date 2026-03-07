# USB QWERTY Keyboard Test for M5Stack Tab5

USB Host HID keyboard test for M5Stack Tab5 (ESP32-P4). Connects a standard USB QWERTY keyboard and displays typed text on the 1280x720 screen.

## Features

- USB Host HID driver for standard USB keyboards
- Full key mapping (A-Z, 0-9, symbols, modifiers)
- Shift support (uppercase, symbols)
- On-screen display with 8x8 bitmap font (3x scale)
- Landscape mode (1280x720)
- Visual keyboard status indicator

## Hardware

- M5Stack Tab5 (ESP32-P4)
- Standard USB keyboard connected to Tab5 USB-A port

## Building

```bash
# Set up ESP-IDF v6.1 (v5.x may also work)
. $IDF_PATH/export.sh

cd tests/usb_qwerty_test
idf.py build
idf.py flash monitor
```

## Documentation

See [USB_KEYBOARD_GUIDE.md](USB_KEYBOARD_GUIDE.md) for a detailed guide on how USB Host HID keyboard input works on ESP32.

## Target

- Chip: ESP32-P4
- Framework: ESP-IDF v6.1 (v5.x may also work)
- Display: 720x1280 MIPI DSI (landscape 1280x720)
