#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp/m5stack_tab5.h"
#include "bsp/display.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "usb/hid.h"

static const char *TAG = "USB_GAMEPAD_DISPLAY";

// Display configuration - physical display dimensions (portrait orientation)
// Rotation 90° left is done via MADCTL command in the driver
#define DISPLAY_WIDTH  720
#define DISPLAY_HEIGHT 1280

// Colors (RGB565)
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY    0x8410
#define COLOR_DARKGRAY 0x4208
#define COLOR_ORANGE  0xFC00

// Display handles
static bsp_lcd_handles_t lcd_handles;
static esp_lcd_panel_handle_t panel_handle = NULL;
static uint16_t* framebuffer = NULL;
static bool display_initialized = false;

// Gamepad state structure
typedef struct {
    uint16_t buttons;
    uint8_t  dpad;
    int8_t   left_x;
    int8_t   left_y;
    int8_t   right_x;
    int8_t   right_y;
    uint8_t  left_trigger;
    uint8_t  right_trigger;
    uint8_t  raw_report[64];
    size_t   raw_report_len;
} gamepad_state_t;

static gamepad_state_t s_gamepad_state = {};
static bool s_gamepad_connected = false;
static usb_host_client_handle_t s_usb_client_handle = NULL;
static uint16_t s_gamepad_vid = 0;
static uint16_t s_gamepad_pid = 0;

// Display update throttling
static bool display_update_pending = false;
static TickType_t last_display_update = 0;
static bool first_display_update = true;  // Flag for first update
#define DISPLAY_UPDATE_INTERVAL_MS 100  // Minimum 100ms between updates

// Track previous state for change detection
static uint16_t s_last_buttons = 0xFFFF;
static uint8_t s_last_dpad = 0xFF;
static bool s_first_report = true;

// Button names (PS5 naming)
static const char* button_names[] = {
    "Square", "Cross", "Circle", "Triangle", "L1", "R1", "Create", "Options",
    "L3", "R3", "PS", "L2", "R2"
};

// D-Pad direction names
static const char* dpad_names[] = {
    "CENTER", "UP", "UP-RIGHT", "RIGHT", "DOWN-RIGHT",
    "DOWN", "DOWN-LEFT", "LEFT", "UP-LEFT"
};

// Simple 8x8 bitmap font (ASCII 32-126)
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

// Display functions
// Rotation 90° left is done in hardware via MADCTL command in the driver
// Coordinates are used directly for portrait orientation (720x1280)
// Display rotates the image itself, so we draw as if framebuffer is in portrait orientation
// But to use the full screen in landscape orientation (1280x720), we need to use coordinates correctly
// Formula: for landscape position (ax, ay) we use portrait position (px, py) where:
//   px = 720 - ay - 1  (vertical position in landscape becomes horizontal in portrait, inverted)
//   py = ax             (horizontal position in landscape becomes vertical in portrait)
static void draw_char(uint16_t* fb, int ax, int ay, char c, uint16_t color) {
    if (c < 32 || c > 126) c = '?';
    int idx = c - 32;
    if (idx < 0 || idx >= 95) idx = 0;
    
    const uint8_t* char_data = font_8x8[idx];
    
    // Convert landscape coordinates to portrait for framebuffer
    // ax, ay - coordinates in landscape orientation (0-1279, 0-719)
    // px, py - coordinates in portrait orientation for framebuffer (0-719, 0-1279)
    
    for (int py = 0; py < 8; py++) {
        uint8_t row = char_data[py];
        for (int px = 0; px < 8; px++) {
            if (row & (1 << px)) {
                // Landscape pixel coordinates
                int a_px = ax + px;
                int a_py = ay + py;
                
                // Convert to portrait coordinates for framebuffer
                // Rotation 90° left: (x, y) -> (y, height - x - 1)
                int p_x = a_py;  // Landscape Y becomes portrait X
                int p_y = DISPLAY_HEIGHT - a_px - 1;  // Landscape X inverted becomes portrait Y
                
                if (p_x >= 0 && p_x < DISPLAY_WIDTH && p_y >= 0 && p_y < DISPLAY_HEIGHT) {
                    fb[p_y * DISPLAY_WIDTH + p_x] = color;
                }
            }
        }
    }
}

