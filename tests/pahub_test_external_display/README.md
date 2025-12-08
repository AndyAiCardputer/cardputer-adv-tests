# PA Hub Test for Cardputer-Adv

Test application for PA Hub (PCA9548A) with joystick, scroll encoders, and keyboard on external ILI9488 display.

## Overview

This test demonstrates how to use the M5Stack Unit PaHub v2.1 (PCA9548A I2C multiplexer) to connect multiple I2C devices to Cardputer-Adv's PORT.A. All input events are displayed on an external ILI9488 display (480×320) and logged to Serial Monitor.

## Connected Devices

- **PA Hub (PCA9548A)** - Address 0x70 on PORT.A (GPIO 2/1)
  - **Channel 0:** Joystick2 (address 0x63)
  - **Channel 1:** Scroll Button A (address 0x40)
  - **Channel 2:** Scroll Button B (address 0x40)
  - **Channel 3:** CardKeyBoard (address 0x5F)

## Hardware Connections

### ILI9488 Display via EXT 2.54-14P:

| Display Pin | Cardputer Pin | GPIO | Function |
|-------------|---------------|------|----------|
| VCC         | PIN 2         | -    | 5VIN     |
| GND         | PIN 4         | -    | GND      |
| SCK         | PIN 7         | 40   | SPI Clock|
| MOSI        | PIN 9         | 14   | SPI Data |
| MISO        | PIN 11        | 39   | SPI MISO |
| CS          | PIN 13        | 5    | Chip Select |
| DC          | PIN 5         | 6    | Data/Command |
| RST         | PIN 1         | 3    | Reset    |

### PA Hub:

- Connect to **PORT.A** (GPIO 2/1)
- **VCC** → 5V or 3.3V
- **GND** → GND
- **SDA** → PORT.A SDA (GPIO 2)
- **SCL** → PORT.A SCL (GPIO 1)

## Features

1. **PA Hub Initialization** - Detection and channel switching
2. **Joystick2** - X/Y coordinates and button reading
3. **Scroll Encoders A/B** - Button state with debounce and edge detection, incremental encoder reading
4. **CardKeyBoard** - Raw keycode reading

## Display Output

The test displays on external display:
- Device initialization status
- `KB: 0xXX` - Keyboard events with raw keycode (Cyan)
- `Joy: X=XXX Y=XXX Btn=X` - Joystick coordinates and button (Yellow)
- `Scroll A/B button: PRESSED/RELEASED` - Button events (Magenta)
- `Enc A/B: +/-X (Total: X)` - Encoder increments (Blue)

## Serial Monitor Output

All events are also logged to Serial Monitor with `[APP]` prefix:
```
[APP] KB: 0x0D
[APP] Joy: X=127 Y=200 Btn=1
[APP] Scroll A button: PRESSED
[APP] Enc A: +5 (Total: 5)
```

**Note:** Requires `-DARDUINO_USB_CDC_ON_BOOT=1` flag in `platformio.ini` to enable USB-Serial/JTAG for Arduino Serial.

## Display Optimization

- **Partial screen updates** - Only changed lines are redrawn
- **Batched rendering** - Multiple updates are batched and rendered once per loop
- **Color coding** - Different colors for different event types
- **Scroll support** - Automatic scrolling when buffer is full

## Usage

After flashing, the test automatically:
1. Initializes external ILI9488 display
2. Initializes PA Hub and detects all connected devices
3. Starts polling devices
4. Displays events on screen and Serial Monitor

Press keys/buttons and observe events on display and Serial Monitor.

## Building and Flashing

```bash
cd cardputer/cardputer_adv/tests-adv/pahub_test_external_display
pio run --target upload
```

## Serial Monitor

```bash
pio device monitor --baud 115200
```

## Requirements

- PlatformIO
- M5Stack Cardputer-Adv
- M5Stack Unit PaHub v2.1
- External ILI9488 display (480×320)
- M5Stack Joystick2 Unit
- M5Stack Unit-Scroll (×2)
- M5Stack CardKeyBoard

## Libraries

- `M5Cardputer@^1.1.1`
- `M5Unified@^0.2.10`
- `M5GFX@^0.2.17`

## Documentation

See [PA_HUB_DOCUMENTATION.md](PA_HUB_DOCUMENTATION.md) for detailed PA Hub technical documentation.

## License

This project is provided as-is for testing and educational purposes.
