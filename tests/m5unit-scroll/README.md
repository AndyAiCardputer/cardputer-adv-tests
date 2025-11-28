# M5Unit-Scroll Test - External Display

**Version:** 1.0  
**For:** M5Stack Cardputer-Adv  
**Module:** M5Unit-Scroll (encoder/scroll)  
**Display:** ILI9488 (480×320) - External display

---

## Description

Complete test for M5Unit-Scroll encoder/scroll module with external ILI9488 display support.

**Status:** ✅ **Working perfectly!**

**Features:**
- ✅ Rotary encoder navigation (incremental reading)
- ✅ Button press detection
- ✅ RGB LED control (GRB color order)
- ✅ Scrollable list with optimized display (no flicker)
- ✅ I2C error handling and recovery
- ✅ Soft I2C bus reset on critical errors
- ✅ Multiple detection methods for STM32F030 compatibility

---

## Hardware Requirements

- **M5Stack Cardputer-Adv** (ESP32-S3)
- **M5Unit-Scroll** module (I2C, address 0x40)
- **ILI9488 external display** (480×320, optional but recommended)
- **USB-C cable** for programming

---

## Connections

### M5Unit-Scroll (PORT.A - Grove HY2.0-4P)

| M5Unit-Scroll Pin | Cardputer-Adv PORT.A | GPIO | Function |
|-------------------|----------------------|------|----------|
| SDA | G2 | GPIO 2 | I2C Data |
| SCL | G1 | GPIO 1 | I2C Clock |
| GND | GND | - | Ground (REQUIRED!) |
| 5V | 5V | - | Power |

### External ILI9488 Display (EXT 2.54-14P Header)

| Display Pin | Cardputer-Adv Pin | GPIO | Function |
|-------------|-------------------|------|----------|
| VCC | PIN 2 | - | 5V Power |
| GND | PIN 4 | - | Ground |
| SCK | PIN 7 | GPIO 40 | SPI Clock |
| MOSI | PIN 9 | GPIO 14 | SPI Data |
| MISO | PIN 11 | GPIO 39 | SPI MISO (not used) |
| CS | PIN 13 | GPIO 5 | Chip Select |
| DC | PIN 5 | GPIO 6 | Data/Command |
| RST | PIN 1 | GPIO 3 | Reset |

---

## Installation

### 1. Install Libraries

**Arduino IDE:**
- Open **Tools → Manage Libraries**
- Search and install:
  - `M5Cardputer`
  - `M5Unified`
  - `M5GFX`

### 2. Select Board

- **Tools → Board → M5Stack Cardputer-Adv**
- **Tools → Port → [Your USB port]**

### 3. Upload Sketch

- Open `unitscroll_test_external_display.ino`
- Click **Upload**
- Open Serial Monitor (115200 baud)

---

## Usage

### Main Screen

- **Rotate encoder** → Changes encoder value
- **Press button** → Resets encoder to 0
- **Press SPACE** → Switch to scroll test screen

### Scroll Test Screen

- **Rotate encoder** → Navigate through list (up/down)
- **Press button** → Check/uncheck selected item
- **Press SPACE** → Return to main screen

### LED Feedback

- **Green** → Rotate right (down)
- **Red** → Rotate left (up)
- **Blue** → Button pressed / Item checked
- **Off** → No activity

---

## I2C Protocol

### Registers

| Register | Name | Type | Description |
|----------|------|------|-------------|
| 0x10 | Encoder Value | R (16-bit) | Absolute encoder value |
| 0x20 | Button Status | R (8-bit) | Button state (0/1) |
| 0x30 | RGB LED | W (3 bytes) | RGB LED control (GRB order) |
| 0x40 | Reset Encoder | W (8-bit) | Write 1 to reset |
| 0x50 | Incremental Encoder | R (16-bit) | Incremental value (resets after read!) |
| 0xF0 | Info Register | R (16 bytes) | Bootloader/FW version |

### Important Notes

- **I2C Bus:** Uses `Wire` (shared with keyboard), NOT `Wire1`
- **Speed:** 50 kHz (STM32F030 compatible)
- **Delays:** 500µs before, 300µs after `requestFrom()`
- **Color Order:** GRB (not RGB!) for WS2812 LED

---

## Troubleshooting

### Module Not Found

**Check:**
1. ✅ LED on module is ON (power check)
2. ✅ GND is connected (required!)
3. ✅ Connected to PORT.A (not other connectors)
4. ✅ Using `Wire`, not `Wire1`

**Try:**
- Power cycle module (unplug/replug)
- Try different I2C speeds (50kHz, 100kHz)
- Check Serial Monitor for detection messages

### I2C Errors

**Symptoms:** `I2C transaction unexpected nack detected`

**Solutions:**
1. Increase reading interval (50ms minimum)
2. Add delays for STM32F030
3. Implement error recovery (automatic in code)
4. Add delay after screen switch (500ms)

### Display Issues

**If display doesn't work:**
- Check all SPI connections
- Verify power (5V)
- Check initialization order (display BEFORE Cardputer)

**If display flickers:**
- Code uses optimized partial updates
- Should not flicker with current implementation

---

## Code Structure

```
unitscroll_test_external_display.ino
├── Display Configuration (ILI9488)
├── I2C Configuration (Wire, PORT.A)
├── State Variables
├── i2cBusReset() - Soft I2C reset
├── readScrollRegister() - I2C read with error handling
├── setScrollLEDFast() - RGB LED control
├── drawScrollTest() - Optimized list rendering
├── setup() - Initialization
└── loop() - Main loop with error recovery
```

---

## Performance

- **Reading interval:** 50ms (20 reads/second)
- **Display updates:** Partial (no flicker)
- **Error recovery:** Automatic soft reset
- **Memory usage:** Optimized for ESP32-S3

---

## Documentation

- **[User Guide](../../docs/M5UNIT_SCROLL_USER_GUIDE.md)** - User-friendly guide
- **[Technical Guide](../../docs/M5UNIT_SCROLL_GUIDE.md)** - Technical reference
- **[Display Guide](../../docs/EXTERNAL_DISPLAY_ILI9488_GUIDE.md)** - Display setup

---

## Credits

- **AndyAiCardputer** - Testing, bug reports
- **AI Assistant** - Code, optimization
- **M5Stack** - Hardware and libraries

---

**Last Updated:** January 13, 2025  
**Tested On:** M5Stack Cardputer-Adv (ESP32-S3)  
**Module:** M5Unit-Scroll (STM32F030, I2C address 0x40)