static void draw_string(uint16_t* fb, int x, int y, const char* str, uint16_t color) {
    int cx = x;
    for (const char* p = str; *p; p++) {
        draw_char(fb, cx, y, *p, color);
        cx += 9; // 8 pixels + 1 spacing
    }
}

// Draw enlarged character (2x scale)
static void draw_char_2x(uint16_t* fb, int ax, int ay, char c, uint16_t color) {
    if (c < 32 || c > 126) c = '?';
    int idx = c - 32;
    if (idx < 0 || idx >= 95) idx = 0;
    
    const uint8_t* char_data = font_8x8[idx];
    
    // Draw each font pixel as a 2x2 square
    for (int py = 0; py < 8; py++) {
        uint8_t row = char_data[py];
        for (int px = 0; px < 8; px++) {
            if (row & (1 << px)) {
                // Landscape pixel coordinates (scaled 2x)
                int a_px = ax + px * 2;
                int a_py = ay + py * 2;
                
                // Draw 2x2 square
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        int a_fx = a_px + dx;
                        int a_fy = a_py + dy;
                        
                        // Convert to portrait coordinates for framebuffer
                        int p_x = a_fy;
                        int p_y = DISPLAY_HEIGHT - a_fx - 1;
                        
                        if (p_x >= 0 && p_x < DISPLAY_WIDTH && p_y >= 0 && p_y < DISPLAY_HEIGHT) {
                            fb[p_y * DISPLAY_WIDTH + p_x] = color;
                        }
                    }
                }
            }
        }
    }
}

// Draw enlarged string (2x scale)
static void draw_string_2x(uint16_t* fb, int x, int y, const char* str, uint16_t color) {
    int cx = x;
    for (const char* p = str; *p; p++) {
        draw_char_2x(fb, cx, y, *p, color);
        cx += 18; // 16 pixels (8*2) + 2 spacing
    }
}

static void fill_rect(uint16_t* fb, int ax, int ay, int aw, int ah, uint16_t color) {
    // ax, ay - coordinates in landscape orientation (0-1279, 0-719)
    // aw, ah - dimensions in landscape orientation
    for (int a_py = 0; a_py < ah; a_py++) {
        for (int a_px = 0; a_px < aw; a_px++) {
            int a_fx = ax + a_px;
            int a_fy = ay + a_py;
            
            // Convert to portrait coordinates for framebuffer
            int p_x = a_fy;  // Landscape Y becomes portrait X
            int p_y = DISPLAY_HEIGHT - a_fx - 1;  // Landscape X inverted becomes portrait Y
            
            if (p_x >= 0 && p_x < DISPLAY_WIDTH && p_y >= 0 && p_y < DISPLAY_HEIGHT) {
                fb[p_y * DISPLAY_WIDTH + p_x] = color;
            }
        }
    }
}

static void draw_button(uint16_t* fb, int x, int y, const char* name, bool pressed) {
    int w = 90;
    int h = 35;
    
    // Button background - brighter when pressed
    uint16_t bg_color = pressed ? COLOR_GREEN : COLOR_DARKGRAY;
    fill_rect(fb, x, y, w, h, bg_color);
    
    // Button border - thicker when pressed
    int border_width = pressed ? 3 : 2;
    uint16_t border_color = pressed ? COLOR_YELLOW : COLOR_WHITE;
    
    fill_rect(fb, x, y, w, border_width, border_color);
    fill_rect(fb, x, y + h - border_width, w, border_width, border_color);
    fill_rect(fb, x, y, border_width, h, border_color);
    fill_rect(fb, x + w - border_width, y, border_width, h, border_color);
    
    // Button text (centered) - black text on green background when pressed
    int text_x = x + (w - strlen(name) * 9) / 2;
    int text_y = y + (h - 8) / 2;
    uint16_t text_color = pressed ? COLOR_BLACK : COLOR_WHITE;
    draw_string(fb, text_x, text_y, name, text_color);
}

