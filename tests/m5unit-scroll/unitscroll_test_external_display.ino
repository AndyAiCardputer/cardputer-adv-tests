/*
 * Тест M5Unit-Scroll для Cardputer-Adv на внешнем дисплее ILI9488
 * 
 * M5Unit-Scroll - это энкодер/скроллер от M5Stack
 * 
 * Характеристики (из официального протокола I2C):
 * - I2C интерфейс (адрес 0x40)
 * - Ротационный энкодер (вращение влево/вправо)
 * - Кнопка нажатия
 * - RGB LED индикация
 * 
 * Регистры I2C:
 * - 0x10: Encoder value (16-bit, little-endian)
 * - 0x20: Button status (0 or 1)
 * - 0x30: RGB LED control (R, G, B)
 * - 0x40: Reset encoder (write 1)
 * - 0x50: Incremental encoder (16-bit, resets after read!)
 * - 0xF0: Info register (Bootloader/FW version, I2C addr)
 * 
 * Подключение ILI9488 через EXT 2.54-14P:
 * - VCC  -> PIN 2 (5VIN)
 * - GND  -> PIN 4 (GND)
 * - SCK  -> PIN 7 (GPIO 40)
 * - MOSI -> PIN 9 (GPIO 14)
 * - MISO -> PIN 11 (GPIO 39)
 * - CS   -> PIN 13 (GPIO 5)
 * - DC   -> PIN 5 (GPIO 6, BUSY)
 * - RST  -> PIN 1 (GPIO 3)
 * 
 * Подключение M5Unit-Scroll:
 * - PORT.A (Grove HY2.0-4P)
 * - SDA -> GPIO 2 (G2)
 * - SCL -> GPIO 1 (G1)
 * - GND -> GND
 * - 5V -> 5V
 * 
 * Примечание: Если модуль использует другой интерфейс (UART, SPI),
 * код нужно будет адаптировать после проверки документации.
 */

#include <M5Cardputer.h>
#include <M5GFX.h>
#include <Wire.h>
#include "lgfx/v1/panel/Panel_LCD.hpp"

// ============================================
// Локальное определение Panel_ILI9488
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
// Конфигурация внешнего дисплея ILI9488
// ============================================

class LGFX_ILI9488 : public lgfx::v1::LGFX_Device {
    Panel_ILI9488_Local panel;
    lgfx::v1::Bus_SPI   bus;

public:
    LGFX_ILI9488() {
        // --- SPI bus ---
        auto b = bus.config();
        b.spi_host   = SPI3_HOST;   // HSPI на ESP32-S3
        b.spi_mode   = 0;
        b.freq_write = 20000000;    // 20 МГц
        b.freq_read  = 16000000;
        b.spi_3wire  = true;        // 3-wire SPI
        b.use_lock   = false;       // Без блокировки SPI
        b.dma_channel = 0;          // Без DMA

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
        p.bus_shared = false;       // Не делим шину с SD
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

LGFX_ILI9488 lcd;  // Внешний дисплей ILI9488

// ============================================
// I2C настройки
// ============================================
#define UNIT_SCROLL_I2C_ADDRESS 0x40  // Официальный адрес из протокола I2C

// Используем Wire для PORT.A (GPIO 2/1) - общий I2C контроллер с клавиатурой
#define I2C_SDA_PIN 2  // GPIO 2 (G2)
#define I2C_SCL_PIN 1  // GPIO 1 (G1)

// Регистры из официального протокола I2C (M5Stack Unit Scroll Protocol)
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
// Состояние экрана скролла
// ============================================
bool showScrollTest = false;  // Флаг отображения тестового экрана скролла
unsigned long screenSwitchTime = 0;  // Время последнего переключения экрана
const int SCREEN_SWITCH_DELAY = 500;  // Пауза после переключения экрана перед чтением I2C (мс)
const int MAX_LIST_ITEMS = 20;  // Максимум элементов в списке
String listItems[MAX_LIST_ITEMS] = {
    "Item 1", "Item 2", "Item 3", "Item 4", "Item 5",
    "Item 6", "Item 7", "Item 8", "Item 9", "Item 10",
    "Item 11", "Item 12", "Item 13", "Item 14", "Item 15",
    "Item 16", "Item 17", "Item 18", "Item 19", "Item 20"
};
bool listItemsChecked[MAX_LIST_ITEMS] = {false};  // Флаги пометки
int listItemCount = MAX_LIST_ITEMS;  // Количество элементов
int selectedListItem = 0;  // Выбранный элемент (0-based)
int lastScrollNavTime = 0;  // Время последней навигации (debounce)
const int SCROLL_NAV_DEBOUNCE = 150;  // Debounce для навигации (мс)
unsigned long lastScrollReadTime = 0;  // Время последнего чтения энкодера
const int SCROLL_READ_INTERVAL = 50;  // Интервал чтения энкодера (мс) - не читать слишком часто
int i2cErrorCount = 0;  // Счетчик ошибок I2C
const int MAX_I2C_ERRORS = 10;  // Максимум ошибок подряд перед пропуском чтений (увеличено)

// ============================================
// Состояние для оптимизации отрисовки скролла
// ============================================
bool scrollScreenInitialized = false;  // Флаг первой инициализации экрана
int lastFirstVisible = -1;  // Последний первый видимый элемент (для оптимизации)

// ============================================
// Мягкий ресет I2C-шины
// ============================================
void i2cBusReset() {
    Serial.println(">>> I2C Bus Reset: Recovering from errors...");
    
    // Останавливаем общий I2C
    Wire.end();
    delay(50);
    
    // Дёргаем SCL 9 раз, чтобы освободить зависшего slave
    pinMode(I2C_SCL_PIN, OUTPUT);
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
    }
    
    // Возвращаем ноги в "I2C-стиль"
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    delay(50);
    
    // Переинициализация Wire на порт A
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);  // 50 kHz для STM32F030
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
    
