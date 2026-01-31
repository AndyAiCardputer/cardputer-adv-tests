/*
 * NES Emulator - External Display ILI9341 (2.4")
 * 
 * NES эмулятор для Cardputer-Adv с внешним дисплеем ILI9341 (240x320, 2.4")
 * - Одно ядро (без RTOS)
 * - Внешний дисплей ILI9341 (240x320, 2.4 дюйма)
 * - Общая SPI шина для SD и дисплея
 * - Простая загрузка ROM с SD
 */

#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include "pins.h"
#ifdef USE_EXTERNAL_DISPLAY
#include "external_display/LGFX_ILI9341.h"
#endif

// Nofrendo
extern "C" {
    #include <nofrendo.h>
    int nofrendo_main(int argc, char *argv[]);
}

// OSD functions (defined in nes_osd.cpp)
extern "C" {
    int osd_init(void);
    void osd_shutdown(void);
    int osd_main(int argc, char *argv[]);
}

// Helper function to list SD card files
void listSD(const char* dirname, uint8_t levels, uint8_t maxDepth);

SPIClass sdSPI(HSPI);

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println("\n========================================");
    Serial.println("NES Emulator - External Display");
    Serial.println("Cardputer-Adv (ILI9341 240x320, 2.4\")");
    Serial.println("========================================\n");
    
    // ✅ 0) Set ALL CS pins HIGH (including LCD pins!) - как в рабочем nes_cardputer_adv_external
    pinMode(LCD_CS, OUTPUT);
    pinMode(SD_CS, OUTPUT);
    digitalWrite(LCD_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    pinMode(LCD_DC, OUTPUT);
    digitalWrite(LCD_DC, HIGH);
    pinMode(LCD_RST, OUTPUT);
    digitalWrite(LCD_RST, HIGH);
    Serial.println("  ✓ CS pins set HIGH");
    
    // ✅ 1) Initialize SPI bus FIRST (before LCD) - как в рабочем nes_cardputer_adv_external
    Serial.println("\nInitializing SPI bus (HSPI/SPI3_HOST)...");
    sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    Serial.println("  ✓ SPI bus initialized");
    
#ifdef USE_EXTERNAL_DISPLAY
    // ✅ 2) Initialize external display SECOND (after SPI, before M5Cardputer) - как в рабочем nes_cardputer_adv_external
    Serial.println("\nInitializing external display...");
    digitalWrite(SD_CS, HIGH);  // Гарантируем, что SD не активен
    
    if (!externalDisplay.init()) {
        Serial.println("  ✗ External display initialization FAILED!");
        Serial.println("  Check connections and power!");
        while (1) delay(1000);
    }
    
    Serial.println("  ✓ Display initialized!");
    externalDisplay.setRotation(0);  // Use base rotation from offset_rotation=1 (no additional rotation)
    externalDisplay.setColorDepth(16);  // 16-bit для производительности
    delay(50);
    
    externalDisplay.fillScreen(TFT_BLACK);
    Serial.printf("  ✓ External display ready: %ldx%ld\n", (long)externalDisplay.width(), (long)externalDisplay.height());
#endif
    
    // ✅ 3) Initialize M5Cardputer THIRD (after display) - как в рабочем nes_cardputer_adv_external
    Serial.println("\nInitializing M5Cardputer...");
    auto cfg = M5.config();
    cfg.output_power = true;
    M5Cardputer.begin(cfg);
    Serial.println("  ✓ M5Cardputer initialized");
    
    // Disable built-in display backlight (we use external display)
    M5Cardputer.Display.setBrightness(0);
    Serial.println("  ✓ Built-in display backlight: DISABLED");
    
    // ✅ 4) Initialize SD card FOURTH (after M5Cardputer, LCD quiesced) - как в рабочем nes_cardputer_adv_external
    Serial.println("\nInitializing SD card...");
#ifdef USE_EXTERNAL_DISPLAY
    lcd_quiesce();  // ✅ Безопасно освобождаем LCD перед SD
#endif
    
    // Register SD card in VFS with mount point "/sd"
    // This allows fopen() to access SD card files (required by nofrendo)
    // Signature: begin(ssPin, spi, frequency, mountpoint, max_files, format_if_empty)
    if (!SD.begin(SD_CS, sdSPI, 40000000, "/sd", 5, false)) {  // 40 MHz как в рабочем проекте
        Serial.println("  ✗ SD card initialization FAILED!");
        Serial.println("  Insert SD card and restart!");
#ifdef USE_EXTERNAL_DISPLAY
        externalDisplay.setTextColor(TFT_RED);
        externalDisplay.setTextSize(2);
        externalDisplay.setCursor(10, 50);
        externalDisplay.println("SD CARD ERROR!");
#endif
        while (1) delay(1000);
    }
    Serial.println("  ✓ SD card initialized and mounted at /sd");
    
    // Initialize OSD (display, sound, input)
    Serial.println("\nInitializing OSD...");
    if (osd_init() != 0) {
        Serial.println("  ✗ OSD initialization FAILED!");
        while (1) delay(1000);
    }
    Serial.println("  ✓ OSD initialized");
    
    // List files on SD card for debugging
    Serial.println("\n=== SD Card Files ===");
    listSD("/sd", 0, 2);  // List /sd directory, max depth 2
    Serial.println("====================\n");
    
    // Try different ROM paths
    // SD.exists() works with paths relative to mount point (without /sd prefix)
    // But fopen() needs full path with /sd prefix
    struct {
        const char* checkPath;  // For SD.exists() - without /sd
        const char* fopenPath;  // For nofrendo - with /sd
    } romPaths[] = {
        { "/roms/game.nes", "/sd/roms/game.nes" },
        { "/game.nes", "/sd/game.nes" },
        { "/roms/super_mario.nes", "/sd/roms/super_mario.nes" },
        { "/super_mario.nes", "/sd/super_mario.nes" }
    };
    
    const char* romPath = nullptr;
    
    Serial.println("Searching for ROM file...");
    for (int i = 0; i < sizeof(romPaths)/sizeof(romPaths[0]); i++) {
        Serial.printf("  Checking: %s", romPaths[i].checkPath);
        if (SD.exists(romPaths[i].checkPath)) {
            romPath = romPaths[i].fopenPath;  // Use full path for nofrendo
            Serial.printf(" ✓ FOUND! (using %s for nofrendo)\n", romPath);
            break;
        }
        Serial.println(" ✗ not found");
    }
    
    if (!romPath) {
        Serial.println("\n  ✗ No ROM file found!");
        Serial.println("  Please copy a .nes file to SD card:");
        Serial.println("    - /roms/game.nes");
        Serial.println("    - /game.nes");
        Serial.println("    - or any other .nes file");
        
#ifdef USE_EXTERNAL_DISPLAY
        externalDisplay.setTextColor(TFT_RED);
        externalDisplay.setTextSize(2);
        externalDisplay.setCursor(10, 20);
        externalDisplay.println("ROM NOT FOUND!");
        externalDisplay.setCursor(10, 60);
        externalDisplay.println("Copy .nes file to SD:");
        externalDisplay.setCursor(10, 100);
        externalDisplay.println("/roms/game.nes");
        externalDisplay.setCursor(10, 140);
        externalDisplay.println("(on SD card)");
#endif
        while (1) delay(1000);
    }
    
    Serial.printf("\nLoading ROM: %s\n", romPath);
    Serial.println("\n========================================");
    Serial.println("Starting NES emulator...");
    Serial.println("========================================\n");
    
    // Run nofrendo (this is blocking)
    char* argv_[1] = { (char*)romPath };
    nofrendo_main(1, argv_);
    
    // Should not reach here
    Serial.println("NES emulator exited");
}