static void draw_stick(uint16_t* fb, int ax, int ay, int sx, int sy, const char* label) {
    // ax, ay - coordinates in landscape orientation (0-1279, 0-719)
    int center_ax = ax + 45;
    int center_ay = ay + 45;
    int radius = 35;
    
    // Draw circle background
    for (int a_py = -radius; a_py <= radius; a_py++) {
        for (int a_px = -radius; a_px <= radius; a_px++) {
            int dist_sq = a_px * a_px + a_py * a_py;
            if (dist_sq <= radius * radius) {
                int a_fx = center_ax + a_px;
                int a_fy = center_ay + a_py;
                
                // Convert to portrait coordinates for framebuffer
                int p_x = a_fy;
                int p_y = DISPLAY_HEIGHT - a_fx - 1;
                
                if (p_x >= 0 && p_x < DISPLAY_WIDTH && p_y >= 0 && p_y < DISPLAY_HEIGHT) {
                    fb[p_y * DISPLAY_WIDTH + p_x] = COLOR_DARKGRAY;
                }
            }
        }
    }
    
    // Draw center crosshair
    fill_rect(fb, center_ax - 1, center_ay - radius, 2, radius * 2, COLOR_GRAY);
    fill_rect(fb, center_ax - radius, center_ay - 1, radius * 2, 2, COLOR_GRAY);
    
    // Draw stick position
    int stick_ax = center_ax + (sx * radius / 127);
    int stick_ay = center_ay + (sy * radius / 127);
    fill_rect(fb, stick_ax - 4, stick_ay - 4, 8, 8, COLOR_GREEN);
    
    // Draw label
    draw_string(fb, ax, ay - 12, label, COLOR_WHITE);
}

static void draw_trigger(uint16_t* fb, int x, int y, uint8_t value, const char* label) {
    int w = 180;
    int h = 25;
    
    // Background
    fill_rect(fb, x, y, w, h, COLOR_DARKGRAY);
    
    // Filled portion
    int filled_w = (value * w) / 255;
    if (filled_w > 0) {
        fill_rect(fb, x, y, filled_w, h, COLOR_GREEN);
    }
    
    // Border
    fill_rect(fb, x, y, w, 2, COLOR_WHITE);
    fill_rect(fb, x, y + h - 2, w, 2, COLOR_WHITE);
    fill_rect(fb, x, y, 2, h, COLOR_WHITE);
    fill_rect(fb, x + w - 2, y, 2, h, COLOR_WHITE);
    
    // Label
    draw_string(fb, x, y - 12, label, COLOR_WHITE);
    
    // Value text
    char value_str[16];
    snprintf(value_str, sizeof(value_str), "%d", value);
    draw_string(fb, x + w + 8, y + 8, value_str, COLOR_WHITE);
}

static esp_err_t display_init(void) {
    if (display_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing display...");
    
    bsp_display_config_t display_config = {};
    esp_err_t ret = bsp_display_new_with_handles_to_st7123(&display_config, &lcd_handles);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display: %s", esp_err_to_name(ret));
        return ret;
    }
    
    panel_handle = lcd_handles.panel;
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle is NULL");
        return ESP_FAIL;
    }
    
    // Turn on display
    ret = esp_lcd_panel_disp_on_off(panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to turn on display: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize brightness (as in working example display_test_simple)
    ESP_LOGI(TAG, "Initializing brightness...");
    ret = bsp_display_brightness_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Brightness init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Brightness initialized");
        bsp_display_brightness_set(80);  // 80% brightness (normal brightness)
        ESP_LOGI(TAG, "Brightness set to 100%%");
    }
    
    // Allocate framebuffer in PSRAM (as in working example)
    size_t fb_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_LOGI(TAG, "Allocating framebuffer: %zu bytes (%.2f MB)", 
             fb_size, fb_size / (1024.0 * 1024.0));
    
    framebuffer = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    if (!framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer in PSRAM!");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Framebuffer allocated in PSRAM at: %p", framebuffer);
    
    display_initialized = true;
    ESP_LOGI(TAG, "Display initialized: %dx%d (will be rotated 90° left via MADCTL)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    
    return ESP_OK;
}