    // Инициализация внешнего ILI9488
    Serial.println("\nInitializing ILI9488 display...");
    
    if (lcd.init()) {
        Serial.println("  ✓ ILI9488 initialized successfully!");
        Serial.printf("  ✓ Display size: %dx%d\n", lcd.width(), lcd.height());
        
        lcd.setRotation(3);  // 180 градусов
        lcd.setColorDepth(24);
        
        lcd.fillScreen(TFT_BLACK);
        lcd.setTextSize(2);  // Больший размер шрифта для большого экрана
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
    
    // Инициализация Cardputer ПОСЛЕ дисплея
    M5Cardputer.begin(true);
    delay(200);
    
    // Отключаем подсветку встроенного дисплея
    M5.Display.setBrightness(0);
    Serial.println("  ✓ Built-in display backlight: DISABLED");
    
    // Инициализация I2C на PORT.A
    Serial.println("\nInitializing I2C...");
    Serial.println("Using PORT.A (GPIO 2/1)");
    Serial.println("CRITICAL: Using Wire (shared with keyboard) instead of Wire1");
    
    // ВАЖНО: STM32F030 использует pull-up резисторы, ESP32 тоже должен их включить
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);
    pinMode(I2C_SCL_PIN, INPUT_PULLUP);
    delay(100);  // Даем время на стабилизацию pull-up
    
    // ИСПРАВЛЕНИЕ: Используем Wire вместо Wire1 (общий контроллер)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 50000);  // 50 kHz для STM32F030
    Wire.setTimeOut(100);
    
    Serial.printf("  SDA: GPIO %d (G2)\n", I2C_SDA_PIN);
    Serial.printf("  SCL: GPIO %d (G1)\n", I2C_SCL_PIN);
    Serial.printf("  Address: 0x%02X\n", UNIT_SCROLL_I2C_ADDRESS);
    Serial.printf("  Speed: 50 kHz\n");
    Serial.printf("  Clock stretching: Enabled\n");
    
    // STM32F030 может требовать больше времени на инициализацию после включения питания
    Serial.println("\nWaiting for STM32F030 initialization...");
    delay(1000);  // Увеличена задержка для STM32F030
    
    // Сканирование I2C шины для поиска модуля
    Serial.println("\nScanning I2C bus for Scroll module...");
    Serial.println("STM32F030 I2C slave - trying multiple detection methods");
    Serial.println("(Trying addresses: 0x40, 0x5E, 0x5F, 0x41, 0x42)");
    
