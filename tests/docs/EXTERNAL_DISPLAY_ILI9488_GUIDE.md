# External Display ILI9488 Guide for Cardputer-Adv

**Purpose:** Complete guide for working with external ILI9488 480×320 display on M5Stack Cardputer-Adv  
**Target:** Cursor AI and developers  
**Last Updated:** 2025-11-16

## Table of Contents

1. [Hardware Setup](#hardware-setup)
2. [Software Configuration](#software-configuration)
3. [Initialization Order](#initialization-order)
4. [Display Operations](#display-operations)
5. [Optimization Techniques](#optimization-techniques)
6. [Common Issues and Solutions](#common-issues-and-solutions)
7. [Best Practices](#best-practices)
8. [Code Examples](#code-examples)

---

## Hardware Setup

### Display Specifications

- **Model:** ILI9488 TFT LCD
- **Resolution:** 480×320 pixels
- **Interface:** SPI (3-wire, write-only)
- **Color Depth:** 24-bit (RGB888)
- **Power:** 3.3V

### Pin Connections (EXT 2.54-14P Header)

| Display Pin | Cardputer-Adv Pin | GPIO | Function |
|-------------|-------------------|------|----------|
| VCC | 3.3V | - | Power |
| GND | GND | - | Ground |
| CS | PIN 13 | GPIO 5 | Chip Select |
| RST | PIN 1 | GPIO 3 | Reset |
| DC | PIN 5 | GPIO 6 | Data/Command |
| MOSI | PIN 9 | GPIO 14 | SPI Data |
| SCK | PIN 7 | GPIO 40 | SPI Clock |
| MISO | - | - | Not used (3-wire) |

### SPI Configuration

- **SPI Host:** `SPI3_HOST` (HSPI)
- **Mode:** 0
- **Frequency:** 20 MHz (write), 16 MHz (read)
- **3-wire:** Yes (no MISO)
- **DMA:** Channel 0 (optional)

---

## Software Configuration

### Required Libraries

```ini
# platformio.ini
lib_deps = 
    m5stack/M5Cardputer @ ^1.1.1
    m5stack/M5Unified @ ^0.2.11
    m5stack/M5GFX @ ^0.2.17
```

### Display Driver Files

Create two files in your project:

**1. `src/external_display/LGFX_ILI9488.h`**

```cpp
#ifndef LGFX_ILI9488_H
#define LGFX_ILI9488_H

#include <M5GFX.h>
#include "lgfx/v1/panel/Panel_LCD.hpp"

// Local ILI9488 panel definition
struct Panel_ILI9488_Local : public lgfx::v1::Panel_LCD {
    Panel_ILI9488_Local(void) {
        _cfg.memory_width  = _cfg.panel_width  = 320;
        _cfg.memory_height = _cfg.panel_height = 480;
    }

    void setColorDepth_impl(lgfx::v1::color_depth_t depth) override {
        _write_depth = (((int)depth & lgfx::v1::color_depth_t::bit_mask) > 16
                    || (_bus && _bus->busType() == lgfx::v1::bus_spi))
                    ? lgfx::v1::rgb888_3Byte
                    : lgfx::v1::rgb565_2Byte;
        _read_depth = lgfx::v1::rgb888_3Byte;
    }

protected:
    static constexpr uint8_t CMD_FRMCTR1 = 0xB1;
    static constexpr uint8_t CMD_INVCTR  = 0xB4;
    static constexpr uint8_t CMD_DFUNCTR = 0xB6;
    static constexpr uint8_t CMD_ETMOD   = 0xB7;
    static constexpr uint8_t CMD_PWCTR1  = 0xC0;
    static constexpr uint8_t CMD_PWCTR2  = 0xC1;
    static constexpr uint8_t CMD_VMCTR   = 0xC5;
    static constexpr uint8_t CMD_ADJCTL3 = 0xF7;

    const uint8_t* getInitCommands(uint8_t listno) const override {
        static constexpr uint8_t list0[] = {
            CMD_PWCTR1,  2, 0x17, 0x15,
            CMD_PWCTR2,  1, 0x41,
            CMD_VMCTR ,  3, 0x00, 0x12, 0x80,
            CMD_FRMCTR1, 1, 0xA0,
            CMD_INVCTR,  1, 0x02,
            CMD_DFUNCTR, 3, 0x02, 0x22, 0x3B,
            CMD_ETMOD,   1, 0xC6,
            CMD_ADJCTL3, 4, 0xA9, 0x51, 0x2C, 0x82,
            CMD_SLPOUT , 0+CMD_INIT_DELAY, 120,
            CMD_IDMOFF , 0,
            CMD_DISPON , 0+CMD_INIT_DELAY, 100,
            0xFF,0xFF,
        };
        switch (listno) {
        case 0: return list0;
        default: return nullptr;
        }
    }
};

// LGFX Device for ILI9488
class LGFX_ILI9488 : public lgfx::v1::LGFX_Device {
    Panel_ILI9488_Local panel;
    lgfx::v1::Bus_SPI   bus;

public:
    LGFX_ILI9488() {
        // SPI bus configuration
        auto b = bus.config();
        b.spi_host   = SPI3_HOST;     // HSPI
        b.spi_mode   = 0;
        b.freq_write = 20000000;       // 20 MHz
        b.freq_read  = 16000000;       // 16 MHz
        b.spi_3wire  = true;           // 3-wire SPI (no MISO)
        b.use_lock   = false;          // No lock (single device)
        b.dma_channel = 0;             // DMA channel 0
        b.bus_shared = false;          // Not shared with SD card
        
        b.pin_sclk = 40;               // SCK -> GPIO 40
        b.pin_mosi = 14;               // MOSI -> GPIO 14
        b.pin_miso = -1;                // Not used (3-wire)
        b.pin_dc   = 6;                // DC -> GPIO 6
        
        bus.config(b);
        panel.setBus(&bus);

        // Panel configuration
        auto p = panel.config();
        p.pin_cs    = 5;               // CS -> GPIO 5
        p.pin_rst   = 3;               // RST -> GPIO 3
        p.bus_shared = false;          // Not shared
        p.readable   = true;           // Can read (optional)
        p.invert     = false;
        p.rgb_order  = false;
        p.dlen_16bit = false;
        p.memory_width  = 320;
        p.memory_height = 480;
        p.panel_width   = 320;
        p.panel_height  = 480;
        p.offset_x = 0;
        p.offset_y = 0;
        p.offset_rotation = 0;
        p.dummy_read_pixel = 8;
        p.dummy_read_bits = 1;
        panel.config(p);

        setPanel(&panel);
    }
};

#endif // LGFX_ILI9488_H
```

**2. `src/external_display/LGFX_ILI9488.cpp`**

```cpp
#include "LGFX_ILI9488.h"

// Global display object
LGFX_ILI9488 externalDisplay;
```

---

## Initialization Order

### ⚠️ CRITICAL: Display MUST be initialized FIRST

The external display must be initialized **before** `M5Cardputer.begin()` to avoid SPI conflicts.

### Correct Order

```cpp
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // ✅ STEP 1: Initialize external display FIRST
    if (!externalDisplay.init()) {
        Serial.println("ERROR: Display initialization FAILED!");
        while (1) delay(1000);
    }
    
    externalDisplay.setRotation(3);  // 180 degrees (portrait)
    externalDisplay.setColorDepth(24);  // 24-bit color
    
    // ✅ STEP 2: Initialize M5Cardputer AFTER display
    M5Cardputer.begin(true);  // enableKeyboard only
    
    // ✅ STEP 3: Disable built-in display backlight
    M5Cardputer.Display.setBrightness(0);
    
    // Now you can use externalDisplay
    externalDisplay.fillScreen(TFT_BLACK);
    externalDisplay.setTextColor(TFT_WHITE);
    externalDisplay.println("Hello!");
}
```

### Wrong Order (Will Cause Issues)

```cpp
void setup() {
    // ❌ WRONG: M5Cardputer first
    M5Cardputer.begin();
    
    // ❌ This may fail or cause SPI conflicts
    externalDisplay.init();
}
```

---

## Display Operations

### Basic Operations

```cpp
// Clear screen
externalDisplay.fillScreen(TFT_BLACK);

// Set text properties
externalDisplay.setTextSize(2);
externalDisplay.setTextColor(TFT_WHITE, TFT_BLACK);
externalDisplay.setCursor(10, 10);

// Print text
externalDisplay.println("Hello World!");

// Draw shapes
externalDisplay.drawRect(10, 50, 100, 50, TFT_RED);
externalDisplay.fillRect(120, 50, 100, 50, TFT_GREEN);
externalDisplay.drawCircle(200, 200, 50, TFT_BLUE);
```

### SPI Transaction Grouping

**Always use `startWrite()` / `endWrite()` for multiple operations:**

```cpp
// ✅ GOOD: Groups SPI transactions
externalDisplay.startWrite();
externalDisplay.fillRect(10, 10, 100, 50, TFT_RED);
externalDisplay.drawRect(120, 10, 100, 50, TFT_GREEN);
externalDisplay.setTextColor(TFT_WHITE);
externalDisplay.setCursor(10, 70);
externalDisplay.println("Text");
externalDisplay.endWrite();

// ❌ BAD: Each operation is a separate SPI transaction
externalDisplay.fillRect(10, 10, 100, 50, TFT_RED);
externalDisplay.drawRect(120, 10, 100, 50, TFT_GREEN);
externalDisplay.println("Text");
```

### Partial Screen Updates

**Use `fillRect()` instead of `fillScreen()` for updates:**

```cpp
// ❌ BAD: Clears entire screen (153,600 pixels)
externalDisplay.fillScreen(TFT_BLACK);
externalDisplay.setCursor(10, 10);
externalDisplay.println("New value");

// ✅ GOOD: Only clears text area (~900 pixels)
externalDisplay.fillRect(10, 10, 200, 30, TFT_BLACK);
externalDisplay.setCursor(10, 10);
externalDisplay.println("New value");
```

---

## Optimization Techniques

### 1. Partial Updates with State Tracking

```cpp
struct DisplayState {
    bool initialized = false;
    String last_value = "";
    int last_number = -1;
};

static DisplayState state;

void updateDisplay(String new_value) {
    externalDisplay.startWrite();
    
    // Full redraw only on first call
    if (!state.initialized) {
        externalDisplay.fillScreen(TFT_BLACK);
        // ... draw static elements
        state.initialized = true;
    }
    
    // Partial update only if value changed
    if (new_value != state.last_value) {
        externalDisplay.fillRect(10, 50, 200, 30, TFT_BLACK);
        externalDisplay.setCursor(10, 50);
        externalDisplay.println(new_value);
        state.last_value = new_value;
    }
    
    externalDisplay.endWrite();
}
```

### 2. Update Thresholds

```cpp
// Update RSSI only when change ≥ 2 dBm
if (abs(new_rssi - last_rssi) >= 2) {
    updateDisplay(new_rssi);
    last_rssi = new_rssi;
}
```

### 3. Layout Coordinates (Calculate Once)

```cpp
struct Layout {
    int header_y = 10;
    int status_y = 50;
    int data_y = 100;
    // ... define once, reuse
};

static Layout layout;
```

### Performance Comparison

| Method | Pixels Cleared | Update Time | Flicker |
|--------|----------------|-------------|---------|
| `fillScreen()` every update | 153,600 | ~50-100ms | High |
| `fillRect()` partial update | ~900-3,000 | ~5-15ms | Minimal |
| **Improvement** | **98% reduction** | **5-10x faster** | **Smooth** |

---

## Common Issues and Solutions

### Issue 1: Display Shows Only Backlight

**Symptoms:** Screen is lit but shows no content

**Causes:**
- Display initialized after `M5Cardputer.begin()`
- Wrong SPI pins
- Power issues

**Solution:**
```cpp
// ✅ Initialize display FIRST
externalDisplay.init();  // BEFORE M5Cardputer.begin()
M5Cardputer.begin(true);
```

### Issue 2: Screen Flickering

**Symptoms:** Screen flickers on every update

**Causes:**
- Using `fillScreen()` on every update
- Not using `startWrite()` / `endWrite()`
- Too frequent updates

**Solution:**
```cpp
// ✅ Use partial updates
externalDisplay.startWrite();
externalDisplay.fillRect(x, y, w, h, TFT_BLACK);  // Small area
externalDisplay.setCursor(x, y);
externalDisplay.println(value);
externalDisplay.endWrite();
```

### Issue 3: SPI Conflicts with SD Card

**Symptoms:** Display or SD card stops working

**Causes:**
- Both devices on same SPI bus without proper sharing
- CS lines not managed correctly

**Solution:**
```cpp
// If sharing SPI bus:
p.bus_shared = true;  // In panel config

// Always release display before SD operations:
externalDisplay.endWrite();
externalDisplay.waitDisplay();
digitalWrite(LCD_CS, HIGH);

// Now safe to use SD card
```

### Issue 4: Wrong Colors

**Symptoms:** Colors appear incorrect

**Causes:**
- Wrong color depth
- RGB order mismatch

**Solution:**
```cpp
externalDisplay.setColorDepth(24);  // 24-bit for ILI9488
// Check rgb_order in panel config (usually false)
```

### Issue 5: Display Not Responding

**Symptoms:** Display doesn't update

**Causes:**
- SPI frequency too high
- Wrong rotation
- Display not initialized

**Solution:**
```cpp
// Check initialization
if (!externalDisplay.init()) {
    // Handle error
}

// Set rotation (0, 1, 2, or 3)
externalDisplay.setRotation(3);  // 180 degrees

// Reduce SPI frequency if needed
b.freq_write = 15000000;  // 15 MHz instead of 20 MHz
```

---

## Best Practices

### 1. Always Initialize Display First

```cpp
void setup() {
    // Display FIRST
    externalDisplay.init();
    
    // Then M5Cardputer
    M5Cardputer.begin(true);
}
```

### 2. Use SPI Transaction Grouping

```cpp
externalDisplay.startWrite();
// ... multiple operations
externalDisplay.endWrite();
```

### 3. Implement Partial Updates

```cpp
// Track previous values
// Only update changed areas
// Use fillRect() instead of fillScreen()
```

### 4. Set Update Thresholds

```cpp
// Don't update on every tiny change
if (abs(new_value - last_value) >= threshold) {
    updateDisplay(new_value);
}
```

### 5. Define Layout Once

```cpp
// Calculate coordinates once
struct Layout {
    int x, y, w, h;
};
// Reuse throughout code
```

### 6. Error Handling

```cpp
if (!externalDisplay.init()) {
    Serial.println("Display init failed!");
    // Handle error (LED blink, etc.)
    while (1) delay(1000);
}
```

### 7. Rotation and Color Depth

```cpp
externalDisplay.setRotation(3);  // Set once in setup()
externalDisplay.setColorDepth(24);  // Set once in setup()
```

---

## Code Examples

### Example 1: Basic Display

```cpp
#include <M5Cardputer.h>
#include "external_display/LGFX_ILI9488.h"

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Initialize display FIRST
    if (!externalDisplay.init()) {
        Serial.println("Display init failed!");
        while (1) delay(1000);
    }
    
    externalDisplay.setRotation(3);
    externalDisplay.setColorDepth(24);
    
    // Initialize M5Cardputer
    M5Cardputer.begin(true);
    M5Cardputer.Display.setBrightness(0);
    
    // Display content
    externalDisplay.fillScreen(TFT_BLACK);
    externalDisplay.setTextSize(3);
    externalDisplay.setTextColor(TFT_WHITE);
    externalDisplay.setCursor(10, 10);
    externalDisplay.println("Hello!");
}

void loop() {
    M5Cardputer.update();
    delay(100);
}
```

### Example 2: Partial Updates

```cpp
struct DisplayState {
    bool initialized = false;
    int last_counter = -1;
};

static DisplayState state;

void displayCounter(int counter) {
    externalDisplay.startWrite();
    
    if (!state.initialized) {
        externalDisplay.fillScreen(TFT_BLACK);
        externalDisplay.setTextSize(2);
        externalDisplay.setTextColor(TFT_WHITE);
        externalDisplay.setCursor(10, 10);
        externalDisplay.println("Counter:");
        state.initialized = true;
    }
    
    if (counter != state.last_counter) {
        // Only update counter area
        externalDisplay.fillRect(10, 40, 200, 30, TFT_BLACK);
        externalDisplay.setCursor(10, 40);
        externalDisplay.setTextColor(TFT_CYAN);
        externalDisplay.println(counter);
        state.last_counter = counter;
    }
    
    externalDisplay.endWrite();
}
```

### Example 3: Status Display with Multiple Fields

```cpp
struct StatusDisplay {
    bool initialized = false;
    String last_status = "";
    String last_ip = "";
    int last_rssi = 0;
    
    int header_y = 10;
    int status_y = 50;
    int ip_y = 90;
    int rssi_y = 130;
};

static StatusDisplay status;

void displayStatus(String wifi_status, String ip, int rssi) {
    externalDisplay.startWrite();
    
    if (!status.initialized || wifi_status != status.last_status) {
        if (!status.initialized) {
            externalDisplay.fillScreen(TFT_BLACK);
            // Draw static header
            externalDisplay.setTextSize(3);
            externalDisplay.setTextColor(TFT_WHITE);
            externalDisplay.setCursor(10, status.header_y);
            externalDisplay.println("WiFi Status");
            status.initialized = true;
        }
        
        // Update status
        externalDisplay.fillRect(10, status.status_y, 300, 30, TFT_BLACK);
        externalDisplay.setCursor(10, status.status_y);
        externalDisplay.setTextSize(2);
        externalDisplay.setTextColor(TFT_GREEN);
        externalDisplay.println(wifi_status);
        status.last_status = wifi_status;
    }
    
    if (ip != status.last_ip) {
        externalDisplay.fillRect(10, status.ip_y, 300, 30, TFT_BLACK);
        externalDisplay.setCursor(10, status.ip_y);
        externalDisplay.setTextSize(2);
        externalDisplay.setTextColor(TFT_CYAN);
        externalDisplay.print("IP: ");
        externalDisplay.println(ip);
        status.last_ip = ip;
    }
    
    if (abs(rssi - status.last_rssi) >= 2) {
        externalDisplay.fillRect(10, status.rssi_y, 300, 30, TFT_BLACK);
        externalDisplay.setCursor(10, status.rssi_y);
        externalDisplay.setTextSize(2);
        externalDisplay.setTextColor(TFT_YELLOW);
        externalDisplay.printf("RSSI: %d dBm", rssi);
        status.last_rssi = rssi;
    }
    
    externalDisplay.endWrite();
}
```

---

## Quick Reference

### Initialization Checklist

- [ ] Display initialized **BEFORE** `M5Cardputer.begin()`
- [ ] Rotation set (usually 3 for 180°)
- [ ] Color depth set (24-bit for ILI9488)
- [ ] Built-in display backlight disabled
- [ ] Error handling for init failure

### Performance Checklist

- [ ] Use `startWrite()` / `endWrite()` for multiple operations
- [ ] Use `fillRect()` instead of `fillScreen()` for updates
- [ ] Track previous values to avoid unnecessary updates
- [ ] Set update thresholds (e.g., RSSI ±2 dBm)
- [ ] Define layout coordinates once

### Common Colors

```cpp
TFT_BLACK
TFT_WHITE
TFT_RED
TFT_GREEN
TFT_BLUE
TFT_CYAN
TFT_MAGENTA
TFT_YELLOW
TFT_ORANGE
TFT_DARKGRAY
```

---

## References

- **Project Examples:**
  - `cardputer/zx_spectrum_external` - ZX Spectrum emulator with external display
  - `cardputer/cardputer_adv/pc_monitor` - PC monitor with partial updates
  - `cardputer/cardputer_adv/tests-adv/python_terminal_external` - Terminal example

- **Libraries:**
  - [M5GFX Documentation](https://github.com/m5stack/M5GFX)
  - [LovyanGFX Documentation](https://github.com/lovyan03/LovyanGFX)

---

**Last Updated:** 2025-11-16  
**Tested On:** M5Stack Cardputer-Adv (ESP32-S3)  
**Display:** ILI9488 480×320 TFT LCD

