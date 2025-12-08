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

