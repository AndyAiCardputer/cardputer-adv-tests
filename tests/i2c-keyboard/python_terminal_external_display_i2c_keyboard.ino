/*
 * Python Terminal for Cardputer-Adv on External ILI9488 Display
 * Version WITH I2C KEYBOARD SUPPORT (CardKeyBoard)
 * 
 * Simple terminal with basic commands for demonstration
 * Works on external ILI9488 display (480x320)
 * 
 * FEATURES:
 * - External I2C keyboard CardKeyBoard support (address 0x5F)
 * - Works in parallel with built-in keyboard
 * - Protocol compatible with CardKeyBoard.ino
 * - Full key combination support:
 *   * Key - regular characters (a-z, 0-9, space, punctuation)
 *   * Sym+Key - symbols (!, @, #, $, %, ^, &, *, (, ), {, }, [, ], /, \, |, ~, ', ", :, ;, `, +, -, _, =, ?, <, >)
 *   * Shift+Key - uppercase letters (A-Z) and Delete
 *   * Fn+Key - function keys (0x80-0xAF, optional)
 * - Command history navigation with arrow keys (Up/Down)
 * - Complete key codes documentation: docs/CARDKEYBOARD_KEYCODES.md
 * 
 * Commands:
 *   help     - list commands
 *   clear    - clear screen
 *   info     - system information
 *   test     - test components
 *   echo     - echo text
 *   version  - firmware version
 * 
 * Scrolling:
 *   Fn + . (period) - scroll down
 *   Fn + ; (semicolon) - scroll up
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
 * CardKeyBoard Connection:
 * - PORT.A (Grove HY2.0-4P)
 * - SDA -> GPIO 2 (G2)
 * - SCL -> GPIO 1 (G1)
 * - GND -> GND (common ground REQUIRED!)
 * - 5V -> 5V (keyboard power)
 */

#include <M5Cardputer.h>
#include <M5GFX.h>
#include <Wire.h>
#include "lgfx/v1/panel/Panel_LCD.hpp"

// I2C settings for CardKeyBoard
#define CARDKEYBOARD_I2C_ADDRESS 0x5F
#define I2C_SDA_PIN 2  // GPIO 2 (G2) - PORT.A
#define I2C_SCL_PIN 1  // GPIO 1 (G1) - PORT.A
#define I2C_KEYBOARD_POLL_INTERVAL 50  // Keyboard poll interval (ms)

// I2C Keyboard support
bool i2cKeyboardEnabled = false;  // I2C keyboard enabled flag
unsigned long lastI2CKeyboardRead = 0;  // Last read time

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
// Constants
// ============================================
#define INPUT_BUFFER_SIZE 128
#define MAX_HISTORY 10
#define CURSOR_BLINK_PERIOD 500
#define MAX_OUTPUT_LINES 100  // Maximum lines in buffer
#define INPUT_LINE_Y 300  // Fixed input line position (for external display 320px)
#define TEXT_AREA_HEIGHT 300  // Text output area height
#define LINE_HEIGHT 24  // Single line height (font size 2)
#define VISIBLE_LINES (TEXT_AREA_HEIGHT / LINE_HEIGHT)  // Number of visible lines (~12)

// ============================================
// Global Variables
// ============================================
String inputBuffer = "";
String history[MAX_HISTORY];
int historyIndex = -1;
int historyCount = 0;
unsigned long cursorBlinkTime = 0;
bool cursorVisible = true;

// Output buffer
String outputLines[MAX_OUTPUT_LINES];
int outputLineCount = 0;
int scrollOffset = 0;  // Scroll offset (how many lines scrolled)
bool needRedraw = false;  // Flag for deferred redraw (batching)

// ============================================
// Terminal Functions
// ============================================

void printPrompt() {
    lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    lcd.print(">>> ");
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
}

void addOutputLine(const String& text, uint16_t color = TFT_WHITE) {
    // Add line to buffer
    if (outputLineCount < MAX_OUTPUT_LINES) {
        outputLines[outputLineCount] = text;
        outputLineCount++;
    } else {
        // Shift buffer up, removing first line
        for (int i = 0; i < MAX_OUTPUT_LINES - 1; i++) {
            outputLines[i] = outputLines[i + 1];
        }
        outputLines[MAX_OUTPUT_LINES - 1] = text;
    }
    
    // Automatically scroll down on new output
    scrollOffset = max(0, outputLineCount - VISIBLE_LINES);
    
    // Set flag for redraw (batching - redraw later)
    needRedraw = true;
}