static void display_update_gamepad_state(const gamepad_state_t* state) {
    if (!display_initialized || !framebuffer || !state) {
        ESP_LOGW(TAG, "Display update skipped: initialized=%d, framebuffer=%p, state=%p", 
                 display_initialized, framebuffer, state);
        return;
    }
    
    // Check: don't update too frequently (but skip check on first call)
    TickType_t now = xTaskGetTickCount();
    if (!first_display_update) {  // If this is not the first update
        if (display_update_pending || (now - last_display_update < pdMS_TO_TICKS(DISPLAY_UPDATE_INTERVAL_MS))) {
            return;  // Skip update
        }
    }
    
    if (first_display_update) {
        ESP_LOGI(TAG, "First display update - forcing through");
    }
    
    display_update_pending = true;
    last_display_update = now;
    first_display_update = false;  // After first update, set flag
    
    // Clear screen
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
        framebuffer[i] = COLOR_BLACK;
    }
    
    // ===== HEADER =====
    // Coordinates in landscape orientation (1280x720) - use full screen!
    // Use enlarged text (2x scale)
    draw_string_2x(framebuffer, 10, 10, "PS5 DualSense Gamepad Test", COLOR_YELLOW);
    
    // Line under header (use full screen width)
    fill_rect(framebuffer, 10, 36, 1260, 2, COLOR_YELLOW);
    
    if (!s_gamepad_connected) {
        draw_string_2x(framebuffer, 10, 50, "Waiting for gamepad...", COLOR_RED);
        draw_string_2x(framebuffer, 10, 70, "Connect PS5 controller to USB-A", COLOR_GRAY);
        // Continue to main display update at end of function
    }
    
    // ===== GAMEPAD INFORMATION =====
    // Check for PS5 DualSense
    bool is_ps5 = (s_gamepad_vid == 0x054C && s_gamepad_pid == 0x0CE6);
    
    if (is_ps5) {
        draw_string_2x(framebuffer, 10, 50, "Controller: PS5 DualSense", COLOR_GREEN);
    } else {
        draw_string_2x(framebuffer, 10, 50, "Controller: Generic USB HID", COLOR_CYAN);
    }
    
    char info_str[64];
    snprintf(info_str, sizeof(info_str), "VID: 0x%04X  PID: 0x%04X", s_gamepad_vid, s_gamepad_pid);
    draw_string_2x(framebuffer, 10, 70, info_str, COLOR_CYAN);
    
    // ===== BUTTONS WITH PRESS VISUALIZATION =====
    // Arrange buttons horizontally in landscape orientation
    // Shifted down due to enlarged header text and controller information
    int btn_x = 10;
    int btn_y = 120;  // Shifted down due to enlarged text
    
    // First row: Face buttons (PS5 names) - enlarged text
    draw_string_2x(framebuffer, btn_x, btn_y - 20, "Face Buttons:", COLOR_WHITE);
    draw_button(framebuffer, btn_x, btn_y, "Square", (state->buttons & (1 << 0)) != 0);
    draw_button(framebuffer, btn_x + 100, btn_y, "Cross", (state->buttons & (1 << 1)) != 0);
    draw_button(framebuffer, btn_x + 200, btn_y, "Circle", (state->buttons & (1 << 2)) != 0);
    draw_button(framebuffer, btn_x + 300, btn_y, "Triangle", (state->buttons & (1 << 3)) != 0);
    
    // Second row: Shoulder buttons - enlarged text
    btn_y += 60;  // Increased spacing between button rows
    draw_string_2x(framebuffer, btn_x, btn_y - 20, "Shoulder:", COLOR_WHITE);
    draw_button(framebuffer, btn_x, btn_y, "L1", (state->buttons & (1 << 4)) != 0);
    draw_button(framebuffer, btn_x + 100, btn_y, "R1", (state->buttons & (1 << 5)) != 0);
    draw_button(framebuffer, btn_x + 200, btn_y, "Create", (state->buttons & (1 << 6)) != 0);
    draw_button(framebuffer, btn_x + 300, btn_y, "Options", (state->buttons & (1 << 7)) != 0);
    
    // Third row: Stick buttons and PS button - enlarged text
    btn_y += 60;  // Increased spacing between button rows
    draw_string_2x(framebuffer, btn_x, btn_y - 20, "Sticks & PS:", COLOR_WHITE);
    draw_button(framebuffer, btn_x, btn_y, "L3", (state->buttons & (1 << 8)) != 0);
    draw_button(framebuffer, btn_x + 100, btn_y, "R3", (state->buttons & (1 << 9)) != 0);
    draw_button(framebuffer, btn_x + 200, btn_y, "PS", (state->buttons & (1 << 10)) != 0);
    
    // ===== D-PAD =====
    btn_y += 60;  // Increased spacing between button rows
    char dpad_str[32];
    const char* dpad_name = dpad_names[state->dpad];
    snprintf(dpad_str, sizeof(dpad_str), "D-Pad: %s", dpad_name);
    
    // Highlight D-Pad if not CENTER - enlarged text
    uint16_t dpad_color = (state->dpad == 0) ? COLOR_GRAY : COLOR_GREEN;
    draw_string_2x(framebuffer, btn_x, btn_y, dpad_str, dpad_color);
    
    // ===== STICKS =====
    // Arrange sticks horizontally in landscape orientation
    draw_stick(framebuffer, 800, 200, state->left_x, state->left_y, "L Stick");
    draw_stick(framebuffer, 1000, 200, state->right_x, state->right_y, "R Stick");
    
    // ===== TRIGGERS =====
    // Arrange triggers horizontally in landscape orientation
    draw_trigger(framebuffer, 800, 400, state->left_trigger, "L2");
    draw_trigger(framebuffer, 1000, 400, state->right_trigger, "R2");
    
    // ===== PRESS STATUS (bottom of screen) =====
    btn_y = 640;  // Bottom of landscape orientation (y=640-720)
    fill_rect(framebuffer, 0, btn_y, 1280, 80, COLOR_DARKGRAY);
    
    // Count pressed buttons
    int pressed_count = 0;
    for (int i = 0; i < 13; i++) {
        if (state->buttons & (1 << i)) {
            pressed_count++;
        }
    }
    
    char status_str[64];
    snprintf(status_str, sizeof(status_str), "Buttons pressed: %d", pressed_count);
    draw_string_2x(framebuffer, 20, btn_y + 10, status_str, COLOR_WHITE);
    
    // List of pressed buttons
    if (pressed_count > 0) {
        char pressed_list[128] = "";
        int pos = 0;
        for (int i = 0; i < 13 && pos < sizeof(pressed_list) - 20; i++) {
            if (state->buttons & (1 << i)) {
                int len = snprintf(pressed_list + pos, sizeof(pressed_list) - pos, "%s ", button_names[i]);
                if (len > 0) pos += len;
            }
        }
        draw_string_2x(framebuffer, 20, btn_y + 30, pressed_list, COLOR_GREEN);
    } else {
        draw_string_2x(framebuffer, 20, btn_y + 30, "No buttons pressed", COLOR_GRAY);
    }
    
    // Update display (as in working example display_test_simple)
    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, framebuffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display update FAILED: %s (0x%x)", esp_err_to_name(ret), ret);
    }
    display_update_pending = false;
}

