#ifndef PINS_H
#define PINS_H

// SD card pins (from Cardputer-Adv)
#define SD_SCK  40
#define SD_MISO 39
#define SD_MOSI 14
#define SD_CS   12

// External display pins (always defined for pin initialization, but only used when USE_EXTERNAL_DISPLAY is defined)
#define PIN_SCK   40  // SCK  -> EXT PIN 7 (same as SD_SCK)
#define PIN_MOSI  14  // MOSI -> EXT PIN 9 (same as SD_MOSI)
#define PIN_MISO  39  // MISO (not used for display, but for SD)
#define LCD_CS    5   // CS   -> EXT PIN 13
#define LCD_DC    6   // DC   -> EXT PIN 5
#define LCD_RST   3   // RST  -> EXT PIN 1

#endif // PINS_H
