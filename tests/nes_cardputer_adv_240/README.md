# NES Emulator - External Display ILI9341 (2.4")

**Version:** 1.0  
**For:** M5Stack Cardputer-Adv  
**Display:** ILI9341 (240×320, 2.4") - External display  
**Status:** ✅ **Working perfectly!**

---

## Description

NES emulator for M5Stack Cardputer-Adv with **external ILI9341 display** (240×320, 2.4 inches).

**Features:**
- ✅ Single-core operation (no RTOS)
- ✅ **External ILI9341 display** (240×320 pixels, 2.4 inches)
- ✅ Shared SPI bus for SD card and display
- ✅ ROM loading from SD card (via VFS mount point `/sd`)
- ✅ Frame rendering (256×240 → 240×240, centered)
- ✅ Keyboard input (WASD for directions, Enter/Space for A/B)
- ✅ **Joystick2 support** (auto-detection, works in parallel with keyboard)
- ✅ **Audio working!** 🔊 (22050 Hz, mono, double buffering)
- ✅ **Volume control** (keys `-` and `=`, step 10, range 0-255)
- ✅ Image displayed correctly (centered, no artifacts)

---

## Hardware Requirements

- **M5Stack Cardputer-Adv** (ESP32-S3)
- **ILI9341 external display** (240×320, 2.4 inches)
- **SD card** (FAT32 formatted)
- **M5Unit Joystick2** (optional, for gamepad control)
- **USB-C cable** for programming

---

## Connections

### External ILI9341 Display (EXT 2.54-14P Header)

| Display Pin | Cardputer-Adv Pin | GPIO | Function |
|-------------|-------------------|------|----------|
| VCC | PIN 2 | - | 5VIN (for backlight) |
| GND | PIN 4 | - | Ground |
| SCK | PIN 7 | GPIO 40 | SPI Clock (shared with SD) |
| MOSI | PIN 9 | GPIO 14 | SPI Data (shared with SD) |
| CS | PIN 13 | GPIO 5 | Chip Select |
| DC | PIN 5 | GPIO 6 | Data/Command |
| RST | PIN 1 | GPIO 3 | Reset |

### SD Card
- Insert into SD card slot on the side of Cardputer-Adv
- CS: GPIO 12

### M5Unit Joystick2 (Optional - PORT.A Grove Port)
- **G1** = SDA (GPIO 2)
- **G2** = SCL (GPIO 1)
- Auto-detected at startup

---

## SPI Configuration

Both devices use a single SPI bus (SPI3_HOST/HSPI):

- **SCK:** GPIO 40 (shared)
- **MOSI:** GPIO 14 (shared)
- **MISO:** GPIO 39 (SD card only)
- **Display CS:** GPIO 5
- **SD Card CS:** GPIO 12

### Important Settings:

```cpp
// For ILI9341 display:
b.spi_host   = SPI3_HOST;     // Same host as SD
b.spi_3wire  = true;          // 3-wire SPI (no MISO)
b.use_lock   = true;          // Transactions enabled
b.freq_write = 20000000;      // 20 MHz (ILI9341 more stable at lower frequency)
p.bus_shared = true;          // IMPORTANT for shared SPI
p.readable   = false;         // Display not readable via MISO
```

---

## Building and Flashing

### Prerequisites
- PlatformIO installed
- M5Stack Cardputer-Adv connected via USB-C

### Build Steps

1. **Navigate to project directory:**
   ```bash
   cd tests/nes_cardputer_adv_240
   ```

2. **Build the project:**
   ```bash
   pio run -e m5stack-cardputer-adv-ext
   ```

3. **Upload to device:**
   ```bash
   pio run -e m5stack-cardputer-adv-ext --target upload
   ```

4. **Monitor serial output:**
   ```bash
   pio device monitor -b 115200
   ```

### Building Firmware Binary

To create a firmware binary file for distribution:

```bash
pio run -e m5stack-cardputer-adv-ext
```

The firmware binary will be located at:
```
.pio/build/m5stack-cardputer-adv-ext/firmware.bin
```

---

## Controls

### Keyboard:
- **W/S/A/D** - Directions (Up/Down/Left/Right)
- **Enter** - START button
- **'** (single quote) - SELECT button
- **Space** - Button A
- **/** - Button B
- **`-`** - Decrease volume
- **`=`** - Increase volume

### Joystick2 (Optional):
- **Joystick left/right** - D-pad ←→
- **Joystick up/down** - D-pad ↑↓
- **Center button** - A (jump)

> 💡 **Note:** Joystick2 works in parallel with keyboard. If joystick is not connected, only keyboard is used.

---

## Audio

- **Sample Rate:** 22050 Hz
- **Format:** 16-bit mono
- **Chunk Size:** 368 samples per frame (60 FPS)
- **Double Buffering:** for smooth playback
- **Initial Volume:** 80/255
- **Control:** `-` decreases, `=` increases (step 10)

---

## ROM Setup

1. Format SD card as **FAT32**
2. Create folder: `/roms/`
3. Place your NES ROM file as: `/roms/game.nes`
4. Insert SD card into Cardputer-Adv

**Note:** Currently, ROM path is hardcoded to `/sd/roms/game.nes`. Menu for ROM selection is planned for future versions.

---

## Initialization Order

The correct initialization sequence is critical for shared SPI:

1. Set CS pins HIGH (both devices inactive)
2. Initialize SPI bus (`sdSPI.begin()`)
3. Initialize external display (`externalDisplay.init()`)
4. Initialize M5Cardputer (`M5Cardputer.begin()`)
5. Release LCD (`lcd_quiesce()`) before SD operations
6. Initialize SD card
7. Initialize OSD (audio, input)

---

## Known Limitations

- Single-core operation (no RTOS) - may be slower on complex games
- ROM path is hardcoded (`/sd/roms/game.nes`)
- No ROM selection menu
- Scaling 256×240 → 240×240 (centered, square)

---

## Comparison with Other Versions

| Feature | External (480×320) | 320×240 (ILI9341, 2.8") | 240×320 (ILI9341, 2.4") |
|---------|-------------------|------------------------|------------------------|
| Display | ILI9488 | ILI9341 | ILI9341 |
| Resolution | 480×320 | 320×240 | 240×320 |
| Size | 2.8" | 2.8" | 2.4" |
| Rendering | 384×240 (centered) | 320×240 (full screen) | 240×240 (centered) |
| Scaling | 256×240→384×240 (1.5x X) | 256×240→320×240 (1.25x X) | 256×240→240×240 (1x) |
| Black borders | Yes (48px left/right) | No | Yes (top/bottom) |
| Performance | Slower (more pixels) | Medium | Faster (fewer pixels) |

---

## Project Structure

```
nes_cardputer_adv_240/
├── src/
│   ├── main.cpp              # Initialization, ROM loading
│   ├── nes_osd.cpp          # OSD functions (display, input, sound)
│   ├── pins.h               # Pin definitions
│   └── external_display/
│       ├── LGFX_ILI9341.h   # External display ILI9341 configuration
│       └── LGFX_ILI9341.cpp # Implementation
├── lib/
│   └── arduino-nofrendo/    # NES emulator library
├── platformio.ini          # Project configuration
└── README.md                # This file
```

---

## Future Improvements

- **Phase 2:** Add RTOS (dual-core)
- **Phase 3:** Rendering optimization (LUT, DMA)
- **Phase 4:** ROM selection menu

---

## Credits

- **NES Emulator:** Nofrendo (https://github.com/implicit/nofrendo)
- **Display Library:** M5GFX / LovyanGFX
- **Hardware:** M5Stack Cardputer-Adv
- **Author:** AndyAiCardputer

---

## License

This project uses the Nofrendo NES emulator, which is licensed under the LGPL. See `lib/arduino-nofrendo/COPYING.LGPL` for details.

---

**Last Updated:** January 31, 2026
