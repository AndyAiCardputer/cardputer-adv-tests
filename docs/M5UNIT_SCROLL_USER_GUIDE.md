# M5Unit-Scroll - User Guide for Cardputer-Adv

**For:** Cardputer-Adv users who want to use M5Unit-Scroll  
**Level:** Beginner/Intermediate  
**Last Updated:** January 13, 2025

---

## What is M5Unit-Scroll?

M5Unit-Scroll is a module from M5Stack with:
- **Rotary encoder** — rotate for navigation (left/right)
- **Button** — press for selection/confirmation
- **RGB LED** — color status indication

Perfect for:
- Menu navigation
- List scrolling
- Value adjustment
- Game controls

---

## Connection

### What You Need

- M5Stack Cardputer-Adv
- M5Unit-Scroll module
- Cable for PORT.A connection (Grove HY2.0-4P)

### Connection Steps

1. **Find PORT.A connector** on Cardputer-Adv (usually on the side or top)
2. **Connect M5Unit-Scroll** to PORT.A:
   - SDA → G2 (GPIO 2)
   - SCL → G1 (GPIO 1)
   - GND → GND (required!)
   - 5V → 5V
3. **Check power** — LED on module should light up
4. **Done!** Module is ready to use

### Important!

- **GND must be connected!** Module won't work without it
- Module must be connected to **PORT.A**, not other connectors
- LED on module should be ON — this means power is present

---

## How It Works?

### Encoder (Rotation)

- **Rotate right** (clockwise) → increases value, scrolls down
- **Rotate left** (counter-clockwise) → decreases value, scrolls up
- Value automatically resets after reading (for smooth navigation)

### Button

- **Press** → select item, confirm action
- **Release** → ready for next action

### RGB LED

- Can change color depending on state
- Example: green = down, red = up, blue = selected

---

## Usage Examples

### Example 1: Simple List Navigation

```cpp
#include <M5Cardputer.h>
#include <Wire.h>

#define UNIT_SCROLL_I2C_ADDRESS 0x40
#define I2C_SDA_PIN 2
#define I2C_SCL_PIN 1

int selectedItem = 0;
int itemCount = 10;

void setup() {
    Serial.begin(115200);
    M5Cardputer.begin(true);
    
    // I2C initialization
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);
    delay(1000);
    
    Serial.println("M5Unit-Scroll ready!");
}

void loop() {
    M5Cardputer.update();
    
    // Read encoder
    Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
    Wire.write(0x50);  // Incremental encoder register
    Wire.endTransmission(false);
    delayMicroseconds(500);
    
    uint8_t bytesReceived = Wire.requestFrom(UNIT_SCROLL_I2C_ADDRESS, 2, true);
    if (bytesReceived == 2) {
        uint8_t low = Wire.read();
        uint8_t high = Wire.read();
        int16_t increment = (int16_t)(low | (high << 8));
        
        if (increment != 0) {
            // Update selected item
            selectedItem += increment;
            
            // Limit range
            if (selectedItem < 0) selectedItem = 0;
            if (selectedItem >= itemCount) selectedItem = itemCount - 1;
            
            Serial.printf("Selected item: %d\n", selectedItem + 1);
        }
    }
    
    // Read button
    Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
    Wire.write(0x20);  // Button register
    Wire.endTransmission(false);
    delayMicroseconds(500);
    
    bytesReceived = Wire.requestFrom(UNIT_SCROLL_I2C_ADDRESS, 1, true);
    if (bytesReceived == 1) {
        uint8_t buttonState = Wire.read();
        if (buttonState == 1) {
            Serial.printf("Button pressed! Selected item %d\n", selectedItem + 1);
            delay(200);  // Debounce protection
        }
    }
    
    delay(50);  // Don't read too frequently
}
```

### Example 2: Value Adjustment with LED Indication