// Helper function to list SD card files
void listSD(const char* dirname, uint8_t levels, uint8_t maxDepth) {
    if (levels > maxDepth) return;
    
    // Use path as-is if it already starts with /sd, otherwise add /sd prefix
    String path = dirname;
    if (path == "/sd" || path.startsWith("/sd/")) {
        // Already has /sd prefix, use as-is
    } else {
        // Add /sd prefix
        path = "/sd" + (path.startsWith("/") ? path : "/" + path);
    }
    
    File root = SD.open(path.c_str());
    if (!root) {
        Serial.printf("Failed to open directory: %s\n", path.c_str());
        return;
    }
    
    if (!root.isDirectory()) {
        Serial.printf("Not a directory: %s\n", path.c_str());
        root.close();
        return;
    }
    
    Serial.printf("Directory: %s\n", path.c_str());
    
    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        // Remove /sd prefix for display
        if (fileName.startsWith("/sd/")) {
            fileName = fileName.substring(4);
        } else if (fileName.startsWith("/sd")) {
            fileName = fileName.substring(3);
        }
        
        if (file.isDirectory()) {
            Serial.printf("  [DIR] %s\n", fileName.c_str());
            if (levels < maxDepth) {
                // Build full path for subdirectory
                String subPath = path;
                if (!subPath.endsWith("/")) subPath += "/";
                // Get relative name from full path
                String relName = file.name();
                if (relName.startsWith("/sd/")) {
                    relName = relName.substring(4);
                } else if (relName.startsWith("/sd")) {
                    relName = relName.substring(3);
                }
                subPath += relName;
                listSD(subPath.c_str(), levels + 1, maxDepth);
            }
        } else {
            Serial.printf("  [FILE] %s (%u bytes)\n", fileName.c_str(), file.size());
        }
        file = root.openNextFile();
    }
    
    root.close();
}

void loop() {
    // Empty - all work happens in nofrendo_main()
    delay(1000);
}
