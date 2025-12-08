/*
 * PA Hub Test for Cardputer V1.1
 * 
 * Test for PA Hub with joystick, scroll encoders, and keyboard
 * Uses built-in display (240x135) and Serial Monitor
 * 
 * CONNECTED DEVICES:
 * - PA Hub (PCA9548A) - address 0x70 on PORT.A (GPIO 2/1)
 *   - Channel 0: Joystick2 (address 0x63)
 *   - Channel 1: Scroll Button A (address 0x40)
 *   - Channel 2: Scroll Button B (address 0x40)
 *   - Channel 3: CardKeyBoard (address 0x5F)
 * 
 * PA Hub Connection:
 * - Connect to PORT.A (GPIO 2/1)
 * - VCC -> 5V or 3.3V
 * - GND -> GND
 * 
 * IMPORTANT FOR V1.1:
 * - Uses Wire (shared with keyboard) instead of Wire1
 * - Keyboard is always enabled (M5Cardputer.begin(true))
 * - PORT.A uses GPIO 2/1 via Wire (same controller as keyboard)
 * - Uses M5.Display (not M5Cardputer.Display) for built-in display
 */

#include <M5Cardputer.h>
#include <Wire.h>

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

// Last event for display (compact display on small screen)
String lastEvent = "";
unsigned long lastEventTime = 0;

// ============================================
// PA Hub Functions
// ============================================

bool selectPaHubChannel(uint8_t channel) {
    if (channel > 5) return false;
    if (pahub_current_channel == channel) return true;
    
    Wire.beginTransmission(PAHUB_ADDR);
    Wire.write(1 << channel);
    byte error = Wire.endTransmission();
    
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
    Wire.beginTransmission(JOYSTICK2_ADDR);
    Wire.write(JOYSTICK2_REG_ADC_X_8);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
        *x = Wire.read();
    } else {
        return false;
    }
    
    // Read Y
    Wire.beginTransmission(JOYSTICK2_ADDR);
    Wire.write(JOYSTICK2_REG_ADC_Y_8);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
        *y = Wire.read();
    } else {
        return false;
    }
    
    // Read button
    Wire.beginTransmission(JOYSTICK2_ADDR);
    Wire.write(JOYSTICK2_REG_BUTTON);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
        uint8_t btn = Wire.read();
        *button = (btn == 0) ? 1 : 0;  // Invert
    } else {
        return false;
    }
    
    return true;
}

bool readScrollButton(uint8_t channel, bool* pressed) {
    if (!pahub_available) return false;
    
    if (!selectPaHubChannel(channel)) return false;
    
    Wire.beginTransmission(SCROLL_ADDR);
    Wire.write(SCROLL_BUTTON_REG);
    if (Wire.endTransmission(false) != 0) return false;
    
    delayMicroseconds(500);  // Delay for STM32F030
    
    Wire.requestFrom(SCROLL_ADDR, 1, true);
    delayMicroseconds(300);
    
    if (Wire.available() >= 1) {
        uint8_t btn = Wire.read();
        *pressed = (btn == 0);
        return true;
    }
    
    return false;
}

// Read incremental encoder value (register 0x50)
bool readScrollEncoder(uint8_t channel, int16_t* increment) {
    if (!pahub_available) return false;
    
    if (!selectPaHubChannel(channel)) return false;
    
    Wire.beginTransmission(SCROLL_ADDR);
    Wire.write(SCROLL_INC_ENCODER_REG);
    if (Wire.endTransmission(false) != 0) return false;
    
    delayMicroseconds(500);  // Delay for STM32F030
    
    Wire.requestFrom(SCROLL_ADDR, 2, true);  // 16-bit value
    delayMicroseconds(300);
    
    if (Wire.available() >= 2) {
        uint8_t low = Wire.read();
        uint8_t high = Wire.read();
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
    
    Wire.requestFrom(CARDKEYBOARD_ADDR, 1);
    delayMicroseconds(500);  // Delay for Slave response
    
    if (Wire.available() > 0) {
        uint8_t key = Wire.read();
        return key;
    }
    
    return 0;
}

// ============================================
// Display Functions (Built-in Display)
// ============================================

void updateDisplay() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(1);
    
    // Header
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.println("PA Hub Test");
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    
    // Device status
    M5.Display.printf("KB:%d Joy:%d\n", keyboard_available ? 1 : 0, joystick2_available ? 1 : 0);
    M5.Display.printf("A:%d B:%d\n", scroll_a_available ? 1 : 0, scroll_b_available ? 1 : 0);
    M5.Display.println("");
    
    // Encoder values
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.printf("Enc A: %d\n", scroll_a_encoder_value);
    M5.Display.printf("Enc B: %d\n", scroll_b_encoder_value);
    M5.Display.println("");
    
    // Last event
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    if (lastEvent.length() > 0) {
        // Truncate if too long for display
        String displayEvent = lastEvent;
        if (displayEvent.length() > 20) {
            displayEvent = displayEvent.substring(0, 17) + "...";
        }
        M5.Display.println(displayEvent);
    } else {
        M5.Display.println("Ready...");
    }
}