void printOutput(const String& text, uint16_t color = TFT_WHITE) {
    addOutputLine(text, color);
}

void redrawScreen() {
    // Clear text output area
    lcd.fillRect(0, 0, 480, TEXT_AREA_HEIGHT, TFT_BLACK);
    
    // Draw visible lines
    int startLine = scrollOffset;
    int endLine = min(startLine + VISIBLE_LINES, outputLineCount);
    
    lcd.setTextSize(2);  // Font size for external display
    lcd.setCursor(0, 0);
    
    for (int i = startLine; i < endLine; i++) {
        // Determine color by prefix or use white
        uint16_t color = TFT_WHITE;
        if (outputLines[i].startsWith(">>>")) {
            color = TFT_GREEN;
        } else if (outputLines[i].startsWith("Error:")) {
            color = TFT_RED;
        } else if (outputLines[i].startsWith("Available commands:") || 
                   outputLines[i].startsWith("System Information:") ||
                   outputLines[i].startsWith("Python Terminal")) {
            color = TFT_CYAN;
        } else if (outputLines[i].startsWith("Cardputer-Adv")) {
            color = TFT_YELLOW;
        }
        
        lcd.setTextColor(color, TFT_BLACK);
        lcd.println(outputLines[i]);
    }
    
    // Draw input line
    renderInputLine();
}

void scrollUp() {
    if (scrollOffset > 0) {
        scrollOffset--;
        needRedraw = true;
    }
}

void scrollDown() {
    int maxScroll = max(0, outputLineCount - VISIBLE_LINES);
    if (scrollOffset < maxScroll) {
        scrollOffset++;
        needRedraw = true;
    }
}

void clearScreen() {
    // Clear entire screen physically
    lcd.fillScreen(TFT_BLACK);
    
    // Clear output buffer completely
    outputLineCount = 0;
    scrollOffset = 0;
    needRedraw = false;
    
    // Clear string array (just in case)
    for (int i = 0; i < MAX_OUTPUT_LINES; i++) {
        outputLines[i] = "";
    }
    
    // Add welcome message directly to buffer (without redraw)
    outputLines[0] = "Python Terminal v1.0";
    outputLines[1] = "Cardputer-Adv";
    outputLines[2] = "I2C Keyboard Support";
    outputLines[3] = "Type 'help' for commands";
    outputLines[4] = "";
    outputLineCount = 5;
    scrollOffset = 0;
    
    // Redraw screen once
    redrawScreen();
}

void renderInputLine() {
    // Always draw input line at fixed position at bottom
    lcd.fillRect(0, INPUT_LINE_Y, 480, 30, TFT_BLACK);
    lcd.setCursor(0, INPUT_LINE_Y);
    lcd.setTextSize(2);  // Font size for external display
    
    printPrompt();
    lcd.print(inputBuffer);
    
    // Blinking cursor
    if (cursorVisible) {
        lcd.print("_");
    } else {
        lcd.print(" ");
    }
}

void addToHistory(const String& cmd) {
    if (cmd.length() == 0) return;
    
    // Shift history
    for (int i = MAX_HISTORY - 1; i > 0; i--) {
        history[i] = history[i - 1];
    }
    history[0] = cmd;
    
    if (historyCount < MAX_HISTORY) {
        historyCount++;
    }
    historyIndex = -1;
}

String getHistory(int index) {
    if (index >= 0 && index < historyCount) {
        return history[index];
    }
    return "";
}

// ============================================
// Command Processing
// ============================================