    uint8_t addresses[] = {0x40, 0x5E, 0x5F, 0x41, 0x42};
    bool found = false;
    uint8_t foundAddress = 0;
    
    // Метод 1: Стандартное сканирование
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
        delay(50);  // Увеличена задержка между попытками для STM32F030
    }
    
    // Метод 2: Прямое чтение регистров (STM32F030 может не отвечать на beginTransmission)
    if (!found) {
        Serial.println("\nMethod 2: Direct register read (STM32F030 I2C slave mode)");
        Serial.println("Trying to read register 0x20 (Button register)...");
        
        for (int attempt = 0; attempt < 3; attempt++) {
            Wire.beginTransmission(UNIT_SCROLL_I2C_ADDRESS);
            Wire.write(SCROLL_BUTTON_REG);  // 0x20
            byte error = Wire.endTransmission(false);  // false = repeated start (важно для STM32F030)
            
            if (error == 0) {
                delayMicroseconds(1000);  // Увеличена задержка для STM32F030
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
            delay(200);  // Задержка между попытками
        }
    }
    
    // Метод 3: Попробовать разные скорости I2C (STM32F030 может требовать другую скорость)
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
        
        // Возвращаемся к стандартной скорости
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
        
        // Отображение ошибки на дисплее
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
        // Модуль найден - устанавливаем флаг
        moduleFound = true;
        
        // Пробуем прочитать информацию о модуле
        Serial.println("\n✓ Module found! Testing communication...");
        
        // Сначала пробуем прочитать I2C адрес (регистр 0xFF из прошивки)
        // Из прошивки: при чтении 0xFF модуль возвращает свой I2C адрес
        Wire.beginTransmission(foundAddress);
        Wire.write(0xFF);  // Команда чтения I2C адреса
        Wire.endTransmission(false);
        delayMicroseconds(200);
        
        uint8_t addrBytes = Wire.requestFrom(foundAddress, 1, true);
        if (addrBytes > 0 && Wire.available()) {
            uint8_t moduleAddr = Wire.read();
            Serial.printf("  ✓ Module responds! I2C Address: 0x%02X\n", moduleAddr);
        } else {
            Serial.println("  ⚠️ Module found but does not respond to commands");
            Serial.println("  Trying direct register read...");
            
            // Пробуем прочитать кнопку напрямую (регистр 0x20)
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
        
        // Отображение успешного подключения
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

// Улучшенная функция чтения регистра с обработкой ошибок
bool readScrollRegister(uint8_t reg, uint8_t length, uint8_t* data) {
    // Проверяем что модуль найден
    if (!moduleFound) {
        return false;
    }
    
    // Используем найденный адрес, если он установлен, иначе константу
    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
    
    // Отправляем адрес регистра
    Wire.beginTransmission(address);
    Wire.write(reg);
    byte error = Wire.endTransmission(false);  // false = не останавливать шину (repeated start)
    
    if (error != 0) {
        i2cErrorCount++;
        return false;
    }
    
    // Задержка для STM32F030 перед чтением (уменьшена для стабильности)
    delayMicroseconds(500);  // Оптимальная задержка для STM32F030
    
    // Запрашиваем данные
    uint8_t bytesReceived = Wire.requestFrom(address, length, true);
    
    if (bytesReceived == 0) {
        i2cErrorCount++;
        return false;
    }
    
    // Небольшая задержка после requestFrom для STM32F030
    delayMicroseconds(300);
    
    // Читаем данные (максимум 16 байт для info register)
    for (int i = 0; i < length && i < 16; i++) {
        if (Wire.available()) {
            data[i] = Wire.read();
        } else {
            i2cErrorCount++;
            return false;
        }
    }
    
    // Успешное чтение - обнуляем счетчик ошибок
    i2cErrorCount = 0;
    return true;
}

// Обертка для обратной совместимости
int readScrollRegisterValue(uint8_t reg, uint8_t length) {
    uint8_t data[4] = {0};
    
    if (!readScrollRegister(reg, length, data)) {
        // Ошибка чтения - возвращаем последнее известное значение
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
// Функция для управления RGB LED
// ============================================
// color: формат 0xRRGGBB (например, 0xFF0000 = красный, 0x00FF00 = зеленый, 0x0000FF = синий)
// Из прошивки: neopixel_set_color() ожидает R в байт 8-15, G в байт 16-23, B в байт 24-31
// Значит записываем: 0x31 = R, 0x32 = G, 0x33 = B
void setScrollLEDFast(uint32_t color) {
    if (!moduleFound) {
        return;
    }
    
    // Используем найденный адрес, если он установлен, иначе константу
    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
    
    // Извлекаем RGB из формата 0xRRGGBB
    uint8_t r = (color >> 16) & 0xFF;  // Красный
    uint8_t g = (color >> 8) & 0xFF;   // Зеленый
    uint8_t b = color & 0xFF;          // Синий
    
    // Записываем R, G, B в регистры 0x31, 0x32, 0x33
    // neopixel_set_color() ожидает: байт 8-15 = R, байт 16-23 = G, байт 24-31 = B
    Wire.beginTransmission(address);
    Wire.write(0x31);  // Начальный регистр для R
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
// Функция отрисовки тестового экрана скролла (ОПТИМИЗИРОВАННАЯ)
// ============================================
// Стиль как в эмуляторе ZX Spectrum
// Использует частичные обновления для устранения мерцания
void drawScrollTest() {
    // Вычисляем первый видимый элемент (скроллинг)
    int firstVisible = selectedListItem - 3;
    if (firstVisible < 0) firstVisible = 0;
    
    // Группируем все операции SPI для устранения мерцания
    lcd.startWrite();
    
    // Перерисовываем статические элементы только при первой инициализации
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
    
    // Обновляем счетчик (справа) - всегда
    lcd.fillRect(360, 10, 120, 20, TFT_BLACK);  // Очищаем область счетчика
    lcd.setTextSize(2);
    lcd.setTextColor(TFT_CYAN, TFT_BLACK);
    lcd.setCursor(360, 10);
    lcd.printf("%d/%d", selectedListItem + 1, listItemCount);
    
    // Перерисовываем рамку ВСЕГДА (чтобы она не пропадала)
    lcd.drawRect(20, 40, 440, 190, TFT_WHITE);
    
    // Очищаем область списка с запасом для устранения артефактов
    // Очищаем всю внутреннюю область рамки (с отступом 2px от краев рамки)
    // Это гарантирует удаление всех артефактов от предыдущих отрисовок
    lcd.fillRect(22, 42, 436, 186, TFT_BLACK);  // Внутри рамки с отступом 2px
    
    // Обновляем lastFirstVisible для отслеживания изменений
    if (firstVisible != lastFirstVisible) {
        lastFirstVisible = firstVisible;
    }
    
    int y = 50;  // Начальная Y позиция
    
    for (int i = 0; i < 8; i++) {
        int itemIdx = firstVisible + i;
        
        // Проверяем границы
        if (itemIdx >= listItemCount) break;
        
        bool isSelected = (itemIdx == selectedListItem);
        bool isChecked = listItemsChecked[itemIdx];
        
        // Устанавливаем размер и цвет
        if (isSelected) {
            lcd.setTextSize(4);  // Большой шрифт для выбранного
            lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        } else {
            lcd.setTextSize(2);  // Маленький шрифт для остальных
            lcd.setTextColor(isChecked ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
        }
        
        lcd.setCursor(30, y);
        
        // Получаем имя элемента
        String name = listItems[itemIdx];
        
        // Обрезаем длинные имена
        int maxLen = isSelected ? 20 : 35;
        if (name.length() > maxLen) {
            name = name.substring(0, maxLen - 3) + "...";
        }
        
        // Показываем пометку [✓] перед именем если элемент помечен
        if (isChecked) {
            lcd.print("[✓] ");
        }
        
        lcd.print(name);
        
        // Следующая строка
        y += isSelected ? 32 : 22;
    }
    
    lcd.endWrite();  // Завершаем группу SPI операций
}

// ============================================
// Функция для сброса состояния при переключении экранов
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
        
        // Пробел - переключение между экранами
        if (keys.space) {
            showScrollTest = !showScrollTest;
            Serial.printf(">>> Screen switched: %s\n", showScrollTest ? "Scroll Test" : "Main");
            
            // Запоминаем время переключения экрана
            screenSwitchTime = millis();
            
            // Сбрасываем счетчик ошибок и состояние при переключении экранов
            i2cErrorCount = 0;
            lastScrollReadTime = 0;
            lastScrollNavTime = 0;
            lastButtonState = false;  // Сбрасываем состояние кнопки
            
            if (showScrollTest) {
                // Переходим на экран скролла - ПОЛНАЯ очистка экрана перед переключением
                lcd.fillScreen(TFT_BLACK);  // Полная очистка для удаления артефактов
                resetScrollScreenState();
                drawScrollTest();
            } else {
                // Возвращаемся на главный экран
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
        // Модуль не найден - не пытаемся читать
        delay(1000);
        return;
    }
    
    // ═══ ЕСЛИ ЭКРАН СКРОЛЛА АКТИВЕН ═══
    if (showScrollTest) {
        unsigned long currentTime = millis();
        
        // ВАЖНО: Не читать I2C сразу после переключения экрана!
        // Даем модулю время на восстановление после переключения
        if (screenSwitchTime > 0 && (currentTime - screenSwitchTime) < SCREEN_SWITCH_DELAY) {
            delay(10);
            return;
        }
        
        // Проверяем интервал чтения - не читать слишком часто
        if (currentTime - lastScrollReadTime < SCROLL_READ_INTERVAL) {
            delay(10);
            return;
        }
        lastScrollReadTime = currentTime;
        
        // Если слишком много ошибок подряд - делаем мягкий ресет шины
        if (i2cErrorCount >= MAX_I2C_ERRORS) {
            i2cBusReset();  // Мягкий ресет шины
            i2cErrorCount = 0;
            delay(200);
            return;  // Пропускаем чтение после ошибок
        }
        
        // Читаем инкрементальное значение энкодера для навигации
        uint8_t incData[2] = {0};
        if (readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData)) {
            i2cErrorCount = 0;  // Успешное чтение - сбрасываем счетчик ошибок
            int16_t incValue = (int16_t)(incData[0] | (incData[1] << 8));
            
            if (incValue != 0 && (currentTime - lastScrollNavTime > SCROLL_NAV_DEBOUNCE)) {
                // Навигация по списку
                if (incValue > 0) {
                    // Вращение вправо → список вниз
                    if (selectedListItem < listItemCount - 1) {
                        selectedListItem++;
                        drawScrollTest();
                        Serial.printf(">>> Scroll DOWN → Item %d/%d\n", selectedListItem + 1, listItemCount);
                        setScrollLEDFast(0x00FF00);  // Зеленый при прокрутке вниз
                    }
                } else {
                    // Вращение влево → список вверх
                    if (selectedListItem > 0) {
                        selectedListItem--;
                        drawScrollTest();
                        Serial.printf(">>> Scroll UP → Item %d/%d\n", selectedListItem + 1, listItemCount);
                        setScrollLEDFast(0xFF0000);  // Красный при прокрутке вверх
                    }
                }
                lastScrollNavTime = currentTime;
            }
        } else {
            // Ошибка чтения - увеличиваем счетчик (но не блокируем полностью)
            i2cErrorCount++;
            // Логируем только каждую 5-ю ошибку чтобы не засорять Serial
            if (i2cErrorCount % 5 == 0) {
                Serial.printf(">>> I2C read errors: %d\n", i2cErrorCount);
            }
        }
        
        // Читаем состояние кнопки для пометки/снятия пометки (реже чем энкодер)
        static unsigned long lastButtonReadTime = 0;
        if (currentTime - lastButtonReadTime > 100) {  // Читаем кнопку раз в 100мс
            uint8_t buttonData[1] = {0};
            if (readScrollRegister(SCROLL_BUTTON_REG, 1, buttonData)) {
                i2cErrorCount = 0;  // Успешное чтение - сбрасываем счетчик
                bool buttonState = (buttonData[0] != 0);
                
                if (buttonState && !lastButtonState) {
                    // Кнопка нажата - переключаем пометку
                    listItemsChecked[selectedListItem] = !listItemsChecked[selectedListItem];
                    drawScrollTest();
                    Serial.printf(">>> Item %d %s\n", selectedListItem + 1, 
                                 listItemsChecked[selectedListItem] ? "CHECKED" : "UNCHECKED");
                    setScrollLEDFast(0x0000FF);  // Синий при пометке
                    delay(100);
                    setScrollLEDFast(0x000000);  // Выключаем LED
                }
                
                lastButtonState = buttonState;
            }
            lastButtonReadTime = currentTime;
        }
        
        delay(10);
        return;
    }
    
    // ═══ ГЛАВНЫЙ ЭКРАН (оригинальный тест) ═══
    // Сбрасываем счетчик ошибок при возврате на главный экран
    i2cErrorCount = 0;
    
    // Читаем инкрементальное значение энкодера (0x50) - это то что нужно!
    // Этот регистр показывает изменение с последнего чтения и автоматически сбрасывается
    uint8_t incData[2] = {0};
    if (readScrollRegister(SCROLL_INC_ENCODER_REG, 2, incData)) {
        i2cErrorCount = 0;  // Успешное чтение - сбрасываем счетчик ошибок
        int16_t incValue = (int16_t)(incData[0] | (incData[1] << 8));  // little-endian
        
        if (incValue != 0) {
            // Было вращение!
            lastEncoderValue += incValue;  // Обновляем общее значение
            Serial.printf(">>> Encoder Increment: %+d (Total: %d)\n", incValue, lastEncoderValue);
            
            // Меняем цвет LED в зависимости от направления вращения
            if (incValue > 0) {
                // Вращение вправо - зеленый
                setScrollLEDFast(0x00FF00);  // Зеленый
            } else {
                // Вращение влево - красный
                setScrollLEDFast(0xFF0000);  // Красный
            }
            
            // Отображение на внешнем дисплее
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
    
    // Также читаем абсолютное значение энкодера (0x10) для синхронизации
    int encoderValue = readScrollRegisterValue(SCROLL_ENCODER_REG, 2);
    if (encoderValue != lastEncoderValue) {
        // Обновляем если изменилось (на случай если пропустили инкремент)
        lastEncoderValue = encoderValue;
    }
    
    // Читаем состояние кнопки (0x20)
    uint8_t buttonData[1] = {0};
    if (readScrollRegister(SCROLL_BUTTON_REG, 1, buttonData)) {
        bool buttonState = (buttonData[0] != 0);
        
        if (buttonState != lastButtonState) {
            Serial.printf(">>> Button: %s\n", buttonState ? "PRESSED" : "RELEASED");
            
            // Меняем цвет LED при нажатии кнопки
            if (buttonState) {
                // Кнопка нажата - синий
                setScrollLEDFast(0x0000FF);  // Синий
            } else {
                // Кнопка отпущена - выключаем LED
                setScrollLEDFast(0x000000);  // Черный (выключено)
            }
            
            // Отображение на внешнем дисплее
            lcd.fillRect(0, 240, 480, 80, TFT_BLACK);
            lcd.setCursor(10, 250);
            lcd.setTextSize(2);
            lcd.setTextColor(buttonState ? TFT_RED : TFT_WHITE, TFT_BLACK);
            lcd.printf("Button: %s", buttonState ? "PRESSED" : "RELEASED");
            
            // При нажатии кнопки - сброс энкодера (как в MicroPython примере)
            if (buttonState) {
                if (moduleFound) {
                    uint8_t address = (foundAddress != 0) ? foundAddress : UNIT_SCROLL_I2C_ADDRESS;
                    Wire.beginTransmission(address);
                    Wire.write(SCROLL_RESET_REG);
                    Wire.write(1);  // Запись 1 для сброса энкодера
                    Wire.endTransmission();
                }
                lastEncoderValue = 0;
                Serial.println(">>> Encoder reset!");
                
                // Обновляем отображение
                lcd.fillRect(0, 130, 480, 100, TFT_BLACK);
                lcd.setCursor(10, 140);
                lcd.setTextColor(TFT_GREEN, TFT_BLACK);
                lcd.setTextSize(3);
                lcd.printf("Encoder: 0\n");
            }
            
            lastButtonState = buttonState;
        }
    }
    
    delay(50);  // Небольшая задержка для стабильности
}

