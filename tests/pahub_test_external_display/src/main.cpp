/*
 * PA Hub Test for Cardputer-Adv on External ILI9488 Display
 * 
 * Test for PA Hub with joystick, scroll encoders, and keyboard
 * Works on external ILI9488 display (480x320)
 * 
 * CONNECTED DEVICES:
 * - PA Hub (PCA9548A) - address 0x70 on PORT.A (GPIO 2/1)
 *   - Channel 0: Joystick2 (address 0x63)
 *   - Channel 1: Scroll Button A (address 0x40)
 *   - Channel 2: Scroll Button B (address 0x40)
 *   - Channel 3: CardKeyBoard (address 0x5F)
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
 * PA Hub Connection:
 * - Connect to PORT.A (GPIO 2/1)
 * - VCC -> 5V or 3.3V
 * - GND -> GND
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
        b.use_lock   = false;       // No SPI locking
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
        p.bus_shared = false;       // Don't share bus with SD
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
// PA Hub and Device Constants
// ============================================

#define PAHUB_ADDR           0x70
#define PAHUB_CH_JOYSTICK2   0
#define PAHUB_CH_SCROLL_A    1
#define PAHUB_CH_SCROLL_B    2
#define PAHUB_CH_KEYBOARD    3

#define JOYSTICK2_ADDR       0x63
#define JOYSTICK2_REG_ADC_X_8   0x10
#define JOYSTICK2_REG_ADC_Y_8   0x11
#define JOYSTICK2_REG_BUTTON    0x20

#define SCROLL_ADDR          0x40
#define SCROLL_ENCODER_REG   0x10  // Absolute encoder (16-bit)
#define SCROLL_INC_ENCODER_REG 0x50  // Incremental encoder (16-bit, resets after read)
#define SCROLL_BUTTON_REG    0x20

#define CARDKEYBOARD_ADDR    0x5F

#define DEBOUNCE_DELAY_MS    50
#define POLL_INTERVAL_KB     20   // Keyboard polling interval (ms)
#define POLL_INTERVAL_JOY    10   // Joystick polling interval (ms)
#define POLL_INTERVAL_BTN    10   // Button polling interval (ms)
#define POLL_INTERVAL_ENC    50   // Encoder polling interval (ms) - not too frequent!

// ============================================
// Global Variables
// ============================================

bool pahub_available = false;
bool joystick2_available = false;
bool scroll_a_available = false;
bool scroll_b_available = false;
bool keyboard_available = false;

uint8_t pahub_current_channel = 0xFF;

// Button state with debounce
struct ButtonState {
    bool current_state;
    bool last_state;
    uint32_t last_change_time;
    bool debounced_state;
    bool last_debounced_state;  // For edge detection
};

ButtonState scroll_a_state = {false, false, 0, false, false};
ButtonState scroll_b_state = {false, false, 0, false, false};

unsigned long last_kb_poll = 0;
unsigned long last_joy_poll = 0;
unsigned long last_btn_poll = 0;
unsigned long last_enc_poll = 0;

// Encoder values
int16_t scroll_a_encoder_value = 0;
int16_t scroll_b_encoder_value = 0;

// Screen output buffer
#define MAX_OUTPUT_LINES 50
String outputLines[MAX_OUTPUT_LINES];
int outputLineCount = 0;
int scrollOffset = 0;
#define LINE_HEIGHT 24  // Increased for font size 2
#define VISIBLE_LINES 13  // Decreased due to larger font

// Display state for partial updates
struct DisplayState {
    bool initialized = false;
    String lastLines[VISIBLE_LINES];
    int lastScrollOffset = -1;
};

static DisplayState displayState;
bool needRedraw = false;  // Flag for batching (deferred redraw)

// ============================================
// PA Hub Functions
// ============================================

bool selectPaHubChannel(uint8_t channel) {
    if (channel > 5) return false;
    if (pahub_current_channel == channel) return true;
    
    Wire1.beginTransmission(PAHUB_ADDR);
    Wire1.write(1 << channel);
    byte error = Wire1.endTransmission();
    
    if (error == 0) {
        pahub_current_channel = channel;
        delayMicroseconds(500);  // Small delay for channel switching
        return true;
    }
    
    return false;
}

// ============================================
// Device Reading Functions
// ============================================

bool readJoystick2(uint8_t* x, uint8_t* y, uint8_t* button) {
    if (!joystick2_available || !pahub_available) return false;
    
    if (!selectPaHubChannel(PAHUB_CH_JOYSTICK2)) return false;
    
    // Read X
    Wire1.beginTransmission(JOYSTICK2_ADDR);
    Wire1.write(JOYSTICK2_REG_ADC_X_8);
    if (Wire1.endTransmission(false) != 0) return false;
    
    Wire1.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire1.available() >= 1) {
        *x = Wire1.read();
    } else {
        return false;
    }
    
    // Read Y
    Wire1.beginTransmission(JOYSTICK2_ADDR);
    Wire1.write(JOYSTICK2_REG_ADC_Y_8);
    if (Wire1.endTransmission(false) != 0) return false;
    
    Wire1.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire1.available() >= 1) {
        *y = Wire1.read();
    } else {
        return false;
    }
    
    // Read button
    Wire1.beginTransmission(JOYSTICK2_ADDR);
    Wire1.write(JOYSTICK2_REG_BUTTON);
    if (Wire1.endTransmission(false) != 0) return false;
    
    Wire1.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire1.available() >= 1) {
        uint8_t btn = Wire1.read();
        *button = (btn == 0) ? 1 : 0;  // Invert
    } else {
        return false;
    }
    
    return true;
}

bool readScrollButton(uint8_t channel, bool* pressed) {
    if (!pahub_available) return false;
    
    if (!selectPaHubChannel(channel)) return false;
    
    Wire1.beginTransmission(SCROLL_ADDR);
    Wire1.write(SCROLL_BUTTON_REG);
    if (Wire1.endTransmission(false) != 0) return false;
    
    delayMicroseconds(500);  // Delay for STM32F030
    
    Wire1.requestFrom(SCROLL_ADDR, 1, true);
    delayMicroseconds(300);
    
    if (Wire1.available() >= 1) {
        uint8_t btn = Wire1.read();
        *pressed = (btn == 0);
        return true;
    }
    
    return false;
}

// Read incremental encoder value (register 0x50)
bool readScrollEncoder(uint8_t channel, int16_t* increment) {
    if (!pahub_available) return false;
    
    if (!selectPaHubChannel(channel)) return false;
    
    Wire1.beginTransmission(SCROLL_ADDR);
    Wire1.write(SCROLL_INC_ENCODER_REG);
    if (Wire1.endTransmission(false) != 0) return false;
    
    delayMicroseconds(500);  // Delay for STM32F030
    
    Wire1.requestFrom(SCROLL_ADDR, 2, true);  // 16-bit value
    delayMicroseconds(300);
    
    if (Wire1.available() >= 2) {
        uint8_t low = Wire1.read();
        uint8_t high = Wire1.read();
        *increment = (int16_t)(low | (high << 8));  // little-endian
        return true;
    }
    
    return false;
}

void updateButtonState(ButtonState* state, uint8_t channel) {
    bool current_raw = false;
    if (!readScrollButton(channel, &current_raw)) {
        return;
    }
    
    state->current_state = current_raw;
    uint32_t now = millis();
    
    if (state->current_state != state->last_state) {
        state->last_change_time = now;
        state->last_state = state->current_state;
    }
    
    state->last_debounced_state = state->debounced_state;
    
    if ((now - state->last_change_time) >= DEBOUNCE_DELAY_MS) {
        state->debounced_state = state->current_state;
    }
}

uint8_t readKeyboardKey() {
    if (!keyboard_available || !pahub_available) {
        return 0;
    }
    
    if (!selectPaHubChannel(PAHUB_CH_KEYBOARD)) {
        return 0;
    }
    
    Wire1.requestFrom(CARDKEYBOARD_ADDR, 1);
    delayMicroseconds(500);  // Delay for Slave response
    
    if (Wire1.available() > 0) {
        uint8_t key = Wire1.read();
        return key;
    }
    
    return 0;
}

// ============================================
// Display Functions
// ============================================

// Forward declaration
void redrawScreen();

void addOutputLine(const String& text, uint16_t color = TFT_WHITE, bool logToSerial = true) {
    if (outputLineCount < MAX_OUTPUT_LINES) {
        outputLines[outputLineCount] = text;
        outputLineCount++;
    } else {
        // Shift buffer up
        for (int i = 0; i < MAX_OUTPUT_LINES - 1; i++) {
            outputLines[i] = outputLines[i + 1];
        }
        outputLines[MAX_OUTPUT_LINES - 1] = text;
    }
    
    // === 2. Serial Logging ===
    if (logToSerial) {
        // Using c_str(), no String magic
        Serial.print("[APP] ");
        Serial.println(text.c_str());
        // Flush is almost not needed on ESP32, but we'll keep it:
        Serial.flush();
    }
    
    // Recalculate scrollOffset
    int newScrollOffset = max(0, outputLineCount - VISIBLE_LINES);
    if (newScrollOffset != scrollOffset) {
        scrollOffset = newScrollOffset;
        // Scroll change requires full redraw
        displayState.lastScrollOffset = -1;
    }
    
    // Set flag for deferred redraw (batching)
    needRedraw = true;
}

void redrawScreen() {
    lcd.startWrite();
    
    int startLine = scrollOffset;
    int endLine = min(startLine + VISIBLE_LINES, outputLineCount);
    
    // First initialization or scroll change - full redraw
    if (!displayState.initialized || scrollOffset != displayState.lastScrollOffset) {
        // Clear entire text output area (always all VISIBLE_LINES)
        lcd.fillRect(0, 0, 480, VISIBLE_LINES * LINE_HEIGHT, TFT_BLACK);
        
        lcd.setTextSize(2);  // Larger font for better readability
        
        // Redraw all visible lines (always VISIBLE_LINES)
        for (int displayIndex = 0; displayIndex < VISIBLE_LINES; displayIndex++) {
            int i = startLine + displayIndex;
            int y = displayIndex * LINE_HEIGHT;  // Explicit coordinates for each line
            
            if (i < outputLineCount) {
                // Data available for this line
                uint16_t color = TFT_WHITE;
                if (outputLines[i].startsWith("OK") || outputLines[i].indexOf(": OK") >= 0) {
                    color = TFT_GREEN;
                } else if (outputLines[i].startsWith("ERR") || outputLines[i].startsWith("ERROR") || outputLines[i].indexOf("NOT FOUND") >= 0) {
                    color = TFT_RED;
                } else if (outputLines[i].startsWith("KB:") || outputLines[i].startsWith("Keyboard")) {
                    color = TFT_CYAN;
                } else if (outputLines[i].startsWith("Joy:") || outputLines[i].startsWith("Joystick")) {
                    color = TFT_YELLOW;
                } else if (outputLines[i].startsWith("Scroll") && outputLines[i].indexOf("button") >= 0) {
                    color = TFT_MAGENTA;
                } else if (outputLines[i].startsWith("Enc") || outputLines[i].startsWith("Encoder")) {
                    color = TFT_BLUE;
                }
                
                lcd.setTextColor(color, TFT_BLACK);
                lcd.setCursor(0, y);  // Explicitly set cursor position
                lcd.println(outputLines[i]);
                
                // Save line state
                displayState.lastLines[displayIndex] = outputLines[i];
            } else {
                // No data - fill with empty string (black on black = clear)
                lcd.fillRect(0, y, 480, LINE_HEIGHT, TFT_BLACK);
                displayState.lastLines[displayIndex] = "";
            }
        }
        
        displayState.initialized = true;
        displayState.lastScrollOffset = scrollOffset;
    } else {
        // Partial update - only changed lines
        lcd.setTextSize(2);
        
        for (int displayIndex = 0; displayIndex < VISIBLE_LINES; displayIndex++) {
            int i = startLine + displayIndex;
            
            if (i < outputLineCount) {
                // Data available for this line
                String currentLine = outputLines[i];
                
                // Check if line changed
                if (currentLine != displayState.lastLines[displayIndex]) {
                    // Line changed - update only this one
                    int y = displayIndex * LINE_HEIGHT;
                    
                    // Clear only this line
                    lcd.fillRect(0, y, 480, LINE_HEIGHT, TFT_BLACK);
                    
                    // Determine color
                    uint16_t color = TFT_WHITE;
                    if (currentLine.startsWith("OK") || currentLine.indexOf(": OK") >= 0) {
                        color = TFT_GREEN;
                    } else if (currentLine.startsWith("ERR") || currentLine.startsWith("ERROR") || currentLine.indexOf("NOT FOUND") >= 0) {
                        color = TFT_RED;
                    } else if (currentLine.startsWith("KB:") || currentLine.startsWith("Keyboard")) {
                        color = TFT_CYAN;
                    } else if (currentLine.startsWith("Joy:") || currentLine.startsWith("Joystick")) {
                        color = TFT_YELLOW;
                    } else if (currentLine.startsWith("Scroll") && currentLine.indexOf("button") >= 0) {
                        color = TFT_MAGENTA;
                    } else if (currentLine.startsWith("Enc") || currentLine.startsWith("Encoder")) {
                        color = TFT_BLUE;
                    }
                    
                    // Draw new line
                    lcd.setTextColor(color, TFT_BLACK);
                    lcd.setCursor(0, y);
                    lcd.println(currentLine);
                    
                    // Save new state
                    displayState.lastLines[displayIndex] = currentLine;
                }
            } else {
                // No data for this line - check if need to clear
                if (displayState.lastLines[displayIndex] != "") {
                    // Had line, now empty - clear
                    int y = displayIndex * LINE_HEIGHT;
                    lcd.fillRect(0, y, 480, LINE_HEIGHT, TFT_BLACK);
                    displayState.lastLines[displayIndex] = "";
                }
            }
        }
    }
    
    lcd.endWrite();
}

void clearScreen() {
    outputLineCount = 0;
    scrollOffset = 0;
    displayState.initialized = false;
    displayState.lastScrollOffset = -1;
    needRedraw = false;  // Reset flag
    for (int i = 0; i < MAX_OUTPUT_LINES; i++) {
        outputLines[i] = "";
    }
    for (int i = 0; i < VISIBLE_LINES; i++) {
        displayState.lastLines[i] = "";
    }
    addOutputLine("PA Hub Test v1.0", TFT_CYAN);
    addOutputLine("Cardputer-Adv", TFT_YELLOW);
    addOutputLine("External Display: ILI9488", TFT_YELLOW);
    addOutputLine("");
}

// ============================================
// Device Initialization
// ============================================

void initDevices() {
    addOutputLine("=== Initializing PA Hub and devices ===", TFT_CYAN);
    addOutputLine("Initializing PA Hub...", TFT_CYAN);
    
    // Wire1 already initialized in setup() after M5Cardputer.begin()
    
    // Detect PA Hub
    Wire1.beginTransmission(PAHUB_ADDR);
    byte error = Wire1.endTransmission();
    
    if (error == 0) {
        pahub_available = true;
        char buf[64];
        snprintf(buf, sizeof(buf), "OK PA Hub detected at 0x%02X", PAHUB_ADDR);
        addOutputLine(String(buf), TFT_GREEN);
        addOutputLine("OK PA Hub: OK", TFT_GREEN);
        
        // Deselect all channels
        Wire1.beginTransmission(PAHUB_ADDR);
        Wire1.write(0x00);
        Wire1.endTransmission();
        pahub_current_channel = 0xFF;
        delay(10);
        
        // Detect Joystick2 on Channel 0
        addOutputLine("Checking Joystick2...", TFT_CYAN);
        if (selectPaHubChannel(PAHUB_CH_JOYSTICK2)) {
            Wire1.beginTransmission(JOYSTICK2_ADDR);
            error = Wire1.endTransmission();
            if (error == 0) {
                joystick2_available = true;
                snprintf(buf, sizeof(buf), "OK Joystick2 found at 0x%02X (Channel 0)", JOYSTICK2_ADDR);
                addOutputLine(String(buf), TFT_GREEN);
                addOutputLine("OK Joystick2: OK", TFT_GREEN);
            } else {
                addOutputLine("ERR Joystick2 not found", TFT_RED);
                addOutputLine("ERR Joystick2: NOT FOUND", TFT_RED);
            }
        }
        
        // Detect Scroll A on Channel 1
        addOutputLine("Checking Scroll A...", TFT_CYAN);
        if (selectPaHubChannel(PAHUB_CH_SCROLL_A)) {
            Wire1.beginTransmission(SCROLL_ADDR);
            error = Wire1.endTransmission();
            if (error == 0) {
                scroll_a_available = true;
                snprintf(buf, sizeof(buf), "OK Scroll A found at 0x%02X (Channel 1)", SCROLL_ADDR);
                addOutputLine(String(buf), TFT_GREEN);
                addOutputLine("OK Scroll A: OK", TFT_GREEN);
            } else {
                addOutputLine("ERR Scroll A not found", TFT_RED);
                addOutputLine("ERR Scroll A: NOT FOUND", TFT_RED);
            }
        }
        
        // Detect Scroll B on Channel 2
        addOutputLine("Checking Scroll B...", TFT_CYAN);
        if (selectPaHubChannel(PAHUB_CH_SCROLL_B)) {
            Wire1.beginTransmission(SCROLL_ADDR);
            error = Wire1.endTransmission();
            if (error == 0) {
                scroll_b_available = true;
                snprintf(buf, sizeof(buf), "OK Scroll B found at 0x%02X (Channel 2)", SCROLL_ADDR);
                addOutputLine(String(buf), TFT_GREEN);
                addOutputLine("OK Scroll B: OK", TFT_GREEN);
            } else {
                addOutputLine("ERR Scroll B not found", TFT_RED);
                addOutputLine("ERR Scroll B: NOT FOUND", TFT_RED);
            }
        }
        
        // Detect CardKeyBoard on Channel 3
        addOutputLine("Checking Keyboard...", TFT_CYAN);
        if (selectPaHubChannel(PAHUB_CH_KEYBOARD)) {
            delay(50);
            Wire1.requestFrom(CARDKEYBOARD_ADDR, 1);
            delay(10);
            
            if (Wire1.available() > 0) {
                uint8_t testKey = Wire1.read();
                keyboard_available = true;
                snprintf(buf, sizeof(buf), "OK CardKeyBoard found at 0x%02X (Channel 3)", CARDKEYBOARD_ADDR);
                addOutputLine(String(buf), TFT_GREEN);
                addOutputLine("OK Keyboard: OK", TFT_GREEN);
            } else {
                addOutputLine("ERR CardKeyBoard not found", TFT_RED);
                addOutputLine("ERR Keyboard: NOT FOUND", TFT_RED);
            }
        }
        
        addOutputLine("");
        addOutputLine("Device status:", TFT_CYAN);
        snprintf(buf, sizeof(buf), "  KB:%d Joy:%d A:%d B:%d",
                 keyboard_available, joystick2_available, scroll_a_available, scroll_b_available);
        addOutputLine(String(buf), TFT_WHITE);
        // Removed duplicate Serial.printf - now only through addOutputLine
        addOutputLine("");
        addOutputLine("Polling devices...", TFT_CYAN);
        addOutputLine("Press keys/buttons to test", TFT_YELLOW);
        
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "ERR PA Hub not found at 0x%02X", PAHUB_ADDR);
        addOutputLine(String(buf), TFT_RED);
        addOutputLine("ERR PA Hub: NOT FOUND", TFT_RED);
        addOutputLine("Check connections!", TFT_RED);
    }
}

// ============================================
// Setup
// ============================================

void setup() {
    Serial.begin(115200);
    delay(2000);  // Increased delay for stabilization
    
    Serial.println("\n=== Serial test from setup() ===");
    Serial.flush();
    
    addOutputLine("========================================", TFT_CYAN);
    addOutputLine("PA Hub Test - Cardputer-Adv", TFT_CYAN);
    addOutputLine("External Display: ILI9488", TFT_CYAN);
    addOutputLine("========================================", TFT_CYAN);
    
    // Initialize external display
    addOutputLine("Initializing ILI9488 display...", TFT_CYAN);
    
    if (lcd.init()) {
        addOutputLine("OK ILI9488 initialized", TFT_GREEN);
        lcd.setRotation(3);  // 180 degrees
        lcd.setColorDepth(24);
        lcd.fillScreen(TFT_BLACK);
        lcd.setTextSize(1);
        lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
        addOutputLine("ERR ILI9488 initialization FAILED!", TFT_RED);
        return;
    }
    
    // Initialize Cardputer
    M5Cardputer.begin(false);  // disableKeyboard - built-in keyboard not needed
    delay(200);
    
    // IMPORTANT: Initialize Wire1 AFTER M5Cardputer.begin()
    // to override internal I2C bus to PORT.A
    Wire1.end();  // Close if already initialized
    delay(50);
    Wire1.begin(2, 1, 100000);  // SDA=G2, SCL=G1 for PORT.A
    Wire1.setTimeOut(100);
    delay(200);
    
    addOutputLine("Wire1 initialized for PORT.A (GPIO 2/1)", TFT_GREEN);
    
    // Disable built-in display backlight
    M5.Display.setBrightness(0);
    
    clearScreen();
    
    // Initialize devices
    initDevices();
    
    // Immediate screen redraw after initialization
    if (needRedraw) {
        needRedraw = false;
        redrawScreen();
    }
    
    addOutputLine("", TFT_WHITE);
    addOutputLine("Test started. Polling devices...", TFT_GREEN);
}

// ============================================
// Loop
// ============================================

void loop() {
    unsigned long now = millis();
    
    // Keyboard polling (every 20ms)
    if (keyboard_available && (now - last_kb_poll) >= POLL_INTERVAL_KB) {
        uint8_t key = readKeyboardKey();
        if (key != 0 && key != 0xFF) {
            char buf[64];
            snprintf(buf, sizeof(buf), "KB: 0x%02X", key);
                addOutputLine(String(buf), TFT_CYAN);  // Only addOutputLine
        }
        last_kb_poll = now;
    }
    
    // Joystick polling (every 10ms)
    if (joystick2_available && (now - last_joy_poll) >= POLL_INTERVAL_JOY) {
        uint8_t joy_x = 0, joy_y = 0, joy_button = 0;
        if (readJoystick2(&joy_x, &joy_y, &joy_button)) {
            // Output only if changed (not centered or button pressed)
            const uint8_t threshold = 40;
            const uint8_t center = 127;
            
            bool changed = (joy_y > (center + threshold)) || (joy_y < (center - threshold)) ||
                          (joy_x > (center + threshold)) || (joy_x < (center - threshold)) ||
                          joy_button;
            
            if (changed) {
                char buf[128];
                snprintf(buf, sizeof(buf), "Joy: X=%3d Y=%3d Btn=%d", joy_x, joy_y, joy_button);
                addOutputLine(String(buf), TFT_YELLOW);  // Only addOutputLine
            }
        }
        last_joy_poll = now;
    }
    
    // Scroll buttons polling (every 10ms)
    if ((now - last_btn_poll) >= POLL_INTERVAL_BTN) {
        if (scroll_a_available) {
            updateButtonState(&scroll_a_state, PAHUB_CH_SCROLL_A);
            // Edge detection: transition from false to true (press)
            if (scroll_a_state.debounced_state && !scroll_a_state.last_debounced_state) {
                addOutputLine("Scroll A button: PRESSED", TFT_MAGENTA);  // Only addOutputLine
            }
            // Edge detection: transition from true to false (release)
            else if (!scroll_a_state.debounced_state && scroll_a_state.last_debounced_state) {
                addOutputLine("Scroll A button: RELEASED", TFT_MAGENTA);  // Only addOutputLine
            }
        }
        
        if (scroll_b_available) {
            updateButtonState(&scroll_b_state, PAHUB_CH_SCROLL_B);
            // Edge detection: transition from false to true (press)
            if (scroll_b_state.debounced_state && !scroll_b_state.last_debounced_state) {
                addOutputLine("Scroll B button: PRESSED", TFT_MAGENTA);  // Only addOutputLine
            }
            // Edge detection: transition from true to false (release)
            else if (!scroll_b_state.debounced_state && scroll_b_state.last_debounced_state) {
                addOutputLine("Scroll B button: RELEASED", TFT_MAGENTA);  // Only addOutputLine
            }
        }
        last_btn_poll = now;
    }
    
    // Scroll encoder polling (every 50ms)
    if ((now - last_enc_poll) >= POLL_INTERVAL_ENC) {
        if (scroll_a_available) {
            int16_t increment = 0;
            if (readScrollEncoder(PAHUB_CH_SCROLL_A, &increment)) {
                if (increment != 0) {
                    scroll_a_encoder_value += increment;
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Enc A: %+d (Total: %d)", increment, scroll_a_encoder_value);
                    addOutputLine(String(buf), TFT_BLUE);  // Only addOutputLine
                }
            }
        }
        
        if (scroll_b_available) {
            int16_t increment = 0;
            if (readScrollEncoder(PAHUB_CH_SCROLL_B, &increment)) {
                if (increment != 0) {
                    scroll_b_encoder_value += increment;
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Enc B: %+d (Total: %d)", increment, scroll_b_encoder_value);
                    addOutputLine(String(buf), TFT_BLUE);  // Only addOutputLine
                }
            }
        }
        last_enc_poll = now;
    }
    
    // Batching: screen redraw once per cycle (if needed)
    if (needRedraw) {
        needRedraw = false;
        redrawScreen();
    }
    
    delay(1);
}