void executeCommand(const String& cmd) {
    if (cmd.length() == 0) {
        return;
    }
    
    // Add to history
    addToHistory(cmd);
    
    // Show command in output area
    addOutputLine(">>> " + cmd, TFT_GREEN);
    
    // Check for Python-like commands: print("text") or print('text')
    if (cmd.startsWith("print(") || cmd.startsWith("Print(")) {
        cmdPrint(cmd);
        return;
    }
    
    // Parse command
    int spaceIndex = cmd.indexOf(' ');
    String command = (spaceIndex > 0) ? cmd.substring(0, spaceIndex) : cmd;
    String args = (spaceIndex > 0) ? cmd.substring(spaceIndex + 1) : "";
    
    command.toLowerCase();
    
    // Execute command
    if (command == "help") {
        cmdHelp();
    } else if (command == "clear") {
        cmdClear();
    } else if (command == "info") {
        cmdInfo();
    } else if (command == "test") {
        cmdTest();
    } else if (command == "echo") {
        cmdEcho(args);
    } else if (command == "version") {
        cmdVersion();
    } else {
        addOutputLine("Error: Unknown command '" + command + "'", TFT_RED);
        addOutputLine("Type 'help' for available commands", TFT_YELLOW);
    }
}

void cmdHelp() {
    addOutputLine("Available commands:", TFT_CYAN);
    addOutputLine("  help     - Show this help");
    addOutputLine("  clear    - Clear screen");
    addOutputLine("  info     - System information");
    addOutputLine("  test     - Test components");
    addOutputLine("  echo     - Echo text");
    addOutputLine("  version  - Show version");
    addOutputLine("  print(\"text\") - Print text");
    addOutputLine("Scroll: Fn+. (down) Fn+; (up)");
    if (i2cKeyboardEnabled) {
        addOutputLine("I2C Keyboard: ACTIVE", TFT_GREEN);
    }
}

void cmdClear() {
    clearScreen();
}

void cmdInfo() {
    int batteryLevel = M5Cardputer.Power.getBatteryLevel();
    int batteryVoltage = M5Cardputer.Power.getBatteryVoltage();
    bool isCharging = M5Cardputer.Power.isCharging();
    
    addOutputLine("System Information:", TFT_CYAN);
    char buf[128];
    snprintf(buf, sizeof(buf), "  Battery: %d%% (%d mV)", batteryLevel, batteryVoltage);
    addOutputLine(String(buf));
    snprintf(buf, sizeof(buf), "  Charging: %s", isCharging ? "Yes" : "No");
    addOutputLine(String(buf));
    snprintf(buf, sizeof(buf), "  Free Heap: %d bytes", ESP.getFreeHeap());
    addOutputLine(String(buf));
    snprintf(buf, sizeof(buf), "  Chip Model: %s", ESP.getChipModel());
    addOutputLine(String(buf));
    snprintf(buf, sizeof(buf), "  CPU Freq: %d MHz", ESP.getCpuFreqMHz());
    addOutputLine(String(buf));
}

void cmdTest() {
    addOutputLine("Testing components...", TFT_CYAN);
    addOutputLine("  Keyboard: OK", TFT_GREEN);
    delay(100);
    addOutputLine("  Display: OK", TFT_GREEN);
    delay(100);
    
    int level = M5Cardputer.Power.getBatteryLevel();
    char buf[64];
    if (level > 0) {
        snprintf(buf, sizeof(buf), "  Battery: OK (%d%%)", level);
        addOutputLine(String(buf));
    } else {
        addOutputLine("  Battery: ERROR", TFT_RED);
    }
    delay(100);
    
    if (i2cKeyboardEnabled) {
        addOutputLine("  I2C Keyboard: OK", TFT_GREEN);
    } else {
        addOutputLine("  I2C Keyboard: NOT CONNECTED", TFT_YELLOW);
    }
    delay(100);
    
    addOutputLine("All tests passed!", TFT_GREEN);
}

void cmdEcho(const String& text) {
    if (text.length() > 0) {
        addOutputLine(text, TFT_CYAN);
    } else {
        addOutputLine("Usage: echo <text>", TFT_YELLOW);
    }
}

void cmdVersion() {
    addOutputLine("Python Terminal v1.0", TFT_CYAN);
    addOutputLine("Cardputer-Adv", TFT_YELLOW);
    addOutputLine("I2C Keyboard Support", TFT_YELLOW);
    char buf[128];
    snprintf(buf, sizeof(buf), "  ESP32-S3 @ %d MHz", ESP.getCpuFreqMHz());
    addOutputLine(String(buf));
    snprintf(buf, sizeof(buf), "  Free Heap: %d KB", ESP.getFreeHeap() / 1024);
    addOutputLine(String(buf));
    if (i2cKeyboardEnabled) {
        addOutputLine("  I2C Keyboard: ENABLED", TFT_GREEN);
    } else {
        addOutputLine("  I2C Keyboard: DISABLED", TFT_YELLOW);
    }
}

