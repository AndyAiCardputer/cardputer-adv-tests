#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
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

static const char *TAG = "USB_KBD";

// Display: physical portrait 720x1280, used as landscape 1280x720
#define DISPLAY_WIDTH  720
#define DISPLAY_HEIGHT 1280
#define LAND_W 1280
#define LAND_H 720

// RGB565 colors
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_RED     0xF800
#define C_GREEN   0x07E0
#define C_BLUE    0x001F
#define C_YELLOW  0xFFE0
#define C_CYAN    0x07FF
#define C_ORANGE  0xFC00
#define C_GRAY    0x8410
#define C_DKGRAY  0x4208
#define C_LTGRAY  0xC618
#define C_DKGREEN 0x03E0

// Display state
static bsp_lcd_handles_t lcd_handles;
static esp_lcd_panel_handle_t panel_handle = NULL;
static uint16_t* framebuffer = NULL;
static bool display_ok = false;

// ---------------------------------------------------------------------------
// 8x8 bitmap font (ASCII 32-126)
// ---------------------------------------------------------------------------
static const uint8_t font_8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // Space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // #
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // &
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // (
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x0C,0x0C,0x7F,0x0C,0x0C,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x06,0x00}, // ,
    {0x00,0x00,0x00,0x7F,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // .
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // /
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x63,0x3E}, // 0
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x3F}, // 1
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x33,0x3F}, // 2
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x33,0x1E}, // 3
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x30,0x78}, // 4
    {0x3F,0x03,0x03,0x1F,0x30,0x30,0x33,0x1E}, // 5
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x33,0x1E}, // 6
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x0C}, // 7
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x33,0x1E}, // 8
    {0x1E,0x33,0x33,0x33,0x3E,0x30,0x18,0x0E}, // 9
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // :
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x06,0x00}, // ;
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // <
    {0x00,0x00,0x7F,0x00,0x00,0x7F,0x00,0x00}, // =
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // >
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // ?
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // @
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // A
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // B
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // C
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // D
    {0x7F,0x06,0x06,0x3E,0x06,0x06,0x7F,0x00}, // E
    {0x7F,0x06,0x06,0x3E,0x06,0x06,0x06,0x00}, // F
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // G
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // H
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // I
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // J
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // K
    {0x06,0x06,0x06,0x06,0x06,0x06,0x7F,0x00}, // L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // O
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x06,0x00}, // P
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // Q
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // R
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // S
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // T
    {0x33,0x33,0x33,0x33,0x33,0x33,0x1E,0x00}, // U
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // X
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // Y
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // Z
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // [
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // backslash
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // a
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // b
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // c
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // d
    {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00}, // e
    {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, // f
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // g
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // h
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // i
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // j
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // k
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // l
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // n
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // o
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // p
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // q
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // r
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // s
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // t
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // u
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // w
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // x
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // y
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // z
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // {
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // |
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // }
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
};

// ---------------------------------------------------------------------------
// Drawing primitives (landscape coords -> portrait framebuffer)
// ---------------------------------------------------------------------------
static inline void put_pixel(int ax, int ay, uint16_t color)
{
    int px = ay;
    int py = DISPLAY_HEIGHT - ax - 1;
    if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
        framebuffer[py * DISPLAY_WIDTH + px] = color;
    }
}

static void fill_rect(int ax, int ay, int aw, int ah, uint16_t color)
{
    for (int dy = 0; dy < ah; dy++) {
        int py_row = ay + dy;
        for (int dx = 0; dx < aw; dx++) {
            put_pixel(ax + dx, py_row, color);
        }
    }
}

static void draw_char_scaled(int ax, int ay, char c, uint16_t color, int scale)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t* glyph = font_8x8[c - 32];
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = glyph[gy];
        for (int gx = 0; gx < 8; gx++) {
            if (row & (1 << gx)) {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        put_pixel(ax + gx * scale + sx, ay + gy * scale + sy, color);
            }
        }
    }
}

static void draw_str(int x, int y, const char* s, uint16_t color, int scale)
{
    int cx = x;
    int spacing = 8 * scale + scale;
    for (; *s; s++) {
        draw_char_scaled(cx, y, *s, color, scale);
        cx += spacing;
    }
}

