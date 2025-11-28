/*
 * M5Unit-Scroll Test for Cardputer-Adv with External ILI9488 Display
 * 
 * M5Unit-Scroll is an encoder/scroll unit from M5Stack
 * 
 * Specifications (from official I2C protocol):
 * - I2C interface (address 0x40)
 * - Rotary encoder (left/right rotation)
 * - Push button
 * - RGB LED indicator
 * 
 * I2C Registers:
 * - 0x10: Encoder value (16-bit, little-endian)
 * - 0x20: Button status (0 or 1)
 * - 0x30: RGB LED control (R, G, B)
 * - 0x40: Reset encoder (write 1)
 * - 0x50: Incremental encoder (16-bit, resets after read!)
 * - 0xF0: Info register (Bootloader/FW version, I2C addr)
 * 
 * ILI9488 Connection via EXT 2.54-14P:
 * - VCC  -> PIN 2 (5VIN)
 * - GND  -> PIN 4 (GND)
 * - SCK  -> PIN 7 (GPIO 40)
 * - MOSI -> PIN 9 (GPIO 14)
 * - MISO -> PIN 11 (GPIO 39)
 * - CS   -> PIN 13 (GPIO 5)
 * - DC   -> PIN 5 (GPIO 6, BUSY)
 * - RST  -> PIN 1 (GPIO 3)
 * 
 * M5Unit-Scroll Connection:
 * - PORT.A (Grove HY2.0-4P)
 * - SDA -> GPIO 2 (G2)
 * - SCL -> GPIO 1 (G1)
 * - GND -> GND
 * - 5V -> 5V
 * 
 * Note: If the module uses a different interface (UART, SPI),
 * the code will need to be adapted after checking the documentation.
 */

#include <M5Cardputer.h>
#include <M5GFX.h>
#include <Wire.h>
#include "lgfx/v1/panel/Panel_LCD.hpp"

// ============================================
// Local Panel_ILI9488 Definition
// ============================================

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
            CMD_PWCTR1,  2, 0x17, 0x15,  // VRH1, VRH2
            CMD_PWCTR2,  1, 0x41,        // VGH, VGL
            CMD_VMCTR ,  3, 0x00, 0x12, 0x80,  // nVM, VCM_REG, VCM_REG_EN
            CMD_FRMCTR1, 1, 0xA0,       // Frame rate = 60Hz
            CMD_INVCTR,  1, 0x02,       // Display Inversion Control = 2dot
            CMD_DFUNCTR, 3, 0x02, 0x22, 0x3B,  // Normal scan
            CMD_ETMOD,   1, 0xC6,
            CMD_ADJCTL3, 4, 0xA9, 0x51, 0x2C, 0x82,  // Adjust Control 3
            CMD_SLPOUT , 0+CMD_INIT_DELAY, 120,  // Exit sleep
            CMD_IDMOFF , 0,                      // Idle mode off
            CMD_DISPON , 0+CMD_INIT_DELAY, 100,  // Display on
            0xFF,0xFF,  // end
        };
        switch (listno) {
        case 0: return list0;
        default: return nullptr;
        }
    }
};

// ============================================
// External ILI9488 Display Configuration
// ============================================

class LGFX_ILI9488 : public lgfx::v1::LGFX_Device {
    Panel_ILI9488_Local panel;
    lgfx::v1::Bus_SPI   bus;

public:
    LGFX_ILI9488() {
        // --- SPI bus ---
        auto b = bus.config();
        b.spi_host   = SPI3_HOST;   // HSPI on ESP32-S3
        b.spi_mode   = 0;
        b.freq_write = 20000000;    // 20 MHz
        b.freq_read  = 16000000;
        b.spi_3wire  = true;        // 3-wire SPI
        b.use_lock   = false;       // No SPI lock
        b.dma_channel = 0;          // No DMA

        b.pin_sclk = 40;            // SCK  -> PIN 7
        b.pin_mosi = 14;            // MOSI -> PIN 9
        b.pin_miso = 39;            // MISO -> PIN 11
        b.pin_dc   = 6;             // DC   -> PIN 5
        
        bus.config(b);
        panel.setBus(&bus);

        // --- Panel config ---
        auto p = panel.config();
        p.pin_cs    = 5;            // CS   -> PIN 13
        p.pin_rst   = 3;            // RST  -> PIN 1
        p.bus_shared = false;       // Not shared with SD
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
        p.readable = true;
        panel.config(p);

        setPanel(&panel);
    }
};

