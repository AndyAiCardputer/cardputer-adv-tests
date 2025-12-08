# I2C Keyboard Test (CardKeyBoard)

**Version:** 1.0  
**For:** M5Stack Cardputer-Adv  
**Module:** CardKeyBoard (I2C Keyboard)

## Description

Python-like terminal with external I2C keyboard (CardKeyBoard) support. Works on external ILI9488 display (480×320) with full key combination support.

## Features

- ✅ External I2C keyboard CardKeyBoard (address 0x5F)
- ✅ Works in parallel with built-in keyboard
- ✅ Full key combination support:
  - **Key** - regular characters (a-z, 0-9, space, punctuation)
  - **Sym+Key** - symbols (!, @, #, $, %, ^, &, *, (, ), {, }, [, ], /, \, |, ~, ', ", :, ;, `, +, -, _, =, ?, <, >)
  - **Shift+Key** - uppercase letters (A-Z) and Delete
  - **Fn+Key** - function keys (0x80-0xAF, optional)
- ✅ Command history navigation with arrow keys (Up/Down)
- ✅ Terminal commands: help, clear, info, test, echo, version, print()
- ✅ Scrolling support (Fn+. / Fn+;)
- ✅ Optimized display updates (no flicker)

## Hardware Requirements

- **Device:** M5Stack Cardputer-Adv (ESP32-S3)
- **Module:** CardKeyBoard (I2C keyboard, address 0x5F)
- **Display:** External ILI9488 (480×320, optional but recommended)

## Connection

### CardKeyBoard Connection (PORT.A)

Connect CardKeyBoard to **PORT.A** (Grove HY2.0-4P):

| CardKeyBoard | Cardputer-Adv PORT.A | GPIO | Description |
|--------------|----------------------|------|-------------|
| SDA          | G2                   | GPIO 2 | I2C Data |
| SCL          | G1                   | GPIO 1 | I2C Clock |
| GND          | GND                  | - | Common ground (REQUIRED!) |
| 5V           | 5V                   | - | Power supply |

**Important:** GND **MUST** be connected! Check that keyboard LED is on (power is present).

### ILI9488 Display Connection (EXT 2.54-14P)

| Display Pin | Cardputer-Adv Pin | GPIO | Function |
|-------------|-------------------|------|----------|
| VCC         | PIN 2 (5VIN)      | - | Power |
| GND         | PIN 4 (GND)       | - | Ground |
| CS          | PIN 13            | GPIO 5 | Chip Select |
| RST         | PIN 1             | GPIO 3 | Reset |
| DC          | PIN 5             | GPIO 6 | Data/Command |
| MOSI        | PIN 9             | GPIO 14 | SPI Data |
| SCK         | PIN 7             | GPIO 40 | SPI Clock |
| MISO        | PIN 11            | GPIO 39 | SPI Read (not used) |

## I2C Protocol

- **Address:** 0x5F
- **Speed:** 100 kHz
- **Interface:** I2C (slave device)
- **Protocol:** Sends key codes via `Wire.onRequest()`

### Key Codes

CardKeyBoard automatically sends correct codes based on pressed modifiers:
- **Regular keys:** ASCII codes (32-126)
- **Sym+Key:** Symbol codes (!, @, #, etc.)
- **Shift+Key:** Uppercase letters (A-Z) and Delete (0x7F)
- **Fn+Key:** Function codes (0x80-0xAF)
- **Arrow keys:** 0xB4 (Left), 0xB5 (Up), 0xB6 (Down), 0xB7 (Right)

**Complete key codes reference:** See [docs/CARDKEYBOARD_KEYCODES.md](../../docs/CARDKEYBOARD_KEYCODES.md)

## Usage

1. **Connect hardware:**
   - Connect CardKeyBoard to PORT.A
   - Connect ILI9488 display to EXT connector (optional)
   - Check that keyboard LED is on

2. **Upload sketch:**
   - Open `python_terminal_external_display_i2c_keyboard.ino` in Arduino IDE
   - Select board: **M5Stack Cardputer-Adv**
   - Upload sketch

3. **Use terminal:**
   - Type commands using CardKeyBoard or built-in keyboard
   - Use arrow keys (Up/Down) to navigate command history
   - Use Fn+. / Fn+; to scroll terminal output

## Commands

| Command | Description |
|---------|-------------|
| `help` | Show available commands |
| `clear` | Clear screen |
| `info` | Show system information (battery, CPU, memory) |
| `test` | Test components (keyboard, display, battery) |
| `echo <text>` | Echo text |
| `version` | Show firmware version |
| `print("text")` | Print text (Python-like syntax) |

## Key Combinations

### Navigation

- **Up Arrow (0xB5)** - Previous command in history
- **Down Arrow (0xB6)** - Next command in history
- **Left/Right Arrows** - Reserved for future cursor movement

### Scrolling (Built-in Keyboard)

- **Fn + . (period)** - Scroll down
- **Fn + ; (semicolon)** - Scroll up

### Special Keys

- **Enter** - Execute command
- **Backspace/Delete** - Delete character
- **Tab** - Insert 4 spaces
- **ESC** - Clear input buffer

## Troubleshooting

### Keyboard Not Found

If you see "I2C Keyboard: NOT FOUND":

1. **Check connections:**
   - SDA → GPIO 2 (G2)
   - SCL → GPIO 1 (G1)
   - GND → GND (MUST be connected!)
   - 5V → 5V (check keyboard LED is on)

2. **Check I2C bus:**
   - CardKeyBoard uses shared I2C bus with built-in keyboard
   - Make sure no other devices conflict on PORT.A

3. **Power issues:**
   - Check 5V power supply
   - Keyboard LED should be on if powered correctly

### Keys Not Working

1. **Check key codes:**
   - Open Serial Monitor (115200 baud)
   - Press keys and check codes in logs
   - Compare with [key codes documentation](../../docs/CARDKEYBOARD_KEYCODES.md)

2. **Modifier keys:**
   - CardKeyBoard automatically handles Shift/Sym/Fn modifiers
   - No need to check modifier state separately

3. **Symbol keys:**
   - Use Sym+Key combination for symbols (!, @, #, etc.)
   - CardKeyBoard sends correct symbol code automatically

### Display Issues

If display doesn't work:

1. **Check initialization order:**
   - Display MUST be initialized BEFORE `M5Cardputer.begin()`
   - Code already handles this correctly

2. **Check connections:**
   - Verify all SPI pins are connected correctly
   - Check power supply (5V)

3. **See display guide:**
   - [External Display Guide](../../docs/EXTERNAL_DISPLAY_ILI9488_GUIDE.md)

## Technical Details

### I2C Configuration

- **Bus:** Shared with built-in keyboard (uses `Wire`)
- **Pins:** GPIO 2/1 (PORT.A)
- **Speed:** 100 kHz
- **Pull-up resistors:** Enabled on ESP32

### Display Configuration

- **SPI Host:** SPI3_HOST (HSPI)
- **Speed:** 20 MHz (write), 16 MHz (read)
- **Mode:** 3-wire SPI (no MISO)
- **Rotation:** 180 degrees
- **Color Depth:** 24-bit (RGB888)

### Performance

- **Poll interval:** 50 ms (prevents reading too frequently)
- **Display updates:** Batched (reduces flicker)
- **Memory usage:** ~25 KB (heap)

## Code Structure

- `initI2CKeyboard()` - Initialize I2C and detect keyboard
- `readI2CKeyboard()` - Read key code from CardKeyBoard
- `processI2CKeyboard()` - Process key codes (control keys, arrows, characters)
- `executeCommand()` - Execute terminal commands
- `addOutputLine()` - Add text to output buffer
- `redrawScreen()` - Redraw visible lines on display

## Documentation

- **Key Codes Reference:** [docs/CARDKEYBOARD_KEYCODES.md](../../docs/CARDKEYBOARD_KEYCODES.md)
- **Display Guide:** [docs/EXTERNAL_DISPLAY_ILI9488_GUIDE.md](../../docs/EXTERNAL_DISPLAY_ILI9488_GUIDE.md)

## License

MIT License

---

**Last Updated:** November 28, 2025  
**Tested On:** M5Stack Cardputer-Adv (ESP32-S3)  
**Author:** AndyAiCardputer