static int str_px_width(const char* s, int scale)
{
    int len = strlen(s);
    return len * (8 * scale + scale) - (len > 0 ? scale : 0);
}

// ---------------------------------------------------------------------------
// Scancode -> ASCII (US QWERTY)
// ---------------------------------------------------------------------------
static const char sc_normal[] = {
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p',
    'q','r','s','t','u','v','w','x','y','z',
    '1','2','3','4','5','6','7','8','9','0',
    '\n',0x1B,'\b','\t',' ','-','=','[',']','\\','#',';','\'','`',',','.','/'
};
static const char sc_shift[] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z',
    '!','@','#','$','%','^','&','*','(',')',
    '\n',0x1B,'\b','\t',' ','_','+','{','}','|','~',':','"','~','<','>','?'
};
#define SC_START 0x04
#define SC_END   0x38

static const char* special_key_name(uint8_t sc)
{
    switch (sc) {
        case 0x28: return "ENTER";
        case 0x29: return "ESC";
        case 0x2A: return "BKSP";
        case 0x2B: return "TAB";
        case 0x2C: return "SPACE";
        case 0x39: return "CAPS";
        case 0x3A: return "F1";   case 0x3B: return "F2";
        case 0x3C: return "F3";   case 0x3D: return "F4";
        case 0x3E: return "F5";   case 0x3F: return "F6";
        case 0x40: return "F7";   case 0x41: return "F8";
        case 0x42: return "F9";   case 0x43: return "F10";
        case 0x44: return "F11";  case 0x45: return "F12";
        case 0x46: return "PRTSC"; case 0x47: return "SCRLK";
        case 0x48: return "PAUSE"; case 0x49: return "INS";
        case 0x4A: return "HOME"; case 0x4B: return "PGUP";
        case 0x4C: return "DEL";  case 0x4D: return "END";
        case 0x4E: return "PGDN";
        case 0x4F: return "RIGHT"; case 0x50: return "LEFT";
        case 0x51: return "DOWN";  case 0x52: return "UP";
        default:   return NULL;
    }
}

// Modifier bit masks
#define MOD_LCTRL  0x01
#define MOD_LSHIFT 0x02
#define MOD_LALT   0x04
#define MOD_LGUI   0x08
#define MOD_RCTRL  0x10
#define MOD_RSHIFT 0x20
#define MOD_RALT   0x40
#define MOD_RGUI   0x80

// ---------------------------------------------------------------------------
// Keyboard state
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t modifier;
    uint8_t keys[6];
} kbd_state_t;

static kbd_state_t s_cur = {};
static kbd_state_t s_prev = {};
static bool s_connected = false;
static bool s_first_report = true;
static usb_host_client_handle_t s_usb_client = NULL;
static uint16_t s_vid = 0, s_pid = 0;

// Telemetry
static uint32_t s_total_keys = 0;
static char s_last_key_name[32] = "";
static uint8_t s_last_scancode = 0;
static int64_t s_last_press_time_us = 0;
static uint32_t s_last_hold_ms = 0;

// Text buffer (typed text)
#define TEXT_BUF_SIZE 256
static char s_text_buf[TEXT_BUF_SIZE] = "";
static int s_text_len = 0;

// WPM tracking: ring buffer of keystroke timestamps
#define WPM_WINDOW_SEC 10
#define WPM_MAX_STAMPS 200
static int64_t s_wpm_stamps[WPM_MAX_STAMPS];
static int s_wpm_head = 0;
static int s_wpm_count = 0;

static float calc_wpm(void)
{
    if (s_wpm_count == 0) return 0.0f;
    int64_t now = esp_timer_get_time();
    int64_t cutoff = now - (int64_t)WPM_WINDOW_SEC * 1000000;
    int valid = 0;
    for (int i = 0; i < s_wpm_count; i++) {
        int idx = (s_wpm_head - s_wpm_count + i + WPM_MAX_STAMPS) % WPM_MAX_STAMPS;
        if (s_wpm_stamps[idx] >= cutoff) valid++;
    }
    return (valid / 5.0f) * (60.0f / WPM_WINDOW_SEC);
}

