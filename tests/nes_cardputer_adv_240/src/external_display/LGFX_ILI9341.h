#ifndef LGFX_ILI9341_H
#define LGFX_ILI9341_H

#ifdef USE_EXTERNAL_DISPLAY

#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_LCD.hpp>
#include "../pins.h"  // ✅ Общие определения пинов

// Local Panel_ILI9341 definition
struct Panel_ILI9341_Local : public lgfx::v1::Panel_LCD {
    Panel_ILI9341_Local(void) {
        // ✅ Нативка ILI9341 — 240×320 (портрет)
        _cfg.memory_width  = _cfg.panel_width  = 240;
        _cfg.memory_height = _cfg.panel_height = 320;
    }

    void setColorDepth_impl(lgfx::v1::color_depth_t depth) override {
        (void)depth;
        // Force 16-bit RGB565 for performance (2 bytes per pixel instead of 3)
        _write_depth = lgfx::v1::color_depth_t::rgb565_2Byte;
        _read_depth  = lgfx::v1::color_depth_t::rgb565_2Byte;
    }

protected:
    static constexpr uint8_t CMD_FRMCTR1 = 0xB1;
    static constexpr uint8_t CMD_INVCTR  = 0xB4;
    static constexpr uint8_t CMD_DFUNCTR = 0xB6;
    static constexpr uint8_t CMD_PWCTR1  = 0xC0;
    static constexpr uint8_t CMD_PWCTR2  = 0xC1;
    static constexpr uint8_t CMD_VMCTR1  = 0xC5;
    static constexpr uint8_t CMD_VMCTR2  = 0xC7;
    static constexpr uint8_t CMD_GMCTRP1 = 0xE0; // Positive Gamma Correction
    static constexpr uint8_t CMD_GMCTRN1 = 0xE1; // Negative Gamma Correction

    const uint8_t* getInitCommands(uint8_t listno) const override {
        static constexpr uint8_t list0[] = {
            0xEF       , 3, 0x03,0x80,0x02,
            0xCF       , 3, 0x00,0xC1,0x30,
            0xED       , 4, 0x64,0x03,0x12,0x81,
            0xE8       , 3, 0x85,0x00,0x78,
            0xCB       , 5, 0x39,0x2C,0x00,0x34,0x02,
            0xF7       , 1, 0x20,
            0xEA       , 2, 0x00,0x00,
            CMD_PWCTR1,  1, 0x23,
            CMD_PWCTR2,  1, 0x10,
            CMD_VMCTR1,  2, 0x3e,0x28,
            CMD_VMCTR2,  1, 0x86,
            CMD_FRMCTR1, 2, 0x00,0x13,
            0xF2       , 1, 0x00,
            CMD_GAMMASET,1, 0x01,  // Gamma set, curve 1
            CMD_GMCTRP1,15, 0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00,
            CMD_GMCTRN1,15, 0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F,
            CMD_DFUNCTR, 3, 0x08,0xC2,0x27,
            CMD_SLPOUT , 0+CMD_INIT_DELAY, 120,    // Exit sleep mode
            CMD_IDMOFF , 0,
            CMD_DISPON , 0+CMD_INIT_DELAY, 100,
            0xFF,0xFF, // end
        };
        switch (listno) {
        case 0: return list0;
        default: return nullptr;
        }
    }
};

// External ILI9341 display configuration
class LGFX_ILI9341 : public lgfx::v1::LGFX_Device {
    Panel_ILI9341_Local panel;
    lgfx::v1::Bus_SPI   bus;

public:
    LGFX_ILI9341() {
        // ✅ CRITICAL: Set LCD pins as OUTPUT before any operations
        pinMode(LCD_CS, OUTPUT);
        digitalWrite(LCD_CS, HIGH);
        pinMode(LCD_DC, OUTPUT);
        digitalWrite(LCD_DC, HIGH);
        pinMode(LCD_RST, OUTPUT);
        digitalWrite(LCD_RST, HIGH);
        
        // --- SPI bus ---
        auto b = bus.config();
        b.spi_host   = SPI3_HOST;     // Same host as SD (HSPI)
        b.spi_mode   = 0;
        b.freq_write = 20000000;      // ✅ 20 MHz (reduced for stability, ILI9341 may be more sensitive than ILI9488)
        b.freq_read  = 16000000;
        b.spi_3wire  = true;          // 3-wire SPI (without MISO)
        b.use_lock   = true;          // Transaction locking enabled
        b.dma_channel = 0;            // ✅ No DMA for stability

        b.pin_sclk = PIN_SCK;        // SCK  -> PIN 7
        b.pin_mosi = PIN_MOSI;       // MOSI -> PIN 9
        b.pin_miso = -1;             // Explicit -1, since 3-wire
        b.pin_dc   = LCD_DC;         // DC   -> PIN 5
        
        bus.config(b);
        panel.setBus(&bus);

        // --- Panel config ---
        auto p = panel.config();
        p.pin_cs    = LCD_CS;        // CS   -> PIN 13
        p.pin_rst   = LCD_RST;       // RST  -> PIN 1
        p.bus_shared = true;         // IMPORTANT for shared SPI
        p.readable   = false;        // Display not readable via MISO
        p.invert     = false;
        p.rgb_order  = false;
        p.dlen_16bit = false;
        
        // ✅ Нативные размеры контроллера ILI9341 (240×320 портрет)
        p.memory_width  = 240;
        p.memory_height = 320;
        p.panel_width   = 240;
        p.panel_height  = 320;
        
        // ✅ ВАЖНО: поворачиваем базу на 90° (чтобы rotation(0) был ландшафтом)
        p.offset_x = 0;
        p.offset_y = 0;
        p.offset_rotation = 1;   // ← Ключевое изменение!
        
        p.dummy_read_pixel = 8;
        p.dummy_read_bits = 1;
        panel.config(p);

        setPanel(&panel);
    }
};

// Global external display object (declaration)
extern LGFX_ILI9341 externalDisplay;

// Function to safely release LCD before SD operations
inline void lcd_quiesce() {
    // ✅ LCD_CS is guaranteed to be OUTPUT (set in constructor)
    externalDisplay.endWrite();
    externalDisplay.waitDisplay();
    digitalWrite(LCD_CS, HIGH);
}

#else
// Dummy declarations when external display is not used
class LGFX_ILI9341 {};
inline void lcd_quiesce() {}
#endif // USE_EXTERNAL_DISPLAY

#endif // LGFX_ILI9341_H