```cpp
int value = 50;  // Current value (0-100)
int minValue = 0;
int maxValue = 100;

void setLED(uint32_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    
    Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
    Wire.write(0x30);  // RGB LED register
    Wire.write(g);     // GRB order for WS2812
    Wire.write(r);
    Wire.write(b);
    Wire.endTransmission();
}

void loop() {
    // ... encoder reading ...
    
    if (increment != 0) {
        value += increment;
        
        // Limit range
        if (value < minValue) value = minValue;
        if (value > maxValue) value = maxValue;
        
        Serial.printf("Value: %d\n", value);
        
        // Change LED color based on value
        if (value < 33) {
            setLED(0xFF0000);  // Red (low value)
        } else if (value < 66) {
            setLED(0xFFFF00);  // Yellow (medium value)
        } else {
            setLED(0x00FF00);  // Green (high value)
        }
        
        delay(100);
        setLED(0x000000);  // Turn off LED
    }
}
```

### Example 3: File List Scrolling

```cpp
String files[] = {"file1.txt", "file2.txt", "file3.txt", "file4.txt", "file5.txt"};
int fileCount = 5;
int selectedFile = 0;
int firstVisible = 0;  // First visible file

void displayFiles() {
    Serial.println("\n=== File List ===");
    for (int i = 0; i < 3; i++) {  // Show 3 files
        int idx = firstVisible + i;
        if (idx >= fileCount) break;
        
        if (idx == selectedFile) {
            Serial.print("> ");  // Highlight selected
        } else {
            Serial.print("  ");
        }
        Serial.println(files[idx]);
    }
    Serial.println("==================\n");
}

void loop() {
    // ... encoder reading ...
    
    if (increment != 0) {
        selectedFile += increment;
        
        // Limit range
        if (selectedFile < 0) selectedFile = 0;
        if (selectedFile >= fileCount) selectedFile = fileCount - 1;
        
        // Update first visible file
        if (selectedFile < firstVisible) {
            firstVisible = selectedFile;
        } else if (selectedFile >= firstVisible + 3) {
            firstVisible = selectedFile - 2;
        }
        
        displayFiles();
    }
    
    // ... button reading ...
    if (buttonState == 1) {
        Serial.printf("Opened file: %s\n", files[selectedFile]);
        delay(200);
    }
}
```

---

## Tips and Tricks

### 1. Don't Read Too Frequently

```cpp
// ✅ Good: read every 50ms
delay(50);

// ❌ Bad: read every loop iteration (may cause errors)
// (without delay)
```

### 2. Use Incremental Encoder

Register `0x50` automatically resets after reading — perfect for navigation:

```cpp
Wire.write(0x50);  // Incremental encoder (recommended)
// instead of
Wire.write(0x10);  // Absolute encoder (need to track changes)
```

### 3. Add Delays for Stability

STM32F030 (microcontroller in module) requires small delays:

```cpp
Wire.endTransmission(false);
delayMicroseconds(500);  // Delay before reading
Wire.requestFrom(...);
delayMicroseconds(300);  // Delay after reading
```

### 4. Handle Errors

If module doesn't respond, don't panic — just skip the reading:

```cpp
uint8_t bytesReceived = Wire.requestFrom(UNIT_SCROLL_I2C_ADDRESS, 2, true);
if (bytesReceived == 2) {
    // Read data
} else {
    // Error - just skip this reading
    // Try again next time
}
```

### 5. Use LED for Feedback

LED helps understand that module is working:

```cpp
// Green = movement down/right
setLED(0x00FF00);

// Red = movement up/left
setLED(0xFF0000);

// Blue = selected/confirmed
setLED(0x0000FF);

// Turn off
setLED(0x000000);
```

---

## Troubleshooting

### Problem: Module Not Found

**What to check:**
1. ✅ Is LED on module ON? (if not — power issue)
2. ✅ Is GND connected? (required!)
3. ✅ Connected to PORT.A? (not other connectors)
4. ✅ Is cable OK?