LGFX_ILI9488 lcd;  // External ILI9488 display

// ============================================
// I2C Settings
// ============================================
#define UNIT_SCROLL_I2C_ADDRESS 0x40  // Official address from I2C protocol

// Use Wire for PORT.A (GPIO 2/1) - shared I2C controller with keyboard
#define I2C_SDA_PIN 2  // GPIO 2 (G2)
#define I2C_SCL_PIN 1  // GPIO 1 (G1)

// Registers from official I2C protocol (M5Stack Unit Scroll Protocol)
#define SCROLL_ENCODER_REG     0x10  // Encoder value (16-bit, little-endian: byte0 + byte1*256)
#define SCROLL_BUTTON_REG      0x20  // Button status (R: 0 or 1)
#define SCROLL_RGB_REG         0x30  // RGB LED control (W/R: NULL, R, G, B)
#define SCROLL_RESET_REG       0x40  // Reset encoder (W: write 1 to reset encoder)
#define SCROLL_INC_ENCODER_REG 0x50  // Incremental encoder (R: 16-bit, resets after read!)
#define SCROLL_INFO_REG        0xF0  // Info register (R: Bootloader/FW version, I2C addr)

int lastEncoderValue = 0;
int lastIncEncoderValue = 0;
bool lastButtonState = false;
bool moduleFound = false;
uint8_t foundAddress = 0;

// ============================================
// Scroll Screen State
// ============================================
bool showScrollTest = false;  // Flag to show scroll test screen
unsigned long screenSwitchTime = 0;  // Last screen switch time
const int SCREEN_SWITCH_DELAY = 500;  // Delay after screen switch before I2C read (ms)
const int MAX_LIST_ITEMS = 20;  // Maximum items in list
String listItems[MAX_LIST_ITEMS] = {
    "Item 1", "Item 2", "Item 3", "Item 4", "Item 5",
    "Item 6", "Item 7", "Item 8", "Item 9", "Item 10",
    "Item 11", "Item 12", "Item 13", "Item 14", "Item 15",
    "Item 16", "Item 17", "Item 18", "Item 19", "Item 20"
};
bool listItemsChecked[MAX_LIST_ITEMS] = {false};  // Check flags
int listItemCount = MAX_LIST_ITEMS;  // Number of items
int selectedListItem = 0;  // Selected item (0-based)
int lastScrollNavTime = 0;  // Last navigation time (debounce)
const int SCROLL_NAV_DEBOUNCE = 150;  // Navigation debounce (ms)
unsigned long lastScrollReadTime = 0;  // Last encoder read time
const int SCROLL_READ_INTERVAL = 50;  // Encoder read interval (ms) - don't read too often
int i2cErrorCount = 0;  // I2C error counter
const int MAX_I2C_ERRORS = 10;  // Maximum consecutive errors before skipping reads (increased)

// ============================================
// State for scroll rendering optimization
// ============================================
bool scrollScreenInitialized = false;  // First screen initialization flag
int lastFirstVisible = -1;  // Last first visible item (for optimization)

