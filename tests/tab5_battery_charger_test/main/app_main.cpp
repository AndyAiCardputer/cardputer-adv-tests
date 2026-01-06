#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/m5stack_tab5.h"
#include "bsp/display.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "battery_monitor.h"

static const char *TAG = "BatteryTest";

// Display handles
static bsp_lcd_handles_t lcd_handles;
static esp_lcd_panel_handle_t panel_handle = NULL;

// Tab5 display dimensions (portrait: 720×1280, rotated to landscape: 1280×720)
#define DISPLAY_PHYSICAL_WIDTH   720
#define DISPLAY_PHYSICAL_HEIGHT  1280
// Landscape dimensions (for UI elements)
#define DISPLAY_LANDSCAPE_WIDTH  1280
#define DISPLAY_LANDSCAPE_HEIGHT 720

// Framebuffer (720x1280 RGB565 portrait)
static uint16_t* framebuffer = NULL;

// Colors (RGB565)
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_GRAY    0x8410

// Simple 8x8 bitmap font (ASCII 32-126) - FULL FONT from NES emulator
static const uint8_t font_8x8[95][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space (32)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // ! (33)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // " (34)
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // # (35)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // $ (36)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // % (37)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // & (38)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' (39)
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // ( (40)
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // ) (41)
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // * (42)
    {0x00, 0x0C, 0x0C, 0x7F, 0x0C, 0x0C, 0x00, 0x00}, // + (43)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x06, 0x00}, // , (44)
    {0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00}, // - (45)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // . (46)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // / (47)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x63, 0x3E}, // 0 (48)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F}, // 1 (49)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x33, 0x3F}, // 2 (50)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x33, 0x1E}, // 3 (51)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x30, 0x78}, // 4 (52)
    {0x3F, 0x03, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E}, // 5 (53)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x33, 0x1E}, // 6 (54)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x0C}, // 7 (55)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x33, 0x1E}, // 8 (56)
    {0x1E, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E}, // 9 (57)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // : (58)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x06, 0x00}, // ; (59)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // < (60)
    {0x00, 0x00, 0x7F, 0x00, 0x00, 0x7F, 0x00, 0x00}, // = (61)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // > (62)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // ? (63)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // @ (64)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // A (65)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // B (66)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // C (67)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // D (68)
    {0x7F, 0x06, 0x06, 0x3E, 0x06, 0x06, 0x7F, 0x00}, // E (69)
    {0x7F, 0x06, 0x06, 0x3E, 0x06, 0x06, 0x06, 0x00}, // F (70)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // G (71)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // H (72)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // I (73)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // J (74)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // K (75)
    {0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x7F, 0x00}, // L (76)
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // M (77)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // N (78)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // O (79)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x06, 0x00}, // P (80)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // Q (81)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // R (82)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // S (83)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // T (84)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x00}, // U (85)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // V (86)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W (87)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // X (88)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // Y (89)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // Z (90)
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, // [ (91)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, // \ (92)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, // ] (93)
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // ^ (94)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // _ (95)
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // ` (96)
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, // a (97)
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, // b (98)
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, // c (99)
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00}, // d (100)
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00}, // e (101)
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00}, // f (102)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // g (103)
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, // h (104)
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // i (105)
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, // j (106)
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, // k (107)
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // l (108)
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // m (109)
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, // n (110)
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, // o (111)
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, // p (112)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, // q (113)
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, // r (114)
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, // s (115)
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, // t (116)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, // u (117)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // v (118)
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, // w (119)
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, // x (120)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // y (121)
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, // z (122)
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, // { (123)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // | (124)
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, // } (125)
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~ (126)
};

// Get font data for character
static const uint8_t* get_font_char(char c) {
    if (c < 32 || c > 126) {
        return font_8x8[0]; // Space
    }
    int idx = c - 32;
    if (idx < 95) {
        return font_8x8[idx];
    }
    return font_8x8[0]; // Space for unknown
}