**What to try:**
- Disconnect and reconnect module
- Restart Cardputer-Adv
- Check code — are you using `Wire`, not `Wire1`?

### Problem: Encoder Not Responding

**What to check:**
1. ✅ Reading correct register? (0x50 for incremental)
2. ✅ Delays added? (500µs before, 300µs after)
3. ✅ Not reading too frequently? (minimum 50ms between reads)

**What to try:**
- Increase delay between reads to 100ms
- Check that `increment != 0` before using
- Add debug output to check values

### Problem: Button Not Working

**What to check:**
1. ✅ Reading register 0x20?
2. ✅ Checking state change? (not just current value)
3. ✅ Debounce protection added? (delay after press)

**What to try:**
```cpp
bool lastButtonState = false;

// In loop():
bool currentButtonState = (buttonData[0] != 0);
if (currentButtonState && !lastButtonState) {
    // Button just pressed
    Serial.println("Button pressed!");
    delay(200);  // Debounce protection
}
lastButtonState = currentButtonState;
```

### Problem: LED Shows Wrong Colors

**Cause:** Wrong color order

**Solution:** Use GRB order (not RGB!):

```cpp
// ✅ Correct: GRB order
Wire.write(g);  // Green
Wire.write(r);  // Red
Wire.write(b);  // Blue

// ❌ Wrong: RGB order
Wire.write(r);  // Red
Wire.write(g);  // Green
Wire.write(b);  // Blue
```

### Problem: I2C Errors After Screen Switch

**Cause:** Module needs time to recover

**Solution:** Add delay after switch:

```cpp
unsigned long screenSwitchTime = 0;
const int SCREEN_SWITCH_DELAY = 500;  // 500ms

// When switching screen:
screenSwitchTime = millis();

// In loop():
if (screenSwitchTime > 0 && (millis() - screenSwitchTime) < SCREEN_SWITCH_DELAY) {
    return;  // Don't read I2C yet
}
```

---

## Frequently Asked Questions

### Q: Can I use multiple modules simultaneously?

**A:** Yes, but you need to configure different I2C addresses. By default, all modules have address 0x40, so you need to change address of at least one module.

### Q: What is the maximum encoder value range?

**A:** Encoder returns 16-bit value (from -32768 to +32767), but for navigation usually small increments are used (±1, ±2, etc.).

### Q: Can I use module without LED?

**A:** Yes, LED is not required for encoder and button to work. Just don't call LED control functions.

### Q: How often can I read encoder?

**A:** Recommended to read no more than once per 50ms (20 times per second). More frequent reading may cause I2C errors.

### Q: Does module work with other platforms?

**A:** Yes, M5Unit-Scroll works with any platforms supporting I2C (Arduino, Raspberry Pi, ESP32, etc.). I2C protocol is standard.

### Q: Can I use module without Cardputer-Adv?

**A:** Yes, module works with any device with I2C. Just connect to corresponding pins (SDA, SCL, GND, 5V).

---

## Useful Links

- **Official M5Stack Documentation:** https://docs.m5stack.com/
- **Code Examples:** `tests/m5unit-scroll/`
- **Technical Documentation for Developers:** [M5UNIT_SCROLL_GUIDE.md](M5UNIT_SCROLL_GUIDE.md)

---

## Ready Examples

There is a ready working example in this repository:

**File:** `tests/m5unit-scroll/unitscroll_test_external_display.ino`

This example includes:
- ✅ Module detection
- ✅ Encoder reading
- ✅ Button reading
- ✅ LED control
- ✅ Scrollable list with optimized rendering
- ✅ Error handling
- ✅ External display support

You can use it as a base for your projects!

---

## Conclusion

M5Unit-Scroll is an excellent module for adding convenient navigation to your Cardputer-Adv projects. Following this guide, you can easily integrate it into your projects.

**Good luck with your projects! 🚀**

---

**Last Updated:** January 13, 2025  
**Version:** 1.0