// ============================================
// Soft I2C Bus Reset
// ============================================
void i2cBusReset() {
    Serial.println(">>> I2C Bus Reset: Recovering from errors...");
    
    // Stop shared I2C
    Wire.end();
    delay(50);
    
    // Toggle SCL 9 times to release stuck slave
    pinMode(I2C_SCL_PIN, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
    }
    
    // Return pins to "I2C mode"
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    delay(50);
    
    // Reinitialize Wire on port A
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);  // 50 kHz for STM32F030
    Wire.setTimeOut(100);
    
    Serial.println(">>> I2C Bus Reset: Complete");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n========================================");
    Serial.println("M5Unit-Scroll Test - External Display");
    Serial.println("Cardputer-Adv");
    Serial.println("========================================");
    Serial.println("Built-in display: DISABLED");
    Serial.println("External display: ILI9488 (480x320)");
    Serial.println("\nNOTE: This is a basic test template.");
    Serial.println("Actual I2C address and registers may differ!");
    Serial.println("Check M5Unit-Scroll documentation.");
    
    // Initialize external ILI9488
    Serial.println("\nInitializing ILI9488 display...");
    
    if (lcd.init()) {
        Serial.println("  ✓ ILI9488 initialized successfully!");
        Serial.printf("  ✓ Display size: %dx%d\n", lcd.width(), lcd.height());
        
        lcd.setRotation(3);  // 180 degrees
        lcd.setColorDepth(24);
        
        lcd.fillScreen(TFT_BLACK);
        lcd.setTextSize(2);  // Larger font size for large screen
        lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        
        Serial.println("\nReady! Display initialized...");
        Serial.println("----------------------------------------\n");
        
        delay(200);
    } else {
        Serial.println("\n  ✗ ERROR: ILI9488 initialization FAILED!");
        Serial.println("\n  Troubleshooting:");
        Serial.println("    1. Check power supply");
        Serial.println("    2. Check all connections");
        return;
    }
    
    // Initialize Cardputer AFTER display
    M5Cardputer.begin(true);
    delay(200);
    
    // Disable built-in display backlight
    M5.Display.setBrightness(0);
    Serial.println("  ✓ Built-in display backlight: DISABLED");
    
    // Initialize I2C on PORT.A
    Serial.println("\nInitializing I2C...");
    Serial.println("Using PORT.A (GPIO 2/1)");
    Serial.println("CRITICAL: Using Wire (shared with keyboard) instead of Wire1");
    
    // IMPORTANT: STM32F030 uses pull-up resistors, ESP32 must enable them too
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    delay(100);  // Give time for pull-up stabilization
    
    // FIX: Use Wire instead of Wire1 (shared controller)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);  // 50 kHz for STM32F030
    Wire.setTimeOut(100);
    
    Serial.printf("  SDA: GPIO %d (G2)\n", I2C_SDA_PIN);
    Serial.printf("  SCL: GPIO %d (G1)\n", I2C_SCL_PIN);
    Serial.printf("  Address: 0x%02X\n", UNIT_SCROLL_I2C_ADDRESS);
    Serial.printf("  Speed: 50 kHz\n");
    Serial.printf("  Clock stretching: Enabled\n");
    
    // STM32F030 may require more initialization time after power-on
    Serial.println("\nWaiting for STM32F030 initialization...");
    delay(1000);  // Increased delay for STM32F030
    
    // Scan I2C bus for module
    Serial.println("\nScanning I2C bus for Scroll module...");
    Serial.println("STM32F030 I2C slave - trying multiple detection methods");
    Serial.println("(Trying addresses: 0x40, 0x5E, 0x5F, 0x41, 0x42)");
    
    uint8_t addresses[] = {0x40, 0x5E, 0x5F, 0x41, 0x42};
    bool found = false;
    uint8_t foundAddress = 0;
    
    // Method 1: Standard scan
    Serial.println("\nMethod 1: Standard I2C scan");
    for (int i = 0; i < 5; i++) {
        Wire.beginTransmission(addresses[i]);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("  ✓ Device found at 0x%02X!\n", addresses[i]);
            found = true;
            foundAddress = addresses[i];
            moduleFound = true;
            break;
        }
        delay(50);  // Increased delay between attempts for STM32F030
    }
    
    // Method 2: Direct register read (STM32F030 may not respond to beginTransmission)
    if (!found) {
        Serial.println("\nMethod 2: Direct register read (STM32F030 I2C slave mode)");
        Serial.println("Trying to read register 0x20 (Button register)...");
        
        for (int attempt = 0; attempt < 3; attempt++) {
            Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
            Wire.write(SCROLL_BUTTON_REG);  // 0x20
            byte error = Wire.endTransmission(false);  // false = repeated start (important for STM32F030)
            
            if (error == 0) {
                delayMicroseconds(1000);  // Increased delay for STM32F030
                uint8_t bytesReceived = Wire.requestFrom(UNIT_SCROLL_I2C_ADDRESS, 1, true);
                
                if (bytesReceived > 0 && Wire.available()) {
                    uint8_t buttonState = Wire.read();
                    Serial.printf("  ✓ Module responds! Button state: %d\n", buttonState);
                    Serial.printf("  ✓ Found at 0x%02X (via register read)\n", UNIT_SCROLL_I2C_ADDRESS);
                    found = true;
                    foundAddress = UNIT_SCROLL_I2C_ADDRESS;
                    moduleFound = true;
                    break;
                }
            }
            delay(200);  // Delay between attempts
        }
    }
    
    // Method 3: Try different I2C speeds (STM32F030 may require different speed)
    if (!found) {
        Serial.println("\nMethod 3: Trying different I2C speeds");
        uint32_t speeds[] = {50000, 100000, 200000};  // 50kHz, 100kHz, 200kHz
        
        for (int s = 0; s < 3; s++) {
            Serial.printf("  Trying %d kHz...\n", speeds[s] / 1000);
            Wire.end();
            delay(100);
            Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, speeds[s]);
            Wire.setTimeOut(100);
            delay(200);
            
            Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
            byte error = Wire.endTransmission();
            
            if (error == 0) {
                Serial.printf("  ✓ Device found at 0x%02X with %d kHz!\n", UNIT_SCROLL_I2C_ADDRESS, speeds[s] / 1000);
                found = true;
                foundAddress = UNIT_SCROLL_I2C_ADDRESS;
                moduleFound = true;
                break;
            }
            delay(100);
        }
        
        // Return to standard speed
        if (!found) {
            Wire.end();
            delay(100);
            Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);
            Wire.setTimeOut(100);
        }
    }
    
    if (!found) {
        Serial.println("\n  ✗ Module not found!");
        Serial.println("\nSTM32F030 Troubleshooting:");
        Serial.println("  1. Check physical connection:");
        Serial.println("     - SDA -> GPIO 2 (G2) on PORT.A");
        Serial.println("     - SCL -> GPIO 1 (G1) on PORT.A");
        Serial.println("     - GND -> GND (MUST be connected!)");
        Serial.println("     - 5V -> 5V (check module LED is on)");
        Serial.println("  2. STM32F030 specific:");
        Serial.println("     - Module needs pull-up resistors (enabled in code)");
        Serial.println("     - Module may need power cycle (unplug/replug)");
        Serial.println("     - Module may be in bootloader mode");
        Serial.println("     - Try external pull-up resistors (4.7kΩ to 3.3V)");
        Serial.println("  3. Timing issues:");
        Serial.println("     - STM32F030 may need slower I2C speed");
        Serial.println("     - Try 50 kHz instead of 100 kHz");
        Serial.println("  4. Module firmware:");
        Serial.println("     - Module may need firmware update");
        Serial.println("     - Check if module LED blinks (bootloader mode)");
        
        // Display error on screen
        lcd.fillScreen(TFT_BLACK);
        lcd.setCursor(10, 10);
        lcd.setTextColor(TFT_RED, TFT_BLACK);
        lcd.println("Unit-Scroll");
        lcd.println("NOT FOUND!");
        lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        lcd.setCursor(10, 100);
        lcd.println("STM32F030 module");
        lcd.setCursor(10, 130);
        lcd.println("Check:");
        lcd.setCursor(10, 160);
        lcd.println("1. Power (5V)");
        lcd.setCursor(10, 190);
        lcd.println("2. Pull-up resistors");
    } else {
        // Module found - set flag
        moduleFound = true;
        
        // Try to read module information
        Serial.println("\n✓ Module found! Testing communication...");
        
        // First try to read I2C address (register 0xFF from firmware)
        // From firmware: reading 0xFF returns module's I2C address
        Wire.beginTransmission(foundAddress);
        Wire.write(0xFF);  // I2C address read command
        Wire.endTransmission(false);
        delayMicroseconds(200);
        
        uint8_t addrBytes = Wire.requestFrom(foundAddress, 1, true);
        if (addrBytes > 0 && Wire.available()) {
            uint8_t moduleAddr = Wire.read();
            Serial.printf("  ✓ Module responds! I2C Address: 0x%02X\n", moduleAddr);
        } else {
            Serial.println("  ⚠️ Module found but does not respond to commands");
            Serial.println("  Trying direct register read...");
            
            // Try to read button directly (register 0x20)
            Wire.beginTransmission(foundAddress);
            Wire.write(0x20);  // Button register
            Wire.endTransmission(false);
            delayMicroseconds(200);
            
            uint8_t buttonBytes = Wire.requestFrom(foundAddress, 1, true);
            if (buttonBytes > 0 && Wire.available()) {
                uint8_t buttonState = Wire.read();
                Serial.printf("  ✓ Button register read OK! State: %d\n", buttonState);
            } else {
                Serial.println("  ✗ Even button register fails - check wiring!");
                Serial.println("  Possible issues:");
                Serial.println("    1. Wrong I2C pins (should be GPIO 2/1 for PORT.A)");
                Serial.println("    2. Module may need pull-up resistors");
                Serial.println("    3. Module may need power cycle");
            }
        }
        
        // Display successful connection
        lcd.fillScreen(TFT_BLACK);
        lcd.setCursor(10, 10);
        lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        lcd.setTextSize(2);
        lcd.println("M5Unit-Scroll Test");
        lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        lcd.setCursor(10, 50);
        lcd.setTextSize(2);
        lcd.printf("Found at 0x%02X", foundAddress);
        lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        lcd.setCursor(10, 90);
        lcd.setTextSize(2);
        lcd.println("Rotate encoder...");
        lcd.setCursor(10, 130);
        lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        lcd.setTextSize(2);
        lcd.println("Press SPACE for");
        lcd.setCursor(10, 160);
        lcd.println("scroll test");
    }
    
    Serial.println("\n========================================");
    Serial.println("Ready! Rotate encoder or press button");
    Serial.println("========================================");
    Serial.println();
}