static void record_keystroke(void)
{
    s_wpm_stamps[s_wpm_head] = esp_timer_get_time();
    s_wpm_head = (s_wpm_head + 1) % WPM_MAX_STAMPS;
    if (s_wpm_count < WPM_MAX_STAMPS) s_wpm_count++;
}

// Per-key press start times (for hold duration)
static int64_t s_key_press_start[256] = {};

// Get readable name for a scancode
static void get_key_label(uint8_t sc, uint8_t mod, char* buf, size_t buflen)
{
    const char* sp = special_key_name(sc);
    if (sp) { strlcpy(buf, sp, buflen); return; }
    if (sc >= SC_START && sc <= SC_END) {
        bool shifted = (mod & (MOD_LSHIFT | MOD_RSHIFT)) != 0;
        char ch = shifted ? sc_shift[sc - SC_START] : sc_normal[sc - SC_START];
        if (ch >= 0x20 && ch <= 0x7E) { snprintf(buf, buflen, "%c", ch); return; }
    }
    snprintf(buf, buflen, "0x%02X", sc);
}

// Append printable character to text buffer
static void text_buf_append(char ch)
{
    if (ch == '\b') {
        if (s_text_len > 0) s_text_buf[--s_text_len] = '\0';
        return;
    }
    if (ch == '\n') {
        if (s_text_len + 1 < TEXT_BUF_SIZE) { s_text_buf[s_text_len++] = ' '; s_text_buf[s_text_len] = '\0'; }
        return;
    }
    if (ch >= 0x20 && ch <= 0x7E && s_text_len + 1 < TEXT_BUF_SIZE) {
        s_text_buf[s_text_len++] = ch;
        s_text_buf[s_text_len] = '\0';
    }
}

