# M5Stack Unit PaHub v2.1 Documentation

Complete technical guide for working with M5Stack Unit PaHub v2.1 (PCA9548A I2C multiplexer).

## Hardware Overview

### Unit PaHub v2.1 Specifications

- **SKU:** U040-B-V21
- **Chip:** PCA9548AP (I2C multiplexer/switch)
- **Channels:** 6 I2C channels (channels 0-5)
- **Default address:** 0x70 (configurable via DIP switch)
- **I2C Speed:** Up to 400 kHz
- **Purpose:** Expand single I2C interface to 6 channels, allowing multiple devices with same/different addresses

### PCA9548AP Chip Details

**Datasheet:** [PCA9548A Datasheet](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/unit/pahub2/pca9548a.pdf)

- **Type:** 8-channel bidirectional I2C switch/multiplexer
- **Channels:** 8 channels (PaHub v2.1 uses 6: channels 0-5)
- **Base Address:** 0x70 (7-bit I2C address)
- **Address Range:** 0x70-0x77 (configurable via DIP switch)
- **Voltage Levels:** Supports 1.8V, 2.5V, 3.3V, and 5V bus voltage translation
- **Reset:** Low-active RESET input for recovery from bus lock-ups
- **Power-on:** All channels deselected on power-up

### Key Features

1. **Channel Expansion:** One I2C master → 6 I2C slave channels
2. **Address Conflict Resolution:** Connect multiple devices with same I2C address on different channels
3. **Cascading:** Multiple PaHub units can be daisy-chained (each with different address)
4. **Voltage Translation:** Supports different bus voltages per channel
5. **Hot Insertion:** Supports hot-plugging devices

## I2C Protocol

### Address Configuration

**Base Address:** 0x70 (7-bit) = 0xE0 (8-bit write) / 0xE1 (8-bit read)

**Address Selection (DIP Switch):**

| DIP Switch | A2 | A1 | A0 | I²C Address (7-bit) | I²C Address (8-bit) |
|------------|----|----|----|---------------------|---------------------|
| 000 (OFF)  | L  | L  | L  | 0x70 (default)      | 0xE0/0xE1           |
| 001        | L  | L  | H  | 0x71                | 0xE2/0xE3           |
| 010        | L  | H  | L  | 0x72                | 0xE4/0xE5           |
| 011        | L  | H  | H  | 0x73                | 0xE6/0xE7           |
| 100        | H  | L  | L  | 0x74                | 0xE8/0xE9           |
| 101        | H  | L  | H  | 0x75                | 0xEA/0xEB           |
| 110        | H  | H  | L  | 0x76                | 0xEC/0xED           |
| 111 (ON)   | H  | H  | H  | 0x77                | 0xEE/0xEF           |

**Note:** PaHub v2.1 uses **DIP switch** instead of A0-A2 pins (v2.0 used A0-A2 pins)

### Control Register

**Register:** Single 8-bit control register (read/write)

**Format:** Each bit represents a channel (bit 0 = channel 0, bit 1 = channel 1, etc.)

| Bit | Channel | Description |
|-----|---------|-------------|
| 0   | CH0     | Channel 0 enable |
| 1   | CH1     | Channel 1 enable |
| 2   | CH2     | Channel 2 enable |
| 3   | CH3     | Channel 3 enable |
| 4   | CH4     | Channel 4 enable |
| 5   | CH5     | Channel 5 enable |
| 6   | CH6     | Not used in PaHub v2.1 |
| 7   | CH7     | Not used in PaHub v2.1 |

**Power-on Default:** 0x00 (all channels deselected)

### Register Operations

**Write (Select Channel):**
```cpp
Wire.beginTransmission(0x70);  // PaHub address
Wire.write(1 << channel);      // channel = 0-5
Wire.endTransmission();
delayMicroseconds(500);        // Allow switch time
```

**Read (Get Current Channel Status):**
```cpp
Wire.requestFrom(0x70, 1);
if (Wire.available()) {
    uint8_t status = Wire.read();  // Returns active channels
}
```

**Important Notes:**
- Only ONE channel should be active at a time (for proper operation)
- Multiple channels can be active simultaneously (not recommended)
- Reading register shows which channels are currently selected