// Gamepad parsing functions (from usb_gamepad_test)
static void parse_gamepad_report(const uint8_t* data, size_t len, gamepad_state_t* state)
{
    if (len < 4) {
        return;
    }
    
    // Sony DualSense (PS5) USB HID (VID: 0x054C, PID: 0x0CE6)
    if (s_gamepad_vid == 0x054C && s_gamepad_pid == 0x0CE6) {
        if (len < 11) {
            return;
        }
        
        int base = 0;
        if (data[0] == 0x01) {
            base = 1;
        } else {
            return;
        }
        
        state->left_x   = (int16_t)((int)data[base + 0] - 128);
        state->left_y   = (int16_t)((int)data[base + 1] - 128);
        state->right_x  = (int16_t)((int)data[base + 2] - 128);
        state->right_y  = (int16_t)((int)data[base + 3] - 128);
        
        state->left_trigger  = data[base + 4];
        state->right_trigger = data[base + 5];
        
        uint8_t b7  = data[base + 7];
        uint8_t hat = b7 & 0x0F;
        
        if (hat <= 7) {
            state->dpad = hat + 1;
        } else {
            state->dpad = 0;
        }
        
        state->buttons = 0;
        
        if (b7 & 0x10) state->buttons |= (1 << 0);
        if (b7 & 0x20) state->buttons |= (1 << 1);
        if (b7 & 0x40) state->buttons |= (1 << 2);
        if (b7 & 0x80) state->buttons |= (1 << 3);
        
        uint8_t b8 = data[base + 8];
        if (b8 & 0x01) state->buttons |= (1 << 4);
        if (b8 & 0x02) state->buttons |= (1 << 5);
        if (b8 & 0x04) state->buttons |= (1 << 11);
        if (b8 & 0x08) state->buttons |= (1 << 12);
        if (b8 & 0x10) state->buttons |= (1 << 6);
        if (b8 & 0x20) state->buttons |= (1 << 7);
        if (b8 & 0x40) state->buttons |= (1 << 8);
        if (b8 & 0x80) state->buttons |= (1 << 9);
        
        uint8_t b9 = data[base + 9];
        if (b9 & 0x01) state->buttons |= (1 << 10);
        
        if (state->left_trigger  > 30) state->buttons |= (1 << 11);
        if (state->right_trigger > 30) state->buttons |= (1 << 12);
        
        memcpy(state->raw_report, data, len < 64 ? len : 64);
        state->raw_report_len = len;
        return;
    } else {
        // Generic gamepad format
        state->buttons = data[0] | (data[1] << 8);
        
        if (len >= 3) {
            uint8_t dpad_val = data[2];
            if (dpad_val <= 8) {
                state->dpad = dpad_val;
                if (len >= 9) {
                    state->left_x = (int8_t)data[3];
                    state->left_y = (int8_t)data[4];
                    state->right_x = (int8_t)data[5];
                    state->right_y = (int8_t)data[6];
                    state->left_trigger = data[7];
                    state->right_trigger = data[8];
                }
            } else {
                state->dpad = 0;
                if (len >= 8) {
                    state->left_x = (int8_t)data[2];
                    state->left_y = (int8_t)data[3];
                    state->right_x = (int8_t)data[4];
                    state->right_y = (int8_t)data[5];
                    state->left_trigger = data[6];
                    state->right_trigger = data[7];
                }
            }
        }
        
        memcpy(state->raw_report, data, len < 64 ? len : 64);
        state->raw_report_len = len;
    }
}

