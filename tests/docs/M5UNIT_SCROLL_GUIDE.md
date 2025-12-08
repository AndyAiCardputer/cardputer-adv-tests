# M5Unit-Scroll Guide for Cardputer-Adv

**Purpose:** Complete guide for working with M5Unit-Scroll encoder/scroll module on M5Stack Cardputer-Adv  
**Target:** Cursor AI and developers  
**Last Updated:** 2025-01-13

## Table of Contents

1. [Hardware Overview](#hardware-overview)
2. [I2C Protocol](#i2c-protocol)
3. [Hardware Setup](#hardware-setup)
4. [Software Configuration](#software-configuration)
5. [Initialization](#initialization)
6. [Reading Encoder Values](#reading-encoder-values)
7. [Reading Button State](#reading-button-state)
8. [Controlling RGB LED](#controlling-rgb-led)
9. [Error Handling and Recovery](#error-handling-and-recovery)
10. [Best Practices](#best-practices)
11. [Common Issues and Solutions](#common-issues-and-solutions)
12. [Code Examples](#code-examples)

---

## Hardware Overview

### Module Specifications

- **Model:** M5Unit-Scroll (SKU: U126)
- **MCU:** STM32F030 (internal microcontroller)
- **Interface:** I2C (slave mode)
- **I2C Address:** 0x40 (default, configurable)
- **Features:**
  - Rotary encoder (rotation left/right)
  - Push button
  - RGB LED (WS2812/NeoPixel)
- **Power:** 5V via PORT.A connector

### Physical Markings

- **Back label:** `STM32F030` - indicates internal MCU
- **Back label:** `a-k323cp` - sensor marking (not relevant for I2C communication)
- **Back label:** `RGB - PA0, encoder - PA6/PA7, button PA5` - internal pin mapping (for reference)

---

## I2C Protocol

### ⚠️ CRITICAL: Use Wire (Shared I2C Bus)

**IMPORTANT:** M5Unit-Scroll must use the **same I2C bus (`Wire`) as the Cardputer keyboard**, NOT `Wire1`. Both devices share PORT.A (GPIO 2/1).

### I2C Configuration

- **Bus:** `Wire` (I2C_NUM_0) - shared with keyboard
- **Pins:** GPIO 2 (SDA/G2), GPIO 1 (SCL/G1)
- **Speed:** 50 kHz (recommended for STM32F030 compatibility)
- **Pull-ups:** Internal pull-ups enabled on ESP32
- **Clock stretching:** Supported (STM32F030 requires this)

### Register Map

| Register | Name | Type | Description |
|----------|------|------|-------------|
| 0x10 | Encoder Value | R (16-bit) | Absolute encoder value (little-endian: byte0 + byte1*256) |
| 0x20 | Button Status | R (8-bit) | Button state: 0 = released, 1 = pressed |
| 0x30 | RGB LED | W (3 bytes) | RGB LED control: R, G, B (GRB order for WS2812) |
| 0x31-0x33 | RGB LED Registers | W (1 byte each) | Individual R, G, B registers (for firmware compatibility) |
| 0x40 | Reset Encoder | W (8-bit) | Write 1 to reset encoder value to 0 |
| 0x50 | Incremental Encoder | R (16-bit) | Incremental encoder value (resets after read!) |
| 0xF0 | Info Register | R (16 bytes) | Bootloader/FW version, I2C address |

### Data Format

- **16-bit values:** Little-endian (LSB first, MSB second)
  - Example: `[0x34, 0x12]` = `0x1234` = 4660 decimal
- **Signed values:** Use `int16_t` cast for encoder increments
- **RGB LED:** GRB color order (not RGB!) for WS2812 protocol

---

## Hardware Setup

### Connection to Cardputer-Adv

**PORT.A (Grove HY2.0-4P connector):**

| M5Unit-Scroll Pin | Cardputer-Adv PORT.A | GPIO | Function |
|-------------------|----------------------|------|----------|
| SDA | G2 | GPIO 2 | I2C Data |
| SCL | G1 | GPIO 1 | I2C Clock |
| GND | GND | - | Ground |
| 5V | 5V | - | Power |

### Power Requirements

- **Voltage:** 5V (from PORT.A)
- **Current:** ~50-100 mA (LED adds ~20 mA per color)
- **LED indicator:** Module LED should be ON when powered

### Physical Connection

1. Connect M5Unit-Scroll to PORT.A connector
2. Ensure GND is connected (critical!)
3. Check module LED is ON
4. Module emits ultrasonic sounds when active (normal)

---

## Software Configuration

### Required Libraries

```cpp
#include <M5Cardputer.h>
#include <Wire.h>
```

### Constants and Definitions

```cpp
// I2C Configuration
#define UNIT_SCROLL_I2C_ADDRESS 0x40
#define I2C_SDA_PIN 2  // GPIO 2 (G2)
#define I2C_SCL_PIN 1  // GPIO 1 (G1)

// Register Definitions
#define SCROLL_ENCODER_REG     0x10  // Absolute encoder value
#define SCROLL_BUTTON_REG      0x20  // Button status
#define SCROLL_RGB_REG         0x30  // RGB LED control
#define SCROLL_RESET_REG       0x40  // Reset encoder
#define SCROLL_INC_ENCODER_REG 0x50  // Incremental encoder (resets after read!)
#define SCROLL_INFO_REG        0xF0  // Info register
```

### State Variables

```cpp
bool moduleFound = false;
uint8_t foundAddress = 0;
int lastEncoderValue = 0;
bool lastButtonState = false;
int i2cErrorCount = 0;
const int MAX_I2C_ERRORS = 10;
```

---

## Initialization

### ⚠️ CRITICAL: Initialization Order

1. **Initialize external display FIRST** (if using)
2. **Initialize M5Cardputer** (`M5Cardputer.begin(true)`)
3. **Initialize I2C on PORT.A** (using `Wire`, not `Wire1`)

### Correct Initialization Code

```cpp
void setup() {
    Serial.begin(115200);
    delay(500);
    
    // Step 1: Initialize external display (if using)
    // ... display init code ...
    
    // Step 2: Initialize M5Cardputer
    M5Cardputer.begin(true);  // enableKeyboard
    delay(200);
    M5.Display.setBrightness(0);  // Disable built-in display
    
    // Step 3: Initialize I2C on PORT.A
    Serial.println("Initializing I2C...");
    Serial.println("CRITICAL: Using Wire (shared with keyboard)");
    
    // Enable internal pull-ups (STM32F030 requires this)
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    delay(100);
    
    // Initialize Wire (NOT Wire1!) on PORT.A
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);  // 50 kHz for STM32F030
    Wire.setTimeOut(100);  // 100ms timeout
    
    Serial.printf("  SDA: GPIO %d (G2)\n", I2C_SDA_PIN);
    Serial.printf("  SCL: GPIO %d (G1)\n", I2C_SCL_PIN);
    Serial.printf("  Address: 0x%02X\n", UNIT_SCROLL_I2C_ADDRESS);
    Serial.printf("  Speed: 50 kHz\n");
    
    // Wait for STM32F030 initialization
    delay(1000);
    
    // Scan for module
    // ... detection code ...
}
```

### Module Detection

STM32F030 may require multiple detection methods:

```cpp
bool found = false;
uint8_t foundAddress = 0;

// Method 1: Standard I2C scan
for (uint8_t addr = 0x40; addr <= 0x42; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();
    if (error == 0) {
        found = true;
        foundAddress = addr;
        break;
    }
    delay(50);
}

// Method 2: Direct register read (if Method 1 fails)
if (!found) {
    Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
    Wire.write(SCROLL_BUTTON_REG);
    byte error = Wire.endTransmission(false);  // repeated start
    if (error == 0) {
        delayMicroseconds(1000);
        uint8_t bytesReceived = Wire.requestFrom(UNIT_SCROLL_I2C_ADDRESS, 1, true);
        if (bytesReceived > 0 && Wire.available()) {
            uint8_t buttonState = Wire.read();
            found = true;
            foundAddress = UNIT_SCROLL_I2C_ADDRESS;
        }
    }
}

// Method 3: Try different I2C speeds (if Methods 1-2 fail)
if (!found) {
    uint32_t speeds[] = {50000, 100000, 200000};
    for (int s = 0; s < 3; s++) {
        Wire.end();
        delay(100);
        Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, speeds[s]);
        Wire.setTimeOut(100);
        delay(200);
        
        Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
        byte error = Wire.endTransmission();
        if (error == 0) {
            found = true;
            foundAddress = UNIT_SCROLL_I2C_ADDRESS;
            break;
        }
    }
}

if (found) {
    moduleFound = true;
    Serial.printf("✓ Module found at 0x%02X\n", foundAddress);
} else {
    Serial.println("✗ Module not found!");
}
```

---

## Reading Encoder Values

### Incremental Encoder (Recommended)

**Use register 0x50** - shows change since last read, automatically resets:

```cpp
bool readScrollRegister(uint8_t reg, uint8_t length, uint8_t* data) {
    if (!moduleFound) return false;
    
    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
    
    Wire.beginTransmission(address);
    Wire.write(reg);
    byte error = Wire.endTransmission(false);  // repeated start
    
    if (error != 0) {
        i2cErrorCount++;
        return false;
    }
    
    delayMicroseconds(500);  // STM32F030 needs delay
    
    uint8_t bytesReceived = Wire.requestFrom(address, length, true);
    if (bytesReceived == 0) {
        i2cErrorCount++;
        return false;
    }
    
    delayMicroseconds(300);
    
    for (int i = 0; i < length && i < 16; i++) {
        if (Wire.available()) {
            data[i] = Wire.read();
        } else {
            i2cErrorCount++;
            return false;
        }
    }
    
    i2cErrorCount = 0;  // Success - reset error counter
    return true;
}

// Read incremental encoder
uint8_t incData[2] = {0};
if (readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData)) {
    int16_t incValue = (int16_t)(incData[0] | (incData[1] << 8));  // little-endian
    
    if (incValue != 0) {
        lastEncoderValue += incValue;
        Serial.printf("Encoder increment: %+d (Total: %d)\n", incValue, lastEncoderValue);
        
        // Handle rotation
        if (incValue > 0) {
            // Rotated right (clockwise)
        } else {
            // Rotated left (counter-clockwise)
        }
    }
}
```

### Absolute Encoder

**Use register 0x10** - shows absolute position:

```cpp
uint8_t encData[2] = {0};
if (readScrollRegister(SCROLL_ENCODER_REG, 2, encData)) {
    int16_t encoderValue = (int16_t)(encData[0] | (encData[1] << 8));
    // Use encoderValue directly
}
```

### Reading Interval

**Don't read too frequently!** Use interval:

```cpp
const int SCROLL_READ_INTERVAL = 50;  // ms
unsigned long lastScrollReadTime = 0;

void loop() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastScrollReadTime < SCROLL_READ_INTERVAL) {
        return;  // Too soon, skip
    }
    lastScrollReadTime = currentTime;
    
    // Read encoder...
}
```

---

## Reading Button State

### Basic Button Read

```cpp
uint8_t buttonData[1] = {0};
if (readScrollRegister(SCROLL_BUTTON_REG, 1, buttonData)) {
    bool buttonState = (buttonData[0] != 0);
    
    if (buttonState != lastButtonState) {
        if (buttonState) {
            Serial.println("Button PRESSED");
            // Handle button press
        } else {
            Serial.println("Button RELEASED");
            // Handle button release
        }
        lastButtonState = buttonState;
    }
}
```

### Button Debounce

Read button less frequently than encoder:

```cpp
static unsigned long lastButtonReadTime = 0;
if (currentTime - lastButtonReadTime > 100) {  // Read button every 100ms
    // Read button...
    lastButtonReadTime = currentTime;
}
```

---

## Controlling RGB LED

### LED Control via Register 0x30

**Color format:** GRB (not RGB!) for WS2812 protocol

```cpp
void setScrollLED(uint32_t color) {
    if (!moduleFound) return;
    
    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
    
    // Extract GRB from 0xRRGGBB format
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    // Write GRB order (WS2812 protocol)
    Wire.beginTransmission(address);
    Wire.write(SCROLL_RGB_REG);  // 0x30
    Wire.write(g);  // Green first
    Wire.write(r);  // Red second
    Wire.write(b);  // Blue third
    byte error = Wire.endTransmission();
    
    if (error != 0) {
        i2cErrorCount++;
    }
}

// Usage
setScrollLED(0xFF0000);  // Red
setScrollLED(0x00FF00);  // Green
setScrollLED(0x0000FF);  // Blue
setScrollLED(0x000000);  // Off
```

### LED Control via Registers 0x31-0x33

**Alternative method** (for firmware compatibility):

```cpp
void setScrollLEDFast(uint32_t color) {
    if (!moduleFound) return;
    
    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
    
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    Wire.beginTransmission(address);
    Wire.write(0x31);  // Start register for R
    Wire.write(r);      // R
    Wire.write(g);      // G
    Wire.write(b);      // B
    byte error = Wire.endTransmission();
    
    if (error != 0) {
        i2cErrorCount++;
    }
}
```

---

## Error Handling and Recovery

### I2C Error Counter

Track consecutive errors:

```cpp
int i2cErrorCount = 0;
const int MAX_I2C_ERRORS = 10;

// In readScrollRegister():
if (error != 0) {
    i2cErrorCount++;
    return false;
}

// On success:
i2cErrorCount = 0;
```

### Soft I2C Bus Reset

**When to use:** After `MAX_I2C_ERRORS` consecutive failures

```cpp
void i2cBusReset() {
    Serial.println(">>> I2C Bus Reset: Recovering from errors...");
    
    // Stop I2C
    Wire.end();
    delay(50);
    
    // Clock recovery: 9 clock pulses to release stuck slave
    pinMode(I2C_SCL_PIN, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
    }
    
    // Restore I2C pins
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    delay(50);
    
    // Reinitialize Wire
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);
    Wire.setTimeOut(100);
    
    Serial.println(">>> I2C Bus Reset: Complete");
}

// Usage in loop()
if (i2cErrorCount >= MAX_I2C_ERRORS) {
    i2cBusReset();
    i2cErrorCount = 0;
    delay(200);
    return;
}
```

### Screen Switch Delay

**When switching screens**, wait before reading I2C:

```cpp
unsigned long screenSwitchTime = 0;
const int SCREEN_SWITCH_DELAY = 500;  // ms

// When switching to scroll screen:
screenSwitchTime = millis();

// In loop():
if (screenSwitchTime > 0 && (millis() - screenSwitchTime) < SCREEN_SWITCH_DELAY) {
    delay(10);
    return;  // Don't read I2C yet
}
```

---

## Best Practices

### 1. Always Use Wire (Not Wire1)

```cpp
// ✅ CORRECT
Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);

// ❌ WRONG
Wire1.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);
```

### 2. Use Incremental Encoder for Navigation

```cpp
// ✅ CORRECT: Use 0x50 (incremental, resets after read)
readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData);

// ⚠️ OK but less efficient: Use 0x10 (absolute, need to track changes)
readScrollRegister(SCROLL_ENCODER_REG, 2, encData);
```

### 3. Implement Reading Intervals

```cpp
// ✅ CORRECT: Don't read too frequently
if (currentTime - lastScrollReadTime < SCROLL_READ_INTERVAL) {
    return;
}

// ❌ WRONG: Reading every loop iteration
// (causes I2C bus congestion)
```

### 4. Handle Errors Gracefully

```cpp
// ✅ CORRECT: Track errors and recover
if (readScrollRegister(...)) {
    i2cErrorCount = 0;  // Success
} else {
    i2cErrorCount++;
    if (i2cErrorCount >= MAX_I2C_ERRORS) {
        i2cBusReset();
    }
}

// ❌ WRONG: Ignore errors
readScrollRegister(...);  // No error handling
```

### 5. Use Repeated Start for Register Reads

```cpp
// ✅ CORRECT: Use repeated start (false parameter)
Wire.endTransmission(false);  // Don't stop bus
delayMicroseconds(500);
Wire.requestFrom(address, length, true);

// ❌ WRONG: Stop bus between write and read
Wire.endTransmission();  // Stops bus
Wire.requestFrom(...);   // New transaction (may fail)
```

### 6. Add Delays for STM32F030

```cpp
// ✅ CORRECT: Add delays for STM32F030
Wire.endTransmission(false);
delayMicroseconds(500);  // Before requestFrom
Wire.requestFrom(...);
delayMicroseconds(300);  // After requestFrom

// ❌ WRONG: No delays (may cause timeouts)
Wire.endTransmission(false);
Wire.requestFrom(...);  // May fail
```

---

## Common Issues and Solutions

### Issue 1: Module Not Found

**Symptoms:** `moduleFound = false`, no response to I2C commands

**Causes:**
- Wrong I2C pins (should be GPIO 2/1)
- Module not powered (check LED)
- GND not connected
- Using `Wire1` instead of `Wire`

**Solution:**
```cpp
// 1. Verify using Wire (not Wire1)
Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);

// 2. Check power (LED should be ON)
// 3. Try multiple detection methods
// 4. Try different I2C speeds (50kHz, 100kHz, 200kHz)
// 5. Power cycle module (unplug/replug)
```

### Issue 2: I2C NACK Errors

**Symptoms:** `I2C transaction unexpected nack detected` errors

**Causes:**
- Reading too frequently
- STM32F030 not ready
- I2C bus conflict with keyboard
- Module in stuck state

**Solution:**
```cpp
// 1. Increase reading interval
const int SCROLL_READ_INTERVAL = 50;  // ms

// 2. Add delays for STM32F030
delayMicroseconds(500);  // Before requestFrom
delayMicroseconds(300);  // After requestFrom

// 3. Implement error recovery
if (i2cErrorCount >= MAX_I2C_ERRORS) {
    i2cBusReset();
}

// 4. Add delay after screen switch
if (screenSwitchTime > 0 && (millis() - screenSwitchTime) < SCREEN_SWITCH_DELAY) {
    return;  // Don't read yet
}
```

### Issue 3: Wrong Colors on LED

**Symptoms:** LED shows wrong colors (e.g., red instead of green)

**Causes:**
- Wrong color order (RGB instead of GRB)
- Wrong register usage

**Solution:**
```cpp
// ✅ CORRECT: Use GRB order for WS2812
Wire.write(g);  // Green first
Wire.write(r);  // Red second
Wire.write(b);  // Blue third

// ❌ WRONG: RGB order
Wire.write(r);  // Wrong!
Wire.write(g);
Wire.write(b);
```

### Issue 4: Encoder Values Don't Reset

**Symptoms:** Encoder value doesn't reset to 0

**Causes:**
- Using wrong register (0x10 instead of 0x50)
- Not writing reset command correctly

**Solution:**
```cpp
// ✅ CORRECT: Reset encoder via register 0x40
Wire.beginTransmission(address);
Wire.write(SCROLL_RESET_REG);  // 0x40
Wire.write(1);  // Write 1 to reset
Wire.endTransmission();

// ✅ CORRECT: Use incremental encoder (0x50) - auto-resets
readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData);
```

### Issue 5: Communication Breaks After Screen Switch

**Symptoms:** I2C works initially, breaks after switching screens

**Causes:**
- No delay after screen switch
- State not reset properly
- I2C bus conflict

**Solution:**
```cpp
// 1. Add delay after screen switch
screenSwitchTime = millis();
// ... wait SCREEN_SWITCH_DELAY before reading I2C

// 2. Reset state variables
i2cErrorCount = 0;
lastScrollReadTime = 0;
lastButtonState = false;

// 3. Use full screen clear before switch
lcd.fillScreen(TFT_BLACK);  // Clear artifacts
```

---

## Code Examples

### Example 1: Basic Encoder Reading

```cpp
#include <M5Cardputer.h>
#include <Wire.h>

#define UNIT_SCROLL_I2C_ADDRESS 0x40
#define I2C_SDA_PIN 2
#define I2C_SCL_PIN 1
#define SCROLL_INC_ENCODER_REG 0x50

bool moduleFound = false;
int lastEncoderValue = 0;

bool readScrollRegister(uint8_t reg, uint8_t length, uint8_t* data) {
    if (!moduleFound) return false;
    
    Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
    Wire.write(reg);
    byte error = Wire.endTransmission(false);
    if (error != 0) return false;
    
    delayMicroseconds(500);
    uint8_t bytesReceived = Wire.requestFrom(UNIT_SCROLL_I2C_ADDRESS, length, true);
    if (bytesReceived == 0) return false;
    
    delayMicroseconds(300);
    for (int i = 0; i < length; i++) {
        if (Wire.available()) {
            data[i] = Wire.read();
        } else {
            return false;
        }
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    M5Cardputer.begin(true);
    
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);
    Wire.setTimeOut(100);
    delay(1000);
    
    // Detect module
    Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
    if (Wire.endTransmission() == 0) {
        moduleFound = true;
        Serial.println("✓ Module found!");
    }
}

void loop() {
    M5Cardputer.update();
    
    if (!moduleFound) return;
    
    uint8_t incData[2] = {0};
    if (readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData)) {
        int16_t incValue = (int16_t)(incData[0] | (incData[1] << 8));
        if (incValue != 0) {
            lastEncoderValue += incValue;
            Serial.printf("Encoder: %d\n", lastEncoderValue);
        }
    }
    
    delay(50);
}
```

### Example 2: Encoder Navigation with LED Feedback

```cpp
void setScrollLED(uint32_t color) {
    if (!moduleFound) return;
    
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
    Wire.write(0x30);
    Wire.write(g);  // GRB order
    Wire.write(r);
    Wire.write(b);
    Wire.endTransmission();
}

void loop() {
    // ... encoder reading code ...
    
    if (incValue != 0) {
        if (incValue > 0) {
            // Rotated right - green LED
            setScrollLED(0x00FF00);
        } else {
            // Rotated left - red LED
            setScrollLED(0xFF0000);
        }
        delay(100);
        setScrollLED(0x000000);  // Turn off
    }
}
```

### Example 3: Button with Encoder Reset

```cpp
bool lastButtonState = false;

void loop() {
    // ... encoder reading ...
    
    // Read button
    uint8_t buttonData[1] = {0};
    if (readScrollRegister(0x20, 1, buttonData)) {
        bool buttonState = (buttonData[0] != 0);
        
        if (buttonState && !lastButtonState) {
            // Button pressed - reset encoder
            Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
            Wire.write(0x40);  // Reset register
            Wire.write(1);
            Wire.endTransmission();
            
            lastEncoderValue = 0;
            Serial.println("Encoder reset!");
        }
        
        lastButtonState = buttonState;
    }
}
```

### Example 4: Scrollable List with Optimized Display

```cpp
// State variables
int selectedItem = 0;
int itemCount = 20;
bool screenInitialized = false;

void drawScrollableList() {
    lcd.startWrite();  // Group SPI operations
    
    // Draw static elements only once
    if (!screenInitialized) {
        lcd.fillScreen(TFT_BLACK);
        lcd.setTextSize(2);
        lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        lcd.setCursor(20, 10);
        lcd.print("Scroll Test");
        screenInitialized = true;
    }
    
    // Always redraw frame
    lcd.drawRect(20, 40, 440, 190, TFT_WHITE);
    
    // Clear only list area (partial update)
    lcd.fillRect(22, 42, 436, 186, TFT_BLACK);
    
    // Draw visible items
    int firstVisible = selectedItem - 3;
    if (firstVisible < 0) firstVisible = 0;
    
    int y = 50;
    for (int i = 0; i < 8; i++) {
        int itemIdx = firstVisible + i;
        if (itemIdx >= itemCount) break;
        
        bool isSelected = (itemIdx == selectedItem);
        
        if (isSelected) {
            lcd.setTextSize(4);
            lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        } else {
            lcd.setTextSize(2);
            lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        
        lcd.setCursor(30, y);
        lcd.printf("Item %d", itemIdx + 1);
        
        y += isSelected ? 32 : 22;
    }
    
    lcd.endWrite();
}

void loop() {
    // Read incremental encoder
    uint8_t incData[2] = {0};
    if (readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData)) {
        int16_t incValue = (int16_t)(incData[0] | (incData[1] << 8));
        
        if (incValue != 0) {
            if (incValue > 0 && selectedItem < itemCount - 1) {
                selectedItem++;
            } else if (incValue < 0 && selectedItem > 0) {
                selectedItem--;
            }
            drawScrollableList();  // Partial update, no flicker!
        }
    }
    
    delay(50);
}
```

---

## Quick Reference

### Initialization Checklist

- [ ] Use `Wire` (not `Wire1`) for I2C
- [ ] Enable internal pull-ups on ESP32
- [ ] Set I2C speed to 50 kHz (STM32F030 compatible)
- [ ] Add 1-second delay after `Wire.begin()` for STM32F030
- [ ] Try multiple detection methods if module not found
- [ ] Verify module LED is ON (power check)

### Reading Checklist

- [ ] Use incremental encoder (0x50) for navigation
- [ ] Implement reading interval (50ms minimum)
- [ ] Add delays for STM32F030 (500µs before, 300µs after)
- [ ] Use repeated start (`endTransmission(false)`)
- [ ] Track error count and implement recovery
- [ ] Add delay after screen switch (500ms)

### LED Control Checklist

- [ ] Use GRB color order (not RGB!)
- [ ] Extract R, G, B from 0xRRGGBB format
- [ ] Write to register 0x30 (or 0x31-0x33)
- [ ] Handle write errors

### Error Recovery Checklist

- [ ] Track `i2cErrorCount`
- [ ] Implement `i2cBusReset()` after MAX errors
- [ ] Reset state variables on screen switch
- [ ] Add delays for STM32F030 compatibility

---

## References

- **Working Example:**
  - `cardputer/cardputer_adv/tests-adv/unitscroll_test_external_display/unitscroll_test_external_display.ino`

- **Related Documentation:**
  - `_docs/EXTERNAL_DISPLAY_ILI9488_GUIDE.md` - External display guide
  - `_docs/ESP_IDF_DOCUMENTATION.md` - ESP32 I2C details

- **Hardware:**
  - M5Stack M5Unit-Scroll (SKU: U126)
  - STM32F030 MCU (internal)
  - WS2812 RGB LED

---

**Last Updated:** 2025-01-13  
**Tested On:** M5Stack Cardputer-Adv (ESP32-S3)  
**Module:** M5Unit-Scroll (STM32F030, I2C address 0x40)