// Convert landscape coordinates to portrait for framebuffer
// Tab5 display: physical 720x1280 (portrait), rotated 90° left → 1280x720 (landscape)
// Formula: for landscape (ax, ay) → portrait (px, py) where:
//   px = ay
//   py = DISPLAY_PHYSICAL_HEIGHT - ax - 1
static void set_pixel_landscape(int ax, int ay, uint16_t color) {
    if (!framebuffer || ax < 0 || ax >= DISPLAY_LANDSCAPE_WIDTH || 
        ay < 0 || ay >= DISPLAY_LANDSCAPE_HEIGHT) {
        return;
    }
    
    int px = ay;
    int py = DISPLAY_PHYSICAL_HEIGHT - ax - 1;
    
    if (px >= 0 && px < DISPLAY_PHYSICAL_WIDTH && 
        py >= 0 && py < DISPLAY_PHYSICAL_HEIGHT) {
        framebuffer[py * DISPLAY_PHYSICAL_WIDTH + px] = color;
    }
}

// Clear screen with color
static void clear_screen(uint16_t color) {
    if (!framebuffer) return;
    for (int i = 0; i < DISPLAY_PHYSICAL_WIDTH * DISPLAY_PHYSICAL_HEIGHT; i++) {
        framebuffer[i] = color;
    }
}

// Draw character at landscape position with scale
static void draw_char(int x, int y, char c, uint16_t color, int scale) {
    const uint8_t* char_data = get_font_char(c);
    
    for (int py = 0; py < 8; py++) {
        uint8_t row = char_data[py];
        for (int px = 0; px < 8; px++) {
            // FIXED: read bits right-to-left (as in NES emulator) for correct orientation
            if (row & (1 << px)) {  // Was: (1 << (7 - px)) - this caused mirroring
                // Draw scaled pixel
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        set_pixel_landscape(x + px * scale + sx, y + py * scale + sy, color);
                    }
                }
            }
        }
    }
}

// Draw string
static void draw_string(int x, int y, const char* str, uint16_t color, int scale) {
    if (!str) return;
    int cx = x;
    for (const char* p = str; *p; p++) {
        draw_char(cx, y, *p, color, scale);
        cx += 8 * scale + 1; // Character width + spacing
    }
}

// Draw rectangle at landscape coordinates
static void draw_rect(int x, int y, int w, int h, uint16_t color) {
    // Top and bottom lines
    for (int i = 0; i < w; i++) {
        set_pixel_landscape(x + i, y, color);
        set_pixel_landscape(x + i, y + h - 1, color);
    }
    // Left and right lines
    for (int i = 0; i < h; i++) {
        set_pixel_landscape(x, y + i, color);
        set_pixel_landscape(x + w - 1, y + i, color);
    }
}

// Fill rectangle at landscape coordinates
static void fill_rect(int x, int y, int w, int h, uint16_t color) {
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            set_pixel_landscape(x + px, y + py, color);
        }
    }
}

// Format float to string (simple version)
static void format_float(char* buf, int buf_size, float value, int decimals) {
    if (value < 0) {
        snprintf(buf, buf_size, "-%.*f", decimals, -value);
    } else {
        snprintf(buf, buf_size, "%.*f", decimals, value);
    }
}

// Format integer to string (unused but kept for future use)
// static void format_int(char* buf, int buf_size, int value) {
//     snprintf(buf, buf_size, "%d", value);
// }

// Detect USB-C presence using simplified logic
// Hardware bit is the primary source of truth, current is used for validation
static bool detect_usb_present(int voltage_mv, float current_ma, bool is_charging) {
    // Get hardware USB detection bit (most reliable)
    bool raw_detect = bsp_usb_c_detect();
    
    // 1. If battery is charging (negative current), USB must be connected
    // This overrides hardware bit if it's wrong
    if (current_ma < -10.0f) {
        return true; // USB connected (charging)
    }
    
    // 2. If discharging significantly (>50mA), USB is likely disconnected
    // This overrides hardware bit if it's wrong
    if (current_ma > 50.0f) {
        return false; // USB disconnected (significant discharge)
    }
    
    // 3. For idle/small discharge (0-50mA), use hardware bit as primary source
    // Hardware bit is most reliable for USB detection
    return raw_detect;
}

// Battery presence is now determined by voltage classification voting in battery_monitor component