// Improved register read function with error handling
bool readScrollRegister(uint8_t reg, uint8_t length, uint8_t* data) {
    // Check if module is found
    if (!moduleFound) {
        return false;
    }
    
    // Use found address if set, otherwise constant
    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
    
    // Send register address
    Wire.beginTransmission(address);
    Wire.write(reg);
    byte error = Wire.endTransmission(false);  // false = не останавливать шину (repeated start)
    
    if (error != 0) {
        i2cErrorCount++;
        return false;
    }
    
    // Delay for STM32F030 before read (reduced for stability)
    delayMicroseconds(500);  // Optimal delay for STM32F030
    
    // Request data
    uint8_t bytesReceived = Wire.requestFrom(address, length, true);
    
    if (bytesReceived == 0) {
        i2cErrorCount++;
        return false;
    }
    
    // Small delay after requestFrom for STM32F030
    delayMicroseconds(300);
    
    // Read data (max 16 bytes for info register)
    for (int i = 0; i < length && i < 16; i++) {
        if (Wire.available()) {
            data[i] = Wire.read();
        } else {
            i2cErrorCount++;
            return false;
        }
    }
    
    // Successful read - reset error counter
    i2cErrorCount = 0;
    return true;
}

// Wrapper for backward compatibility
int readScrollRegisterValue(uint8_t reg, uint8_t length) {
    uint8_t data[4] = {0};
    
    if (!readScrollRegister(reg, length, data)) {
        // Read error - return last known value
        if (length == 2) {
            return lastEncoderValue;
        } else {
            return lastButtonState ? 1 : 0;
        }
    }
    
    if (length == 2) {
        // 16-bit value (encoder)
        return (int16_t)(data[0] | (data[1] << 8));
    } else {
        // 8-bit value (button)
        return data[0];
    }
}