// Process keyboard changes
static void process_keys(const kbd_state_t* cur, const kbd_state_t* prev)
{
    // Newly pressed keys
    for (int i = 0; i < 6; i++) {
        uint8_t sc = cur->keys[i];
        if (sc == 0 || sc == 0x01) continue;
        bool was = false;
        for (int j = 0; j < 6; j++) { if (prev->keys[j] == sc) { was = true; break; } }
        if (!was) {
            s_total_keys++;
            record_keystroke();
            s_key_press_start[sc] = esp_timer_get_time();
            s_last_scancode = sc;
            get_key_label(sc, cur->modifier, s_last_key_name, sizeof(s_last_key_name));
            s_last_press_time_us = esp_timer_get_time();

            // Append to text buffer
            if (sc >= SC_START && sc <= SC_END) {
                bool shifted = (cur->modifier & (MOD_LSHIFT | MOD_RSHIFT)) != 0;
                char ch = shifted ? sc_shift[sc - SC_START] : sc_normal[sc - SC_START];
                text_buf_append(ch);
            } else if (sc == 0x2C) {
                text_buf_append(' ');
            } else if (sc == 0x28) {
                text_buf_append('\n');
            } else if (sc == 0x2A) {
                text_buf_append('\b');
            }

            ESP_LOGI(TAG, "KEY DOWN: %s (0x%02X)", s_last_key_name, sc);
        }
    }

    // Released keys
    for (int i = 0; i < 6; i++) {
        uint8_t sc = prev->keys[i];
        if (sc == 0 || sc == 0x01) continue;
        bool still = false;
        for (int j = 0; j < 6; j++) { if (cur->keys[j] == sc) { still = true; break; } }
        if (!still) {
            if (s_key_press_start[sc] > 0) {
                s_last_hold_ms = (uint32_t)((esp_timer_get_time() - s_key_press_start[sc]) / 1000);
                s_key_press_start[sc] = 0;
            }
            char name[32];
            get_key_label(sc, prev->modifier, name, sizeof(name));
            ESP_LOGI(TAG, "KEY UP:   %s (0x%02X) held %lums", name, sc, (unsigned long)s_last_hold_ms);
        }
    }

    // Modifier changes
    uint8_t mchg = cur->modifier ^ prev->modifier;
    if (mchg) {
        const char* mn[] = {"L-Ctrl","L-Shift","L-Alt","L-GUI","R-Ctrl","R-Shift","R-Alt","R-GUI"};
        for (int i = 0; i < 8; i++) {
            if (mchg & (1 << i)) {
                ESP_LOGI(TAG, "%s %s", mn[i], (cur->modifier & (1 << i)) ? "PRESSED" : "RELEASED");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Display rendering
// ---------------------------------------------------------------------------
static void render_display(void)
{
    if (!display_ok || !framebuffer) return;

    // Clear to dark background
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++)
        framebuffer[i] = C_BLACK;

    char tmp[128];

    // === HEADER (y=5..30) ===
    draw_str(10, 8, "USB Keyboard Test v2.0", C_YELLOW, 2);
    fill_rect(10, 44, LAND_W - 20, 2, C_YELLOW);

    // === CONNECTION STATUS ===
    if (!s_connected) {
        draw_str(10, 55, "Waiting for keyboard...", C_RED, 2);
        draw_str(10, 85, "Connect USB keyboard to USB-A port", C_GRAY, 2);
    } else {
        snprintf(tmp, sizeof(tmp), "Connected: VID:0x%04X PID:0x%04X", s_vid, s_pid);
        draw_str(10, 55, tmp, C_GREEN, 2);
    }

    // === BIG KEY DISPLAY (center-ish area, y=100..250) ===
    fill_rect(10, 100, 300, 160, C_DKGRAY);
    fill_rect(12, 102, 296, 156, C_BLACK);
    draw_str(20, 105, "Last Key:", C_LTGRAY, 1);
    if (s_last_key_name[0]) {
        int kw = str_px_width(s_last_key_name, 4);
        int kx = 10 + (300 - kw) / 2;
        if (kx < 15) kx = 15;
        draw_str(kx, 140, s_last_key_name, C_CYAN, 4);
    }
    snprintf(tmp, sizeof(tmp), "Scancode: 0x%02X", s_last_scancode);
    draw_str(20, 240, tmp, C_GRAY, 1);

    // === MODIFIER INDICATORS (right of big key) ===
    int mx = 330, my = 105;
    draw_str(mx, my, "Modifiers:", C_LTGRAY, 1);
    my += 18;

    uint8_t mod = s_cur.modifier;
    struct { const char* name; uint8_t mask; } mods[] = {
        {"CTRL",  MOD_LCTRL | MOD_RCTRL},
        {"SHIFT", MOD_LSHIFT | MOD_RSHIFT},
        {"ALT",   MOD_LALT | MOD_RALT},
        {"GUI",   MOD_LGUI | MOD_RGUI},
    };
    for (int i = 0; i < 4; i++) {
        bool active = (mod & mods[i].mask) != 0;
        fill_rect(mx, my, 90, 28, active ? C_GREEN : C_DKGRAY);
        fill_rect(mx + 2, my + 2, 86, 24, active ? C_DKGREEN : C_BLACK);
        draw_str(mx + 8, my + 8, mods[i].name, active ? C_WHITE : C_GRAY, 1);
        my += 34;
    }

    // === CURRENTLY PRESSED KEYS ===
    int kx = 330, ky = my + 10;
    draw_str(kx, ky, "Pressed:", C_LTGRAY, 1);
    ky += 14;
    char pressed[128] = "";
    int ppos = 0;
    for (int i = 0; i < 6; i++) {
        if (s_cur.keys[i] && s_cur.keys[i] != 0x01) {
            char kn[16];
            get_key_label(s_cur.keys[i], s_cur.modifier, kn, sizeof(kn));
            int l = snprintf(pressed + ppos, sizeof(pressed) - ppos, "%s ", kn);
            if (l > 0) ppos += l;
        }
    }
    if (ppos == 0) strlcpy(pressed, "(none)", sizeof(pressed));
    draw_str(kx, ky, pressed, C_CYAN, 1);

    // === TELEMETRY PANEL (right side) ===
    int tx = 650, ty = 100;
    fill_rect(tx - 5, ty - 5, 330, 165, C_DKGRAY);
    fill_rect(tx - 3, ty - 3, 326, 161, C_BLACK);

    draw_str(tx, ty, "TELEMETRY", C_YELLOW, 2);
    ty += 30;

    snprintf(tmp, sizeof(tmp), "Keystrokes: %lu", (unsigned long)s_total_keys);
    draw_str(tx, ty, tmp, C_WHITE, 2);
    ty += 25;

    float wpm = calc_wpm();
    snprintf(tmp, sizeof(tmp), "WPM: %.0f", wpm);
    uint16_t wpm_color = wpm > 40 ? C_GREEN : (wpm > 20 ? C_YELLOW : C_WHITE);
    draw_str(tx, ty, tmp, wpm_color, 2);
    ty += 25;

    snprintf(tmp, sizeof(tmp), "Hold: %lums", (unsigned long)s_last_hold_ms);
    draw_str(tx, ty, tmp, C_WHITE, 2);
    ty += 25;

    // WPM bar
    draw_str(tx, ty, "Speed:", C_LTGRAY, 1);
    ty += 12;
    int bar_w = 300;
    int bar_h = 16;
    fill_rect(tx, ty, bar_w, bar_h, C_DKGRAY);
    int filled = (int)(wpm * bar_w / 100.0f);
    if (filled > bar_w) filled = bar_w;
    if (filled > 0) {
        uint16_t bar_color = wpm > 60 ? C_GREEN : (wpm > 30 ? C_YELLOW : C_ORANGE);
        fill_rect(tx, ty, filled, bar_h, bar_color);
    }

    // === TEXT BUFFER (bottom area) ===
    int text_y = 290;
    fill_rect(10, text_y, LAND_W - 20, 2, C_DKGRAY);
    text_y += 8;
    draw_str(10, text_y, "Typed text:", C_LTGRAY, 1);
    text_y += 14;

    // Show last ~70 characters to fit on screen (2x scale)
    int max_chars = (LAND_W - 40) / 18;
    const char* display_text = s_text_buf;
    if (s_text_len > max_chars) display_text = s_text_buf + s_text_len - max_chars;
    draw_str(10, text_y, display_text, C_WHITE, 2);

    // Blinking cursor
    static int blink = 0;
    blink++;
    if (blink % 4 < 2) {
        int cursor_x = 10 + (int)strlen(display_text) * 18;
        if (cursor_x > LAND_W - 20) cursor_x = LAND_W - 20;
        fill_rect(cursor_x, text_y, 2, 16, C_CYAN);
    }

    // === RAW REPORT (very bottom) ===
    int raw_y = text_y + 30;
    draw_str(10, raw_y, "Raw HID:", C_LTGRAY, 1);
    snprintf(tmp, sizeof(tmp), "%02X %02X %02X %02X %02X %02X %02X %02X",
             s_cur.modifier, 0, s_cur.keys[0], s_cur.keys[1],
             s_cur.keys[2], s_cur.keys[3], s_cur.keys[4], s_cur.keys[5]);
    draw_str(80, raw_y, tmp, C_GRAY, 1);

    // === FOOTER ===
    draw_str(10, LAND_H - 15, "Andy+AI  |  M5Stack Tab5  |  ESP-IDF 6.1", C_DKGRAY, 1);

    // Push to display
    esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, framebuffer);
}

// ---------------------------------------------------------------------------
// USB Host callbacks
// ---------------------------------------------------------------------------
static void usb_event_cb(const usb_host_client_event_msg_t* msg, void* arg)
{
    switch (msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "USB: New device (addr %d)", msg->new_dev.address);
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGI(TAG, "USB: Device disconnected");
            break;
        default: break;
    }
}

static void usb_client_task(void* arg)
{
    while (1) {
        if (s_usb_client) usb_host_client_handle_events(s_usb_client, portMAX_DELAY);
        else vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void hid_iface_cb(hid_host_device_handle_t dev, const hid_host_interface_event_t event, void* arg)
{
    switch (event) {
        case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
            uint8_t data[64] = {};
            size_t len = 0;
            if (hid_host_device_get_raw_input_report_data(dev, data, sizeof(data), &len) == ESP_OK && len >= 8) {
                s_cur.modifier = data[0];
                for (int i = 0; i < 6; i++) s_cur.keys[i] = data[2 + i];

                if (s_first_report) {
                    memcpy(&s_prev, &s_cur, sizeof(kbd_state_t));
                    s_first_report = false;
                    ESP_LOGI(TAG, "First keyboard report");
                    return;
                }
                process_keys(&s_cur, &s_prev);
                memcpy(&s_prev, &s_cur, sizeof(kbd_state_t));
            }
            break;
        }
        case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Keyboard DISCONNECTED");
            hid_host_device_close(dev);
            s_connected = false;
            memset(&s_cur, 0, sizeof(kbd_state_t));
            memset(&s_prev, 0, sizeof(kbd_state_t));
            s_first_report = true;
            s_vid = s_pid = 0;
            break;
        case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
            ESP_LOGW(TAG, "Transfer error");
            break;
        default: break;
    }
}

static void hid_device_cb(hid_host_device_handle_t dev, const hid_host_driver_event_t event, void* arg)
{
    hid_host_dev_params_t params;
    if (hid_host_device_get_params(dev, &params) != ESP_OK) return;

    ESP_LOGI(TAG, "HID event=%d addr=%d iface=%d proto=%d", event, params.addr, params.iface_num, params.proto);

    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        hid_host_dev_info_t info;
        if (hid_host_get_device_info(dev, &info) == ESP_OK) {
            ESP_LOGI(TAG, "HID Connected VID:0x%04X PID:0x%04X proto=%d", info.VID, info.PID, params.proto);
            s_vid = info.VID;
            s_pid = info.PID;
        }

        const hid_host_device_config_t cfg = { .callback = hid_iface_cb, .callback_arg = NULL };
        hid_host_device_open(dev, &cfg);
        hid_host_device_start(dev);
        vTaskDelay(pdMS_TO_TICKS(100));
        s_connected = true;
        ESP_LOGI(TAG, "Keyboard ready!");
    }
}

// ---------------------------------------------------------------------------
// Display init
// ---------------------------------------------------------------------------
static esp_err_t display_init(void)
{
    bsp_display_config_t cfg = {};
    esp_err_t ret = bsp_display_new_with_handles_to_st7123(&cfg, &lcd_handles);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(ret)); return ret; }

    panel_handle = lcd_handles.panel;
    if (!panel_handle) { ESP_LOGE(TAG, "Panel handle NULL"); return ESP_FAIL; }

    esp_lcd_panel_disp_on_off(panel_handle, true);

    ret = bsp_display_brightness_init();
    if (ret == ESP_OK) bsp_display_brightness_set(80);

    size_t fb_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    framebuffer = (uint16_t*)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    if (!framebuffer) { ESP_LOGE(TAG, "Framebuffer alloc failed!"); return ESP_ERR_NO_MEM; }

    display_ok = true;
    ESP_LOGI(TAG, "Display ready: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=========================================");
    ESP_LOGI(TAG, " USB Keyboard Test v2.0 - M5Stack Tab5");
    ESP_LOGI(TAG, "=========================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Display first (BSP initializes I2C and IO expanders internally)
    ESP_LOGI(TAG, "Init display...");
    ret = display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display failed, continuing with serial only");
    }

    // USB Host
    ESP_LOGI(TAG, "Starting USB Host...");
    ESP_ERROR_CHECK(bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true));
    bsp_set_usb_5v_en(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    ret = usb_host_lib_set_root_port_power(true);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        ESP_LOGW(TAG, "Root port power: %s", esp_err_to_name(ret));

    usb_host_client_config_t ccfg = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = { .client_event_callback = usb_event_cb, .callback_arg = NULL }
    };
    if (usb_host_client_register(&ccfg, &s_usb_client) == ESP_OK) {
        xTaskCreate(usb_client_task, "usb_cl", 4096, NULL, 5, NULL);
    }

    const hid_host_driver_config_t hcfg = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_device_cb,
        .callback_arg = NULL
    };
    ESP_ERROR_CHECK(hid_host_install(&hcfg));

    ESP_LOGI(TAG, "Connect USB keyboard to USB-A port on Tab5");

    // Main loop: update display at ~10 FPS
    while (1) {
        render_display();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