// Update display with battery telemetry
static void update_display(battery_status_t* status, float current_ma, bool usb_present, bool battery_present) {
    clear_screen(COLOR_BLACK);
    
    int scale = 3; // 3x scale for better readability
    int y_pos = 50;
    int line_height = 30;
    
    // Title
    draw_string(50, y_pos, "BATTERY TEST", COLOR_WHITE, scale);
    y_pos += line_height * 2;
    
    // Voltage
    char buf[32];
    snprintf(buf, sizeof(buf), "Voltage: %d mV", status->voltage_mv);
    draw_string(50, y_pos, buf, COLOR_CYAN, scale);
    y_pos += line_height;
    
    // Current with direction indicator
    format_float(buf, sizeof(buf), current_ma, 3);  // 3 decimal places for better precision
    char current_str[64];
    const char* current_icon = "";
    if (current_ma < -0.1f) {
        current_icon = "->";  // Charging (negative current = charging)
    } else if (current_ma > 0.1f) {
        current_icon = "<-";  // Discharging (positive current = discharging)
    } else {
        current_icon = "=";   // Idle (very small current)
    }
    snprintf(current_str, sizeof(current_str), "Current: %s %s mA", current_icon, buf);
    draw_string(50, y_pos, current_str, COLOR_YELLOW, scale);
    y_pos += line_height;
    
    // Battery percentage (use level from battery_monitor_read, now 0-100%)
    int percent = status->level;
    if (percent < 0) {
        snprintf(buf, sizeof(buf), "Level: N/A");
    } else {
        if (percent > 100) percent = 100;  // Safety limit
        snprintf(buf, sizeof(buf), "Level: %d%%", percent);
    }
    draw_string(50, y_pos, buf, COLOR_GREEN, scale);
    y_pos += line_height;
    
    // Status
    const char* status_str = "UNKNOWN";
    uint16_t status_color = COLOR_GRAY;
    
    if (!battery_present) {
        status_str = "NO BATTERY";
        status_color = COLOR_RED;
    } else if (status->is_charging) {
        status_str = "CHARGING";
        status_color = COLOR_GREEN;
    } else if (current_ma > 5.0f) {
        status_str = "DISCHARGING";
        status_color = COLOR_YELLOW;
    } else {
        status_str = "FULL/IDLE";
        status_color = COLOR_CYAN;
    }
    
    snprintf(buf, sizeof(buf), "Status: %s", status_str);
    draw_string(50, y_pos, buf, status_color, scale);
    y_pos += line_height;
    
    // USB status
    snprintf(buf, sizeof(buf), "USB: %s", usb_present ? "CONNECTED" : "DISCONNECTED");
    draw_string(50, y_pos, buf, usb_present ? COLOR_GREEN : COLOR_RED, scale);
    y_pos += line_height;
    
    // Battery present
    snprintf(buf, sizeof(buf), "Battery: %s", battery_present ? "PRESENT" : "NOT PRESENT");
    draw_string(50, y_pos, buf, battery_present ? COLOR_GREEN : COLOR_RED, scale);
    
    // Draw battery icon (simple rectangle) - landscape coordinates
    int icon_x = DISPLAY_LANDSCAPE_WIDTH - 150;  // 1280 - 150 = 1130
    int icon_y = 50;
    int icon_w = 100;
    int icon_h = 50;
    
    // Battery frame
    draw_rect(icon_x, icon_y, icon_w, icon_h, COLOR_WHITE);
    // Terminal
    fill_rect(icon_x + icon_w, icon_y + 15, 10, 20, COLOR_WHITE);
    
    // Fill based on percentage
    if (battery_present && percent >= 0) {
        int fill_w = (icon_w - 4) * percent / 100;
        uint16_t fill_color = COLOR_GREEN;
        if (percent <= 20) fill_color = COLOR_RED;
        else if (percent <= 60) fill_color = COLOR_YELLOW;
        
        fill_rect(icon_x + 2, icon_y + 2, fill_w, icon_h - 4, fill_color);
    }
    
    // Flush to display (physical dimensions)
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 
                              DISPLAY_PHYSICAL_WIDTH, DISPLAY_PHYSICAL_HEIGHT, framebuffer);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Tab5 Battery Charger Test");
    ESP_LOGI(TAG, "ESP-IDF Framework");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
    
    // Initialize I2C bus
    ESP_LOGI(TAG, "Initializing I2C...");
    ret = bsp_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "I2C initialized");
    
    // Initialize IO expanders
    ESP_LOGI(TAG, "Initializing IO expanders...");
    i2c_master_bus_handle_t i2c_bus = bsp_i2c_get_handle();
    bsp_io_expander_pi4ioe_init(i2c_bus);
    ESP_LOGI(TAG, "IO expanders initialized");
    
    // Enable battery charging
    ESP_LOGI(TAG, "Enabling battery charging...");
    bsp_set_charge_en(true);
    bsp_set_charge_qc_en(true);
    
    // Verify charge enable status
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for I2C to settle
    bool charge_enabled = bsp_get_charge_en();
    ESP_LOGI(TAG, "Battery charging enabled: %s (verified: %s)", 
             charge_enabled ? "YES" : "NO", charge_enabled ? "OK" : "FAILED!");
    
    // Initialize battery monitor
    ESP_LOGI(TAG, "Initializing battery monitor...");
    ret = battery_monitor_init(i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Battery monitor init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Battery monitor initialized");
    
    // Initialize display
    ESP_LOGI(TAG, "Initializing display...");
    bsp_display_config_t display_config = {};
    ret = bsp_display_new_with_handles_to_st7123(&display_config, &lcd_handles);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display initialization FAILED: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "Display initialized successfully");
    panel_handle = lcd_handles.panel;
    
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle is NULL!");
        return;
    }
    
    // CRITICAL: Re-enable battery charging after display init
    // Display initialization calls bsp_io_expander_pi4ioe_init() which resets the charge enable register
    ESP_LOGI(TAG, "Re-enabling battery charging after display init...");
    bsp_set_charge_en(true);
    bsp_set_charge_qc_en(true);
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for I2C to settle
    bool charge_enabled_after_display = bsp_get_charge_en();
    ESP_LOGI(TAG, "Battery charging after display init: %s (verified: %s)", 
             charge_enabled_after_display ? "YES" : "NO",
             charge_enabled_after_display ? "OK" : "FAILED!");
    
    // Turn on display
    ESP_LOGI(TAG, "Turning on display...");
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on display: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Display turned on");
    
    // Initialize brightness
    ESP_LOGI(TAG, "Initializing brightness...");
    ret = bsp_display_brightness_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Brightness init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Brightness initialized");
        bsp_display_brightness_set(80);  // 80% brightness
        ESP_LOGI(TAG, "Brightness set to 80%%");
    }
    
    // Allocate framebuffer in PSRAM
    size_t fb_size = DISPLAY_PHYSICAL_WIDTH * DISPLAY_PHYSICAL_HEIGHT * sizeof(uint16_t);
    ESP_LOGI(TAG, "Allocating framebuffer: %zu bytes (%.2f MB)", 
             fb_size, fb_size / (1024.0 * 1024.0));
    
    framebuffer = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    if (!framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer in PSRAM!");
        return;
    }
    ESP_LOGI(TAG, "Framebuffer allocated at: %p", framebuffer);
    
    // Main loop: read battery and update display
    ESP_LOGI(TAG, "Starting battery test loop...");
    
    uint32_t update_count = 0;
    while (1) {
        update_count++;
        
        // Read battery status
        battery_status_t status = {};
        status.level = -1;
        status.voltage_mv = -1;
        status.is_charging = false;
        status.initialized = false;
        ret = battery_monitor_read(&status);
        
        if (ret == ESP_OK) {
            // Read current directly from shunt (more reliable)
            float current_ma = 0.0f;
            int shunt_uv = 0;
            esp_err_t current_err = battery_monitor_get_current_ma(&current_ma);
            esp_err_t shunt_err = battery_monitor_get_shunt_voltage_uv(&shunt_uv);
            
            if (current_err != ESP_OK || shunt_err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to read shunt/current: shunt=%s current=%s", 
                         esp_err_to_name(shunt_err), esp_err_to_name(current_err));
                current_ma = 0.0f;
                shunt_uv = 0;
            }
            
            // Check USB-C presence (using charging status - simpler and more reliable)
            bool usb_present = detect_usb_present(status.voltage_mv, current_ma, status.is_charging);
            
            // Use battery_present from status (determined by voltage classification voting in battery_monitor)
            bool battery_present = status.battery_present;
            
            // Update display
            update_display(&status, current_ma, usb_present, battery_present);
            
            // Log in simple format: usb=0/1 bus_mv=... shunt_uv=... current_ma=...
            ESP_LOGI(TAG, "usb=%d bus_mv=%d shunt_uv=%d current_ma=%.1f",
                     usb_present ? 1 : 0,
                     status.voltage_mv,
                     shunt_uv,
                     current_ma);
        } else {
            ESP_LOGW(TAG, "Failed to read battery status: %s", esp_err_to_name(ret));
        }
        
        // Update every 100ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