// ============================================
// Function to control RGB LED
// ============================================
// color: format 0xRRGGBB (e.g., 0xFF0000 = red, 0x00FF00 = green, 0x0000FF = blue)
// From firmware: neopixel_set_color() expects R in byte 8-15, G in byte 16-23, B in byte 24-31
// So write: 0x31 = R, 0x32 = G, 0x33 = B
void setScrollLEDFast(uint32_t color) {
    if (!moduleFound) {
        return;
    }
    
    // Use found address if set, otherwise constant
    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
    
    // Extract RGB from 0xRRGGBB format
    uint8_t r = (color >> 16) & 0xFF;  // Red
    uint8_t g = (color >> 8) & 0xFF;   // Green
    uint8_t b = color & 0xFF;          // Blue
    
    // Write R, G, B to registers 0x31, 0x32, 0x33
    // neopixel_set_color() ожидает: байт 8-15 = R, байт 16-23 = G, байт 24-31 = B
    Wire.beginTransmission(address);
    Wire.write(0x31);  // Start register for R
    Wire.write(r);     // R → байт 8-15 буфера
    Wire.write(g);     // G → байт 16-23 буфера
    Wire.write(b);     // B → байт 24-31 буфера
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        Serial.printf(">>> LED set to RGB(%d, %d, %d) = 0x%06X\n", r, g, b, color);
    } else {
        Serial.printf(">>> LED write error: %d\n", error);
        i2cErrorCount++;
    }
}