// USB Host client event callback
static void usb_host_event_callback(const usb_host_client_event_msg_t* event_msg, void* arg)
{
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "USB Host: New device connected on address %d", event_msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGI(TAG, "USB Host: Device disconnected");
            break;
        default:
            break;
    }
}

// USB Host client event handler task
static void usb_host_client_task(void* arg)
{
    while (1) {
        if (s_usb_client_handle) {
            usb_host_client_handle_events(s_usb_client_handle, portMAX_DELAY);
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// HID Interface event callback
static void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                        const hid_host_interface_event_t event,
                                        void* arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));
    
    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
            uint8_t data[64] = {0};
            size_t data_length = 0;
            
            esp_err_t ret = hid_host_device_get_raw_input_report_data(
                hid_device_handle, data, sizeof(data), &data_length);
            
            if (ret == ESP_OK && data_length > 0) {
                // Log first few reports for diagnostics
                static int report_count = 0;
                if (report_count < 5) {
                    ESP_LOGI(TAG, "HID Report #%d: length=%zu, data[0..10]=%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                             report_count, data_length,
                             data[0], data[1], data[2], data[3], data[4],
                             data[5], data[6], data[7], data[8], data[9], data[10]);
                    report_count++;
                }
                
                parse_gamepad_report(data, data_length, &s_gamepad_state);
                
                if (s_first_report) {
                    ESP_LOGI(TAG, "First report parsed: buttons=0x%04X, dpad=%d, L(%d,%d) R(%d,%d) L2=%d R2=%d",
                             s_gamepad_state.buttons, s_gamepad_state.dpad,
                             s_gamepad_state.left_x, s_gamepad_state.left_y,
                             s_gamepad_state.right_x, s_gamepad_state.right_y,
                             s_gamepad_state.left_trigger, s_gamepad_state.right_trigger);
                    s_last_buttons = s_gamepad_state.buttons;
                    s_last_dpad = s_gamepad_state.dpad;
                    s_first_report = false;
                    return;
                }
                
                uint16_t button_changes = s_gamepad_state.buttons ^ s_last_buttons;
                bool dpad_changed = (s_gamepad_state.dpad != s_last_dpad);
                
                if (button_changes || dpad_changed) {
                    ESP_LOGI(TAG, "Input changed: buttons=0x%04X (changed=0x%04X), dpad=%d, L(%d,%d) R(%d,%d) L2=%d R2=%d",
                             s_gamepad_state.buttons, button_changes, s_gamepad_state.dpad,
                             s_gamepad_state.left_x, s_gamepad_state.left_y,
                             s_gamepad_state.right_x, s_gamepad_state.right_y,
                             s_gamepad_state.left_trigger, s_gamepad_state.right_trigger);
                    s_last_buttons = s_gamepad_state.buttons;
                    s_last_dpad = s_gamepad_state.dpad;
                }
                
                // Do NOT update display here - main loop will do it
                // display_update_gamepad_state(&s_gamepad_state);  // REMOVED
            } else {
                ESP_LOGW(TAG, "Failed to read HID report: ret=%s, length=%zu", 
                         esp_err_to_name(ret), data_length);
            }
            break;
        }
        
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Gamepad DISCONNECTED");
            ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
            s_gamepad_connected = false;
            memset(&s_gamepad_state, 0, sizeof(s_gamepad_state));
            s_gamepad_vid = 0;
            s_gamepad_pid = 0;
            s_first_report = true;
            // Display will update in main loop
            break;
            
        default:
            break;
    }
}