void logEvent(const String& event) {
    // Serial Monitor logging
    Serial.print("[APP] ");
    Serial.println(event.c_str());
    Serial.flush();
    
    // Update last event for display
    lastEvent = event;
    lastEventTime = millis();
    
    // Update display
    updateDisplay();
}

// ============================================
// Device Initialization
// ============================================

void initDevices() {
    Serial.println("=== Initializing PA Hub and devices ===");
    Serial.println("Initializing PA Hub...");
    
    // Wire already initialized in setup() after M5Cardputer.begin()
    
    // Detect PA Hub
    Wire.beginTransmission(PAHUB_ADDR);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        pahub_available = true;
        char buf[64];
        snprintf(buf, sizeof(buf), "OK PA Hub detected at 0x%02X", PAHUB_ADDR);
        Serial.print("[APP] ");
        Serial.println(buf);
        
        // Deselect all channels
        Wire.beginTransmission(PAHUB_ADDR);
        Wire.write(0x00);
        Wire.endTransmission();
        pahub_current_channel = 0xFF;
        delay(10);
        
        // Detect Joystick2 on Channel 0
        Serial.println("Checking Joystick2...");
        if (selectPaHubChannel(PAHUB_CH_JOYSTICK2)) {
            Wire.beginTransmission(JOYSTICK2_ADDR);
            error = Wire.endTransmission();
            if (error == 0) {
                joystick2_available = true;
                snprintf(buf, sizeof(buf), "OK Joystick2 found at 0x%02X (Channel 0)", JOYSTICK2_ADDR);
                Serial.print("[APP] ");
                Serial.println(buf);
            } else {
                Serial.print("[APP] ERR Joystick2 not found\n");
            }
        }
        
        // Detect Scroll A on Channel 1
        Serial.println("Checking Scroll A...");
        if (selectPaHubChannel(PAHUB_CH_SCROLL_A)) {
            Wire.beginTransmission(SCROLL_ADDR);
            error = Wire.endTransmission();
            if (error == 0) {
                scroll_a_available = true;
                snprintf(buf, sizeof(buf), "OK Scroll A found at 0x%02X (Channel 1)", SCROLL_ADDR);
                Serial.print("[APP] ");
                Serial.println(buf);
            } else {
                Serial.print("[APP] ERR Scroll A not found\n");
            }
        }
        
        // Detect Scroll B on Channel 2
        Serial.println("Checking Scroll B...");
        if (selectPaHubChannel(PAHUB_CH_SCROLL_B)) {
            Wire.beginTransmission(SCROLL_ADDR);
            error = Wire.endTransmission();
            if (error == 0) {
                scroll_b_available = true;
                snprintf(buf, sizeof(buf), "OK Scroll B found at 0x%02X (Channel 2)", SCROLL_ADDR);
                Serial.print("[APP] ");
                Serial.println(buf);
            } else {
                Serial.print("[APP] ERR Scroll B not found\n");
            }
        }
        
        // Detect CardKeyBoard on Channel 3
        Serial.println("Checking Keyboard...");
        if (selectPaHubChannel(PAHUB_CH_KEYBOARD)) {
            delay(50);
            Wire.requestFrom(CARDKEYBOARD_ADDR, 1);
            delay(10);
            
            if (Wire.available() > 0) {
                uint8_t testKey = Wire.read();
                keyboard_available = true;
                snprintf(buf, sizeof(buf), "OK CardKeyBoard found at 0x%02X (Channel 3)", CARDKEYBOARD_ADDR);
                Serial.print("[APP] ");
                Serial.println(buf);
            } else {
                Serial.print("[APP] ERR CardKeyBoard not found\n");
            }
        }
        
        Serial.print("[APP] Device status: KB:");
        Serial.print(keyboard_available ? 1 : 0);
        Serial.print(" Joy:");
        Serial.print(joystick2_available ? 1 : 0);
        Serial.print(" A:");
        Serial.print(scroll_a_available ? 1 : 0);
        Serial.print(" B:");
        Serial.println(scroll_b_available ? 1 : 0);
        Serial.println("[APP] Polling devices...");
        Serial.println("[APP] Press keys/buttons to test");
        
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "ERR PA Hub not found at 0x%02X", PAHUB_ADDR);
        Serial.print("[APP] ");
        Serial.println(buf);
        Serial.println("[APP] ERR PA Hub: NOT FOUND");
        Serial.println("[APP] Check connections!");
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
    
    Serial.println("========================================");
    Serial.println("PA Hub Test - Cardputer V1.1");
    Serial.println("Built-in Display: 240x135");
    Serial.println("========================================");
    
    // Initialize Cardputer V1.1
    // IMPORTANT: Keyboard is always enabled on V1.1
    M5Cardputer.begin(true);  // enableKeyboard - required for V1.1
    delay(200);
    
    // Initialize built-in display
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.println("PA Hub Test");
    M5.Display.println("Initializing...");
    
    // IMPORTANT FOR V1.1: Reinitialize Wire for PORT.A
    // Wire is shared with keyboard, but we need to reconfigure it for PORT.A (GPIO 2/1)
    // On V1.1, keyboard uses GPIO 8/9, but PORT.A uses GPIO 2/1
    // We need to reconfigure Wire to use PORT.A pins
    Wire.end();  // Close current I2C configuration
    delay(50);
    
    // Configure pull-up resistors for PORT.A
    pinMode(2, INPUT_PULLUP);  // SDA (G2)
    pinMode(1, INPUT_PULLUP);  // SCL (G1)
    delay(100);  // Give time for pull-up stabilization
    
    // Reinitialize Wire for PORT.A (GPIO 2/1)
    Wire.begin(2, 1, 100000);  // SDA=G2, SCL=G1 for PORT.A, 100 kHz
    Wire.setTimeOut(100);
    delay(200);
    
    Serial.print("[APP] Wire initialized for PORT.A (GPIO 2/1)\n");
    Serial.print("[APP] NOTE: Wire shared with keyboard controller\n");
    
    // Initialize devices
    initDevices();
    
    // Initial display update
    updateDisplay();
    
    Serial.println("[APP] Test started. Polling devices...");
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
            logEvent(String(buf));
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
                logEvent(String(buf));
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
                logEvent("Scroll A: PRESSED");
            }
            // Edge detection: transition from true to false (release)
            else if (!scroll_a_state.debounced_state && scroll_a_state.last_debounced_state) {
                logEvent("Scroll A: RELEASED");
            }
        }
        
        if (scroll_b_available) {
            updateButtonState(&scroll_b_state, PAHUB_CH_SCROLL_B);
            // Edge detection: transition from false to true (press)
            if (scroll_b_state.debounced_state && !scroll_b_state.last_debounced_state) {
                logEvent("Scroll B: PRESSED");
            }
            // Edge detection: transition from true to false (release)
            else if (!scroll_b_state.debounced_state && scroll_b_state.last_debounced_state) {
                logEvent("Scroll B: RELEASED");
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
                    snprintf(buf, sizeof(buf), "Enc A: %+d (%d)", increment, scroll_a_encoder_value);
                    logEvent(String(buf));
                }
            }
        }
        
        if (scroll_b_available) {
            int16_t increment = 0;
            if (readScrollEncoder(PAHUB_CH_SCROLL_B, &increment)) {
                if (increment != 0) {
                    scroll_b_encoder_value += increment;
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Enc B: %+d (%d)", increment, scroll_b_encoder_value);
                    logEvent(String(buf));
                }
            }
        }
        last_enc_poll = now;
    }
    
    delay(1);
}
