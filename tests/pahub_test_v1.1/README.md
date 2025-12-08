# PA Hub Test for Cardputer V1.1

Test application for PA Hub (PCA9548A) with joystick, scroll encoders, and keyboard on built-in display.

## Overview

This test demonstrates how to use the M5Stack Unit PaHub v2.1 (PCA9548A I2C multiplexer) to connect multiple I2C devices to Cardputer V1.1's PORT.A. All input events are displayed on the built-in display (240×135) and logged to Serial Monitor.

## Connected Devices

- **PA Hub (PCA9548A)** - Address 0x70 on PORT.A (GPIO 2/1)
  - **Channel 0:** Joystick2 (address 0x63)
  - **Channel 1:** Scroll Button A (address 0x40)
  - **Channel 2:** Scroll Button B (address 0x40)
  - **Channel 3:** CardKeyBoard (address 0x5F)

## Hardware Connections

### PA Hub:

- Connect to **PORT.A** (Grove HY2.0-4P)
- **VCC** → 5V or 3.3V
- **GND** → GND
- **SDA** → PORT.A SDA (GPIO 2, G2)
- **SCL** → PORT.A SCL (GPIO 1, G1)

## Important Notes for V1.1

- **Uses `Wire` instead of `Wire1`** - PORT.A shares I2C controller with keyboard
- **Keyboard is always enabled** - `M5Cardputer.begin(true)` is required
- **Wire reconfiguration** - After `M5Cardputer.begin()`, Wire is reconfigured for PORT.A (GPIO 2/1)
- **Pull-up resistors** - Must be configured for PORT.A pins (GPIO 2/1)
- **Built-in display** - Uses `M5.Display` (not `M5Cardputer.Display`) with `setRotation(1)` and `fillScreen(TFT_BLACK)`

## Features

1. **PA Hub Initialization** - Detection and channel switching
2. **Joystick2** - X/Y coordinates and button reading
3. **Scroll Encoders A/B** - Button state with debounce and edge detection, incremental encoder reading
4. **CardKeyBoard** - Raw keycode reading

## Display Output

The test displays on built-in display (240×135):
- Header: "PA Hub Test" (Green)
- Device status: KB, Joy, A, B availability (White)
- Encoder values: Enc A and Enc B current values (Cyan)
- Last event: Most recent input event (Yellow, truncated if >20 chars)

## Serial Monitor Output

All events are also logged to Serial Monitor with `[APP]` prefix:
```
[APP] KB: 0x0D
[APP] Joy: X=127 Y=200 Btn=1
[APP] Scroll A button: PRESSED
[APP] Enc A: +5 (5)
```

**Note:** Requires `-DARDUINO_USB_CDC_ON_BOOT=1` flag in `platformio.ini` to enable USB-Serial/JTAG for Arduino Serial.

## Usage

After flashing, the test automatically:
1. Initializes Cardputer V1.1 (keyboard enabled)
2. Initializes built-in display with rotation and colors
3. Reconfigures Wire for PORT.A (GPIO 2/1)
4. Initializes PA Hub and detects all connected devices
5. Starts polling devices
6. Displays events on built-in display and Serial Monitor

Press keys/buttons and observe events on display and Serial Monitor.

## Building and Flashing

```bash
cd cardputer/cardputer_tests/pahub_test_external_display
pio run --target upload
```

## Serial Monitor

```bash
pio device monitor --baud 115200
```

## Requirements

- PlatformIO
- M5Stack Cardputer V1.1
- M5Stack Unit PaHub v2.1
- M5Stack Joystick2 Unit
- M5Stack Unit-Scroll (×2)
- M5Stack CardKeyBoard (optional)

## Differences from Cardputer-Adv Version

1. **I2C Bus:** Uses `Wire` (shared with keyboard) instead of `Wire1`
2. **Keyboard:** Always enabled (`begin(true)`) - cannot be disabled on V1.1
3. **Wire Reconfiguration:** Wire is reconfigured after `M5Cardputer.begin()` to use PORT.A pins
4. **Pull-up Resistors:** Must be explicitly configured for PORT.A pins
5. **Display:** Uses built-in display (240×135) instead of external ILI9488 (480×320)
6. **Display API:** Uses `M5.Display` with `setRotation(1)` and `fillScreen(TFT_BLACK)`

## Troubleshooting

### PA Hub not detected
- Check PORT.A connections (GPIO 2/1)
- Verify pull-up resistors are configured
- Check I2C address (should be 0x70)

### Devices not found
- Verify PA Hub channel selection
- Check device addresses (Joystick2: 0x63, Scroll: 0x40, Keyboard: 0x5F)
- Ensure devices are powered

### Display not working
- Check that `M5.Display.setRotation(1)` is called
- Verify `M5.Display.fillScreen(TFT_BLACK)` is used (not `clear()`)
- Ensure `setTextColor()` uses two parameters (text color, background color)

### Serial Monitor not showing logs
- Ensure `-DARDUINO_USB_CDC_ON_BOOT=1` is set in `platformio.ini`
- Check USB cable connection
- Try different USB port

## License

See main project license.