## Channel Selection Examples

**Channel 0:** `0x01` (binary: `00000001`)
**Channel 1:** `0x02` (binary: `00000010`)
**Channel 2:** `0x04` (binary: `00000100`)
**Channel 3:** `0x08` (binary: `00001000`)
**Channel 4:** `0x10` (binary: `00010000`)
**Channel 5:** `0x20` (binary: `00100000`)

## Software Implementation

### Basic Channel Selection Function

```cpp
#define PAHUB_ADDR 0x70

bool selectPaHubChannel(uint8_t channel) {
    if (channel > 5) return false;  // Only channels 0-5 available
    
    Wire.beginTransmission(PAHUB_ADDR);
    Wire.write(1 << channel);  // Set bit for channel
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        delayMicroseconds(500);  // Allow switch time
        return true;
    }
    
    return false;
}
```

### Deselect All Channels

```cpp
void deselectAllChannels() {
    Wire.beginTransmission(PAHUB_ADDR);
    Wire.write(0x00);  // Clear all bits
    Wire.endTransmission();
    delay(10);
}
```

### Read Current Channel Status

```cpp
uint8_t readChannelStatus() {
    Wire.requestFrom(PAHUB_ADDR, 1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}
```

## Usage Example

```cpp
// Select channel 0 (Joystick2)
if (selectPaHubChannel(0)) {
    // Now communicate with device on channel 0
    Wire.beginTransmission(0x63);  // Joystick2 address
    Wire.write(0x10);  // Register address
    Wire.endTransmission();
    
    Wire.requestFrom(0x63, 1);
    if (Wire.available()) {
        uint8_t value = Wire.read();
    }
}

// Switch to channel 1 (Scroll A)
selectPaHubChannel(1);
// Now communicate with device on channel 1
```

## Best Practices

1. **Always deselect channels** before selecting a new one
2. **Add delays** after channel switching (500µs minimum)
3. **Check for errors** after channel selection
4. **Only one channel active** at a time for proper operation
5. **Reset on startup** - Deselect all channels on initialization

## Troubleshooting

### Issue 1: Device Not Found

**Symptoms:** I2C device not responding after channel selection

**Solutions:**
1. Verify channel selection worked:
   ```cpp
   uint8_t status = readChannelStatus();
   Serial.printf("Active channels: 0x%02X\n", status);
   ```
2. Check device address is correct
3. Verify I2C connections (SDA/SCL/GND)
4. Try deselecting all channels first:
   ```cpp
   deselectAllChannels();
   delay(10);
   selectPaHubChannel(channel);
   ```

### Issue 2: Wrong Channel Selected

**Symptoms:** Reading from wrong device

**Solutions:**
1. Verify channel selection before each operation
2. Clear all channels before selecting:
   ```cpp
   Wire.beginTransmission(PAHUB_ADDR);
   Wire.write(0x00);  // Deselect all
   Wire.endTransmission();
   delay(10);
   ```

### Issue 3: I2C Bus Lock-up

**Symptoms:** I2C errors, NACK, bus stuck

**Solutions:**
1. Reset PaHub (power cycle or RESET pin)
2. Clear all channels:
   ```cpp
   deselectAllChannels();
   ```
3. Reset I2C bus:
   ```cpp
   Wire.end();
   delay(100);
   Wire.begin(SDA, SCL);
   ```

## Technical Details

### I2C Timing

- **Clock Frequency:** 0 kHz to 400 kHz
- **Supported Speeds:** Standard (100 kHz), Fast (400 kHz)
- **Channel Switch Time:** ~10-50 µs (add delay in code)

### Voltage Translation

**Supported Voltages:**
- 1.8V
- 2.5V
- 3.3V
- 5V

**How it works:**
- VCC pin limits maximum voltage passed through
- External pull-up resistors set bus voltage per channel
- All I/O pins are 5V tolerant

### Power-on Behavior

- All channels deselected on power-up (register = 0x00)
- I2C state machine initialized
- No glitches during power-on

## References

- [PCA9548A Datasheet](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/unit/pahub2/pca9548a.pdf)
- [M5Stack Unit PaHub v2.1 Product Page](https://docs.m5stack.com/en/unit/pahub2)