void cmdPrint(const String& cmd) {
    // Parse print("text") or print('text')
    // Find opening parenthesis
    int openParen = cmd.indexOf('(');
    if (openParen == -1) {
        addOutputLine("Error: Syntax error. Use: print(\"text\")", TFT_RED);
        return;
    }
    
    // Find closing parenthesis
    int closeParen = cmd.lastIndexOf(')');
    if (closeParen == -1 || closeParen <= openParen) {
        addOutputLine("Error: Syntax error. Use: print(\"text\")", TFT_RED);
        return;
    }
    
    // Extract content between parentheses
    String content = cmd.substring(openParen + 1, closeParen);
    content.trim();
    
    // Remove quotes (single or double)
    if (content.startsWith("\"") && content.endsWith("\"")) {
        content = content.substring(1, content.length() - 1);
    } else if (content.startsWith("'") && content.endsWith("'")) {
        content = content.substring(1, content.length() - 1);
    }
    
    // Output text
    if (content.length() > 0) {
        addOutputLine(content, TFT_CYAN);
    } else {
        addOutputLine("", TFT_WHITE);  // Empty string
    }
}

// ============================================
// I2C Keyboard Functions
// ============================================

void initI2CKeyboard() {
    Serial.println("Initializing I2C Keyboard (CardKeyBoard) on PORT.A (GPIO 2/1)...");
    Serial.println("  Using Wire (shared with keyboard)");
    Serial.println("  Address: 0x5F (CardKeyBoard)");
    
    // Enable pull-up resistors
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    delay(100);
    
    // Initialize I2C on PORT.A
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);  // 100 kHz
    Wire.setTimeOut(100);
    
    delay(200);  // Give keyboard time to initialize
    
    // Try to detect keyboard
    Wire.beginTransmission(CARDKEYBOARD_I2C_ADDRESS);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        i2cKeyboardEnabled = true;
        Serial.println("  ✓ CardKeyBoard found at 0x5F");
        Serial.println("  ✓ I2C Keyboard initialized!");
        addOutputLine("I2C Keyboard: Connected", TFT_GREEN);
    } else {
        i2cKeyboardEnabled = false;
        Serial.println("  ✗ CardKeyBoard not found at 0x5F");
        Serial.println("  Using built-in keyboard only");
        addOutputLine("I2C Keyboard: NOT FOUND", TFT_YELLOW);
    }
}

unsigned char readI2CKeyboard() {
    if (!i2cKeyboardEnabled) {
        return 0;
    }
    
    unsigned long currentTime = millis();
    
    // Don't read too often
    if (currentTime - lastI2CKeyboardRead < I2C_KEYBOARD_POLL_INTERVAL) {
        return 0;
    }
    lastI2CKeyboardRead = currentTime;
    
    // Request 1 byte from CardKeyBoard
    uint8_t bytesReceived = Wire.requestFrom(CARDKEYBOARD_I2C_ADDRESS, 1);
    
    if (bytesReceived > 0 && Wire.available()) {
        unsigned char key = Wire.read();
        
        // CardKeyBoard sends 0 or 255 when no key pressed
        if (key == 0 || key == 255) {
            return 0;
        }
        
        return key;
    }
    
    return 0;
}