// HID Device event callback
static void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                                  const hid_host_driver_event_t event,
                                  void* arg)
{
    hid_host_dev_params_t dev_params;
    esp_err_t ret = hid_host_device_get_params(hid_device_handle, &dev_params);
    if (ret != ESP_OK) {
        return;
    }
    
    switch (event) {
        case HID_HOST_DRIVER_EVENT_CONNECTED: {
            hid_host_dev_info_t dev_info;
            ret = hid_host_get_device_info(hid_device_handle, &dev_info);
            
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Gamepad CONNECTED");
                ESP_LOGI(TAG, "  VID: 0x%04X", dev_info.VID);
                ESP_LOGI(TAG, "  PID: 0x%04X", dev_info.PID);
                
                s_gamepad_vid = dev_info.VID;
                s_gamepad_pid = dev_info.PID;
            }
            
            const hid_host_device_config_t dev_config = {
                .callback = hid_host_interface_callback,
                .callback_arg = NULL
            };
            
            ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
            ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
            
            s_gamepad_connected = true;
            ESP_LOGI(TAG, "Gamepad ready!");
            // Display will update in main loop
            break;
        }
        
        default:
            break;
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "USB Gamepad Display Test for M5Stack Tab5");
    ESP_LOGI(TAG, "==========================================");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize display (BSP initializes I2C and IO-expander inside bsp_display_new_with_handles_to_st7123)
    ret = display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display initialization failed!");
        return;
    }
    
    // Start USB Host
    ESP_ERROR_CHECK(bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true));
    
    // Enable USB-A port power
    bsp_set_usb_5v_en(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Enable USB Host root port power
    ret = usb_host_lib_set_root_port_power(true);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "USB Host root port power already enabled");
    }
    
    // Register USB Host client
    usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = usb_host_event_callback,
            .callback_arg = NULL
        }
    };
    
    ret = usb_host_client_register(&client_config, &s_usb_client_handle);
    if (ret == ESP_OK) {
        xTaskCreate(usb_host_client_task, "usb_host_client", 4096, NULL, 5, NULL);
    }
    
    // Install HID Host driver
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_event,
        .callback_arg = NULL
    };
    
    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
    
    ESP_LOGI(TAG, "Waiting for USB gamepad to be connected...");
    ESP_LOGI(TAG, "Connect gamepad to USB-A port on Tab5");
    
    // Force display update immediately after initialization
    // Reset throttling for first update
    first_display_update = true;
    display_update_pending = false;
    last_display_update = 0;
    
    // Main loop - update display periodically
    while (1) {
        // Always update display (shows either waiting or gamepad state)
        display_update_gamepad_state(&s_gamepad_state);
        vTaskDelay(pdMS_TO_TICKS(200)); // Update display at ~5 FPS
    }
}