// ============================================
// Scroll test screen rendering function (OPTIMIZED)
// ============================================
// Style like ZX Spectrum emulator
// Uses partial updates to eliminate flickering
void drawScrollTest() {
    // Calculate first visible item (scrolling)
    int firstVisible = selectedListItem - 3;
    if (firstVisible < 0) firstVisible = 0;
    
    // Group all SPI operations to eliminate flickering
    lcd.startWrite();
    
    // Redraw static elements only on first initialization
    if (!scrollScreenInitialized) {
        // ═══ ЗАГОЛОВОК ═══
        lcd.setTextSize(2);
        lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        lcd.setCursor(20, 10);
        lcd.print("Scroll Test");
        
        // ═══ ПОДСКАЗКИ УПРАВЛЕНИЯ ═══
        lcd.setTextSize(2);
        lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        lcd.setCursor(10, 240);
        lcd.print("Scroll=Nav Button=Check Space=Back");
        
        scrollScreenInitialized = true;
    }
    
    // Update counter (right) - always
    lcd.fillRect(360, 10, 120, 20, TFT_BLACK);  // Clear counter area
    lcd.setTextSize(2);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    lcd.setCursor(360, 10);
    lcd.printf("%d/%d", selectedListItem + 1, listItemCount);
    
    // Redraw frame ALWAYS (so it doesn't disappear)
    lcd.drawRect(20, 40, 440, 190, TFT_WHITE);
    
    // Clear list area with margin to eliminate artifacts
    // Clear entire inner frame area (with 2px margin from frame edges)
    // This ensures removal of all artifacts from previous renders
    lcd.fillRect(22, 42, 436, 186, TFT_BLACK);  // Inside frame with 2px margin
    
    // Update lastFirstVisible to track changes
    if (firstVisible != lastFirstVisible) {
        lastFirstVisible = firstVisible;
    }
    
    int y = 50;  // Initial Y position
    
    for (int i = 0; i < 8; i++) {
        int itemIdx = firstVisible + i;
        
        // Check bounds
        if (itemIdx >= listItemCount) break;
        
        bool isSelected = (itemIdx == selectedListItem);
        bool isChecked = listItemsChecked[itemIdx];
        
        // Set size and color
        if (isSelected) {
            lcd.setTextSize(4);  // Large font for selected
            lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        } else {
            lcd.setTextSize(2);  // Small font for others
            lcd.setTextColor(isChecked ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
        }
        
        lcd.setCursor(30, y);
        
        // Get item name
        String name = listItems[itemIdx];
        
        // Truncate long names
        int maxLen = isSelected ? 20 : 35;
        if (name.length() > maxLen) {
            name = name.substring(0, maxLen - 3) + "...";
        }
        
        // Show [✓] mark before name if item is checked
        if (isChecked) {
            lcd.print("[✓] ");
        }
        
        lcd.print(name);
        
        // Next line
        y += isSelected ? 32 : 22;
    }
    
    lcd.endWrite();  // End SPI operation group
}

// ============================================
// Function to reset state when switching screens
// ============================================
void resetScrollScreenState() {
    scrollScreenInitialized = false;
    lastFirstVisible = -1;
}

void loop() {
    M5Cardputer.update();
    
    // ═══ ОБРАБОТКА КЛАВИАТУРЫ (переключение экранов) ═══
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        auto keys = M5Cardputer.Keyboard.keysState();
        
        // Space - switch between screens
        if (keys.space) {
            showScrollTest = !showScrollTest;
            Serial.printf(">>> Screen switched: %s\n", showScrollTest ? "Scroll Test" : "Main");
            
            // Remember screen switch time
            screenSwitchTime = millis();
            
            // Reset error counter and state when switching screens
            i2cErrorCount = 0;
            lastScrollReadTime = 0;
            lastScrollNavTime = 0;
            lastButtonState = false;  // Reset button state
            
            if (showScrollTest) {
                // Switch to scroll screen - FULL screen clear before switch
                lcd.fillScreen(TFT_BLACK);  // Full clear to remove artifacts
                resetScrollScreenState();
                drawScrollTest();
            } else {
                // Return to main screen
                lcd.fillScreen(TFT_BLACK);
                lcd.setCursor(10, 10);
                lcd.setTextSize(2);
                lcd.setTextColor(TFT_CYAN, TFT_BLACK);
                lcd.println("M5Unit-Scroll Test");
                lcd.setCursor(10, 50);
                lcd.setTextColor(TFT_GREEN, TFT_BLACK);
                lcd.setTextSize(2);
                lcd.printf("Found at 0x%02X", foundAddress);
                lcd.setCursor(10, 90);
                lcd.setTextColor(TFT_WHITE, TFT_BLACK);
                lcd.setTextSize(2);
                lcd.println("Rotate encoder...");
                lcd.setCursor(10, 130);
                lcd.setTextColor(TFT_CYAN, TFT_BLACK);
                lcd.setTextSize(2);
                lcd.println("Press SPACE for");
                lcd.setCursor(10, 160);
                lcd.println("scroll test");
            }
            delay(200);  // Debounce
            return;
        }
    }
    
    if (!moduleFound) {
        // Module not found - don't try to read
        delay(1000);
        return;
    }
    
    // ═══ ЕСЛИ ЭКРАН СКРОЛЛА АКТИВЕН ═══
    if (showScrollTest) {
        unsigned long currentTime = millis();
        
        // IMPORTANT: Don't read I2C immediately after screen switch!
        // Give module time to recover after switch
        if (screenSwitchTime > 0 && (currentTime - screenSwitchTime) < SCREEN_SWITCH_DELAY) {
            delay(10);
            return;
        }
        
        // Check read interval - don't read too often
        if (currentTime - lastScrollReadTime < SCROLL_READ_INTERVAL) {
            delay(10);
            return;
        }
        lastScrollReadTime = currentTime;
        
        // If too many errors in a row - do soft bus reset
        if (i2cErrorCount >= MAX_I2C_ERRORS) {
            i2cBusReset();  // Soft bus reset
            i2cErrorCount = 0;
            delay(200);
            return;  // Skip read after errors
        }
        
        // Read incremental encoder value for navigation
        uint8_t incData[2] = {0};
        if (readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData)) {
            i2cErrorCount = 0;  // Successful read - reset error counter
            int16_t incValue = (int16_t)(incData[0] | (incData[1] << 8));
            
            if (incValue != 0 && (currentTime - lastScrollNavTime > SCROLL_NAV_DEBOUNCE)) {
                // Navigate list
                if (incValue > 0) {
                    // Rotate right → list down
                    if (selectedListItem < listItemCount - 1) {
                        selectedListItem++;
                        drawScrollTest();
                        Serial.printf(">>> Scroll DOWN → Item %d/%d\n", selectedListItem + 1, listItemCount);
                        setScrollLEDFast(0x00FF00);  // Green when scrolling down
                    }
                } else {
                    // Rotate left → list up
                    if (selectedListItem > 0) {
                        selectedListItem--;
                        drawScrollTest();
                        Serial.printf(">>> Scroll UP → Item %d/%d\n", selectedListItem + 1, listItemCount);
                        setScrollLEDFast(0xFF0000);  // Red when scrolling up
                    }
                }
                lastScrollNavTime = currentTime;
            }
        } else {
            // Read error - increment counter (but don't block completely)
            i2cErrorCount++;
            // Log only every 5th error to avoid cluttering Serial
            if (i2cErrorCount % 5 == 0) {
                Serial.printf(">>> I2C read errors: %d\n", i2cErrorCount);
            }
        }
        
        // Read button state for check/uncheck (less often than encoder)
        static unsigned long lastButtonReadTime = 0;
        if (currentTime - lastButtonReadTime > 100) {  // Read button once per 100ms
            uint8_t buttonData[1] = {0};
            if (readScrollRegister(SCROLL_BUTTON_REG, 1, buttonData)) {
                i2cErrorCount = 0;  // Successful read - reset counter
                bool buttonState = (buttonData[0] != 0);
                
                if (buttonState && !lastButtonState) {
                    // Button pressed - toggle check
                    listItemsChecked[selectedListItem] = !listItemsChecked[selectedListItem];
                    drawScrollTest();
                    Serial.printf(">>> Item %d %s\n", selectedListItem + 1, 
                                 listItemsChecked[selectedListItem] ? "CHECKED" : "UNCHECKED");
                    setScrollLEDFast(0x0000FF);  // Blue when checking
                    delay(100);
                    setScrollLEDFast(0x000000);  // Turn off LED
                }
                
                lastButtonState = buttonState;
            }
            lastButtonReadTime = currentTime;
        }
        
        delay(10);
        return;
    }
    
    // ═══ ГЛАВНЫЙ ЭКРАН (оригинальный тест) ═══
    // Reset error counter when returning to main screen
    i2cErrorCount = 0;
    
    // Read incremental encoder value (0x50) - this is what we need!
    // This register shows change since last read and automatically resets
    uint8_t incData[2] = {0};
    if (readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData)) {
        i2cErrorCount = 0;  // Successful read - reset error counter
        int16_t incValue = (int16_t)(incData[0] | (incData[1] << 8));  // little-endian
        
        if (incValue != 0) {
            // Rotation detected!
            lastEncoderValue += incValue;  // Update total value
            Serial.printf(">>> Encoder Increment: %+d (Total: %d)\n", incValue, lastEncoderValue);
            
            // Change LED color based on rotation direction
            if (incValue > 0) {
                // Rotate right - green
                setScrollLEDFast(0x00FF00);  // Green
            } else {
                // Rotate left - red
                setScrollLEDFast(0xFF0000);  // Red
            }
            
            // Display on external screen
            lcd.fillRect(0, 130, 480, 100, TFT_BLACK);
            lcd.setCursor(10, 140);
            lcd.setTextColor(TFT_GREEN, TFT_BLACK);
            lcd.setTextSize(3);
            lcd.printf("Encoder: %d\n", lastEncoderValue);
            lcd.setTextSize(2);
            lcd.setCursor(10, 200);
            lcd.setTextColor(TFT_CYAN, TFT_BLACK);
            lcd.printf("Increment: %+d", incValue);
        }
    }
    
    // Also read absolute encoder value (0x10) for synchronization
    int encoderValue = readScrollRegisterValue(SCROLL_ENCODER_REG, 2);
    if (encoderValue != lastEncoderValue) {
        // Update if changed (in case we missed increment)
        lastEncoderValue = encoderValue;
    }
    
    // Read button state (0x20)
    uint8_t buttonData[1] = {0};
    if (readScrollRegister(SCROLL_BUTTON_REG, 1, buttonData)) {
        bool buttonState = (buttonData[0] != 0);
        
        if (buttonState != lastButtonState) {
            Serial.printf(">>> Button: %s\n", buttonState ? "PRESSED" : "RELEASED");
            
            // Change LED color on button press
            if (buttonState) {
                // Button pressed - blue
                setScrollLEDFast(0x0000FF);  // Blue
            } else {
                // Button released - turn off LED
                setScrollLEDFast(0x000000);  // Black (off)
            }
            
            // Display on external screen
            lcd.fillRect(0, 240, 480, 80, TFT_BLACK);
            lcd.setCursor(10, 250);
            lcd.setTextSize(2);
            lcd.setTextColor(buttonState ? TFT_RED : TFT_WHITE, TFT_BLACK);
            lcd.printf("Button: %s", buttonState ? "PRESSED" : "RELEASED");
            
            // On button press - reset encoder (like MicroPython example)
            if (buttonState) {
                if (moduleFound) {
                    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
                    Wire.beginTransmission(address);
                    Wire.write(SCROLL_RESET_REG);
                    Wire.write(1);  // Write 1 to reset encoder
                    Wire.endTransmission();
                }
                lastEncoderValue = 0;
                Serial.println(">>> Encoder reset!");
                
                // Update display
                lcd.fillRect(0, 130, 480, 100, TFT_BLACK);
                lcd.setCursor(10, 140);
                lcd.setTextColor(TFT_GREEN, TFT_BLACK);
                lcd.setTextSize(3);
                lcd.printf("Encoder: 0\n");
            }
            
            lastButtonState = buttonState;
        }
    }
    
    delay(50);  // Small delay for stability
}