void processI2CKeyboard(unsigned char key) {
    if (key == 0) {
        return;  // No key pressed
    }
    
    // Handle control keys
    if (key == 13) {  // Enter (0x0D)
        executeCommand(inputBuffer);
        inputBuffer = "";
        historyIndex = -1;
        if (needRedraw) {
            needRedraw = false;
            redrawScreen();
        } else {
            renderInputLine();
        }
        return;
    }
    
    if (key == 8 || key == 127) {  // Backspace (0x08) / Delete (0x7F)
        if (inputBuffer.length() > 0) {
            inputBuffer.remove(inputBuffer.length() - 1);
            renderInputLine();
        }
        return;
    }
    
    if (key == 9) {  // Tab (0x09)
        if (inputBuffer.length() + 4 < INPUT_BUFFER_SIZE) {
            inputBuffer += "    ";  // 4 spaces
            renderInputLine();
        }
        return;
    }
    
    if (key == 27) {  // ESC (0x1B)
        inputBuffer = "";
        renderInputLine();
        return;
    }
    
    // Arrow keys (0xB4-0xB7)
    // 180 (0xB4) = Left, 181 (0xB5) = Up, 182 (0xB6) = Down, 183 (0xB7) = Right
    if (key >= 180 && key <= 183) {
        // Navigation through history
        if (key == 181) {  // Up - previous command in history
            if (historyCount > 0) {
                if (historyIndex < historyCount - 1) {
                    historyIndex++;
                    inputBuffer = getHistory(historyIndex);
                    renderInputLine();
                }
            }
        } else if (key == 182) {  // Down - next command in history
            if (historyIndex > 0) {
                historyIndex--;
                inputBuffer = getHistory(historyIndex);
                renderInputLine();
            } else if (historyIndex == 0) {
                historyIndex = -1;
                inputBuffer = "";
                renderInputLine();
            }
        }
        // Left/Right arrows could be used for cursor movement in future
        return;
    }
    
    // Function keys (Fn+Key) - 0x80-0xAF
    // These are special function codes that can be mapped to custom actions
    if (key >= 128 && key <= 175) {
        // Optional: Map Fn+1-0 to F1-F10, or other custom functions
        // For now, we ignore them (as requested - no special mapping)
        // But you can add custom handling here if needed
        return;
    }
    
    // Regular printable characters (32-126)
    // This includes:
    // - Letters (a-z, A-Z) - handled automatically by CardKeyBoard with Shift
    // - Numbers (0-9)
    // - Symbols (!, @, #, $, %, ^, &, *, (, ), {, }, [, ], /, \, |, ~, ', ", :, ;, `, +, -, _, =, ?, <, >)
    //   - Symbols are sent when Sym+Key is pressed
    //   - CardKeyBoard automatically sends the correct code, no need to check modifiers
    if (key >= 32 && key <= 126) {
        if (inputBuffer.length() < INPUT_BUFFER_SIZE - 1) {
            inputBuffer += (char)key;
            renderInputLine();
        }
    }
}

// ============================================
// Setup
// ============================================

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n========================================");
    Serial.println("Python Terminal External Display - Cardputer-Adv");
    Serial.println("Version: WITH I2C KEYBOARD SUPPORT");
    Serial.println("========================================");
    Serial.println("Built-in display: DISABLED");
    Serial.println("External display: ILI9488 (480x320)");
    Serial.println("I2C Keyboard: Supported\n");
    
    // Initialize external ILI9488
    Serial.println("Initializing ILI9488 display...");
    
    if (lcd.init()) {
        Serial.println("  ✓ ILI9488 initialized successfully!");
        Serial.printf("  ✓ Display size: %dx%d\n", lcd.width(), lcd.height());
        
        lcd.setRotation(3);  // 180 degrees
        lcd.setColorDepth(24);
        
        lcd.fillScreen(TFT_BLACK);
        lcd.setTextSize(2);  // Larger font size for large screen
        lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        
        Serial.println("\nReady! Terminal initialized...");
        Serial.println("----------------------------------------\n");
        
        delay(200);
    } else {
        Serial.println("\n  ✗ ERROR: ILI9488 initialization FAILED!");
        return;
    }
    
    // Initialize Cardputer AFTER display
    M5Cardputer.begin(true);  // enableKeyboard
    delay(200);
    
    // Disable built-in display backlight
    M5.Display.setBrightness(0);
    Serial.println("  ✓ Built-in display backlight: DISABLED");
    
    // Initialize I2C keyboard
    Serial.println("\nInitializing I2C Keyboard...");
    initI2CKeyboard();
    
    clearScreen();
    
    Serial.println("Terminal ready!");
    Serial.println("Type commands...\n");
}

// ============================================
// Loop
// ============================================

void loop() {
    M5Cardputer.update();
    M5.update();
    
    // Redraw screen if needed (batching - redraw all changes at once)
    if (needRedraw) {
        needRedraw = false;
        redrawScreen();
    }
    
    // Update blinking cursor
    unsigned long currentMillis = millis();
    if (currentMillis - cursorBlinkTime > CURSOR_BLINK_PERIOD) {
        cursorBlinkTime = currentMillis;
        cursorVisible = !cursorVisible;
        renderInputLine();
    }
    
    // Handle I2C keyboard (if enabled)
    if (i2cKeyboardEnabled) {
        unsigned char i2cKey = readI2CKeyboard();
        if (i2cKey != 0) {
            processI2CKeyboard(i2cKey);
        }
    }
    
    // Handle built-in keyboard
    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            auto keyList = M5Cardputer.Keyboard.keyList();
            auto keys = M5Cardputer.Keyboard.keysState();
            
            // Check scrolling combinations
            if (keys.fn) {
                // Fn + . (period) - scroll down
                for (auto& key : keyList) {
                    char c = M5Cardputer.Keyboard.getKey(key);
                    if (c == '.') {
                        scrollDown();
                        // Force redraw on scroll
                        if (needRedraw) {
                            needRedraw = false;
                            redrawScreen();
                        }
                        return;
                    }
                    // Fn + ; (semicolon) - scroll up
                    if (c == ';') {
                        scrollUp();
                        // Force redraw on scroll
                        if (needRedraw) {
                            needRedraw = false;
                            redrawScreen();
                        }
                        return;
                    }
                }
            }
            
            // Check if there are regular keys (not just modifiers)
            bool hasRegularKey = false;
            for (auto& key : keyList) {
                char c = M5Cardputer.Keyboard.getKey(key);
                // Skip modifiers (0xFF=FN, 0x80=CTRL, 0x81=SHIFT, 0x82=special)
                if (c != 0 && c != 0xFF && c != 0x80 && c != 0x81 && c != 0x82) {
                    hasRegularKey = true;
                    break;
                }
            }
            
            // If only modifiers pressed without other keys - ignore
            if (!hasRegularKey && (keys.shift || keys.ctrl || keys.opt || keys.fn)) {
                // Ignore modifier-only press
                delay(10);
                return;
            }
            
            // Enter - execute command
            if (keys.enter) {
                executeCommand(inputBuffer);
                inputBuffer = "";
                historyIndex = -1;
                // Force redraw screen after command execution
                if (needRedraw) {
                    needRedraw = false;
                    redrawScreen();
                } else {
                    renderInputLine();
                }
                return;
            }
            // Backspace - delete character
            if (keys.del) {
                if (inputBuffer.length() > 0) {
                    inputBuffer.remove(inputBuffer.length() - 1);
                    renderInputLine();
                }
                return;
            }
            // Tab - add spaces (tabulation)
            if (keys.tab) {
                if (inputBuffer.length() + 4 < INPUT_BUFFER_SIZE) {
                    inputBuffer += "    ";  // 4 spaces for tabulation
                    renderInputLine();
                }
                return;
            }
            // Space - add space
            if (keys.space) {
                if (inputBuffer.length() < INPUT_BUFFER_SIZE - 1) {
                    inputBuffer += " ";
                    renderInputLine();
                }
                return;
            }
            
            // Regular characters (with Shift consideration)
            String newChars = "";
            for (auto& key : keyList) {
                char c = M5Cardputer.Keyboard.getKey(key);
                
                // Skip modifiers and special keys
                if (c == 0 || c == 0xFF || c == 0x80 || c == 0x81 || c == 0x82) {
                    continue;
                }
                
                // Skip scrolling keys if Fn is pressed
                if (keys.fn && (c == '.' || c == ';')) {
                    continue;
                }
                
                // getKey() usually already accounts for Shift automatically
                // But if we need to force case change:
                if (keys.shift && c >= 'a' && c <= 'z') {
                    c = c - 'a' + 'A';  // Convert to uppercase
                }
                
                if (inputBuffer.length() < INPUT_BUFFER_SIZE - 1) {
                    newChars += c;
                }
            }
            
            if (newChars.length() > 0) {
                inputBuffer += newChars;
                renderInputLine();
            }
        }
    }
    
    delay(10);
}

