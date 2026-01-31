/*
 * NES OSD - External Display Implementation ILI9341 (2.4")
 * 
 * OSD functions for nofrendo with external ILI9341 display (240x320, 2.4")
 * - Display: External ILI9341 (240x320, 2.4 дюйма)
 * - Input: Keyboard + Joystick2
 * - Sound: Speaker
 */

#ifdef USE_EXTERNAL_DISPLAY

#include <M5Cardputer.h>
#include <Arduino.h>
#include <string.h>
#include <Wire.h>  // For Joystick2 I2C
#include <esp_timer.h>  // For audio timer (60 Hz independent from video)
#include "external_display/LGFX_ILI9341.h"

// Nofrendo headers
extern "C" {
    #include <noftypes.h>
    #include <event.h>
    #include <log.h>
    #include <osd.h>
    #include <nofconfig.h>
    #include <nes/nes_pal.h>
    #include <nes/nesinput.h>
}

// NES screen dimensions
#define NES_SCREEN_WIDTH  256
#define NES_SCREEN_HEIGHT 240

// Display dimensions (external ILI9341 display - physical, native portrait, 2.4")
// Native ILI9341: 240×320 (portrait)
// After offset_rotation=1 + setRotation(1): depends on rotation
#define PHYSICAL_WIDTH  240  // Native width
#define PHYSICAL_HEIGHT 320  // Native height

// Render dimensions: height=240, aspect ratio 4:3 (width = 240 * 4/3 = 320)
// But physical width is only 240, so we scale 320×240 → 240×180 to fit
// However, user wants height=240, so we use 240×240 and center it
#define RENDER_WIDTH  240  // Full width (fits physical width)
#define RENDER_HEIGHT 240  // Fixed height as requested

// Centering offsets (center both horizontally and vertically)
// After offset_rotation=1 + setRotation(0): physical display is 320×240 (width×height, landscape)
// RENDER_WIDTH=240, so center horizontally: (320 - 240) / 2 = 40
// RENDER_HEIGHT=240 fits exactly, so Y=0 is correct (already centered vertically)
#define RENDER_X ((320 - RENDER_WIDTH) / 2)  // Center horizontally: (320 - 240) / 2 = 40
#define RENDER_Y 0  // Centered vertically (240 fits exactly)

// Frame buffer (static allocation)
static uint8_t fb[NES_SCREEN_WIDTH * 256];  // 256x256 buffer
static bitmap_t *myBitmap = NULL;
static bool fb_initialized = false;
static uint16_t myPalette[256];

// Pre-computed scaling LUTs (for 256×240 → 320×240)
static uint16_t xLut[RENDER_WIDTH];
static uint16_t yLut[RENDER_HEIGHT];
static bool luts_initialized = false;

// Initialize scaling LUTs (called once)
static void init_scale_luts(void) {
    if (luts_initialized) return;
    
    // X scaling LUT: NES 256 → render 240 (for 4:3 aspect ratio at height 240)
    // Target 4:3 ratio: width = 240 * 4/3 = 320, but physical width is 240
    // So we scale NES 256×240 → 240×240 (height fixed at 240, width scaled to fit)
    // X: 256 → 240 (scale factor 240/256 = 0.9375x)
    for (int x = 0; x < RENDER_WIDTH; x++) {
        int srcX = (x * NES_SCREEN_WIDTH) / RENDER_WIDTH;
        if (srcX < 0) srcX = 0;
        if (srcX >= NES_SCREEN_WIDTH) srcX = NES_SCREEN_WIDTH - 1;
        xLut[x] = srcX;
    }
    
    // Y scaling LUT: NES 240 → render 240 (1:1, height fixed at 240)
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        int srcY = (y * NES_SCREEN_HEIGHT) / RENDER_HEIGHT;
        if (srcY < 0) srcY = 0;
        if (srcY >= NES_SCREEN_HEIGHT) srcY = NES_SCREEN_HEIGHT - 1;
        yLut[y] = srcY;
    }
    
    // ✅ Проверка LUT на выход за границы (для диагностики)
    #ifdef DEBUG_LUT
    for (int x = 0; x < RENDER_WIDTH; ++x) {
        if (xLut[x] < 0 || xLut[x] >= NES_SCREEN_WIDTH) {
            Serial.printf("BAD xLut[%d] = %d\n", x, xLut[x]);
        }
    }
    for (int y = 0; y < RENDER_HEIGHT; ++y) {
        if (yLut[y] < 0 || yLut[y] >= NES_SCREEN_HEIGHT) {
            Serial.printf("BAD yLut[%d] = %d\n", y, yLut[y]);
        }
    }
    #endif
    
    luts_initialized = true;
}

// ============================================================================
// JOYSTICK2 SUPPORT
// ============================================================================

// Joystick2 I2C address (PORT.A: G1=SDA, G2=SCL)
#define JOYSTICK2_ADDR 0x63
static bool joystick2_available = false;

// Joystick2 registers (from official documentation)
#define REG_ADC_X_8   0x10  // X ADC 8-bit (0-255)
#define REG_ADC_Y_8   0x11  // Y ADC 8-bit (0-255)
#define REG_BUTTON    0x20  // Button (1=no press, 0=press)

// Joystick2 data structure
struct Joystick2Data {
    uint8_t x;      // 0-255, center ~127
    uint8_t y;      // 0-255, center ~127
    uint8_t button; // 1=pressed, 0=not pressed
};

// Read Joystick2 data
static bool readJoystick2(Joystick2Data* data) {
    if (!joystick2_available) return false;
    
    // Read X
    Wire.beginTransmission(JOYSTICK2_ADDR);
    Wire.write(REG_ADC_X_8);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
        data->x = Wire.read();
    } else {
        return false;
    }
    
    // Read Y
    Wire.beginTransmission(JOYSTICK2_ADDR);
    Wire.write(REG_ADC_Y_8);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
        data->y = Wire.read();
    } else {
        return false;
    }
    
    // Read button
    Wire.beginTransmission(JOYSTICK2_ADDR);
    Wire.write(REG_BUTTON);
    if (Wire.endTransmission(false) != 0) return false;
    
    Wire.requestFrom(JOYSTICK2_ADDR, 1);
    if (Wire.available() >= 1) {
        uint8_t btn = Wire.read();
        // Invert: in joystick 0=pressed, 1=not pressed
        data->button = (btn == 0) ? 1 : 0;
    } else {
        return false;
    }
    
    return true;
}

// Memory allocation
extern "C" void *mem_alloc(int size, bool prefer_fast_memory) {
    (void)prefer_fast_memory;
    return malloc((size_t)size);
}

// ============================================================================
// VIDEO DRIVER
// ============================================================================

static int init(int width, int height) {
    (void)width; (void)height;
    return 0;
}

static void shutdown(void) {
    // Cleanup handled by osd_shutdown
}

static int set_mode(int width, int height) {
    (void)width; (void)height;
    return 0;
}

static void set_palette(rgb_t *pal) {
    // Convert RGB palette to RGB565 (with byte swap for LovyanGFX)
    for (int i = 0; i < 256; i++) {
        uint16_t c =
            ((pal[i].r & 0xF8) << 8) |  // Rrrrr000000
            ((pal[i].g & 0xFC) << 3) |  // Gggggg000
            ((pal[i].b & 0xF8) >> 3);   // 000bbbbb
        
        // Swap bytes (as in working nes_cardputer_adv_simple project)
        myPalette[i] = (uint16_t)((c >> 8) | ((c & 0xff) << 8));
    }
}

static void clear(uint8_t color) {
    (void)color;
    // Fill entire screen black (including borders for centered rendering)
    externalDisplay.fillScreen(TFT_BLACK);
}

static bitmap_t *lock_write(void) {
    if (!fb_initialized) {
        // Clear static buffer
        memset(fb, 0, sizeof(fb));
        
        // Create bitmap using static buffer
        myBitmap = bmp_createhw((uint8_t *)fb, NES_SCREEN_WIDTH, 256, NES_SCREEN_WIDTH);
        if (!myBitmap) {
            Serial.println("[OSD] ERROR: bmp_createhw failed!");
            return NULL;
        }
        
        fb_initialized = true;
        Serial.printf("[OSD] Frame buffer initialized: %u bytes\n", sizeof(fb));
    }
    return myBitmap;
}

static void free_write(int num_dirties, rect_t *dirty_rects) {
    (void)num_dirties; (void)dirty_rects;
    // Persistent framebuffer, no free needed
}

// Forward declarations
extern "C" void do_audio_frame(void);

// Render frame to display (240×240 with scaling, pushImage for each line)
// ✅ OPTIMIZED: Using pushImage() instead of setWindow() + pushPixels() to avoid rotation issues
static void render_frame(const uint8_t **data) {
    if (!data) return;
    
    // Initialize LUTs on first call
    if (!luts_initialized) {
        init_scale_luts();
    }
    
    // Get actual display dimensions after rotation (dynamic)
    int32_t dispW = externalDisplay.width();
    int32_t dispH = externalDisplay.height();
    
    // Calculate centering offsets dynamically
    int32_t renderX = (dispW - RENDER_WIDTH) / 2;
    int32_t renderY = (dispH - RENDER_HEIGHT) / 2;
    
    // ✅ Single transaction for entire frame
    externalDisplay.startWrite();
    
    static uint16_t lineBuf[RENDER_WIDTH];
    
    // Render with scaling (256×240 → 240×240, height fixed at 240, centered)
    for (int y = 0; y < RENDER_HEIGHT; y++) {
        // Use pre-computed Y LUT
        int srcY = yLut[y];
        const uint8_t *srcLine = data[srcY];
        
        // Use pre-computed X LUT for each pixel
        for (int x = 0; x < RENDER_WIDTH; x++) {
            int srcX = xLut[x];
            lineBuf[x] = myPalette[srcLine[srcX]];
        }
        
        // ✅ ВАЖНО: используем pushImage() вместо setWindow() + pushPixels()
        // Это позволяет LovyanGFX правильно обработать rotation
        externalDisplay.pushImage(renderX, renderY + y, RENDER_WIDTH, 1, lineBuf);
        
        // Yield every 32 lines to avoid blocking
        if ((y & 31) == 0) {
            vTaskDelay(0);
        }
    }
    
    // Fill borders (top, bottom, left, right)
    if (renderY > 0) {
        externalDisplay.fillRect(0, 0, dispW, renderY, TFT_BLACK);
    }
    int bottomY = renderY + RENDER_HEIGHT;
    if (bottomY < dispH) {
        externalDisplay.fillRect(0, bottomY, dispW, dispH - bottomY, TFT_BLACK);
    }
    if (renderX > 0) {
        externalDisplay.fillRect(0, renderY, renderX, RENDER_HEIGHT, TFT_BLACK);
    }
    int rightX = renderX + RENDER_WIDTH;
    if (rightX < dispW) {
        externalDisplay.fillRect(rightX, renderY, dispW - rightX, RENDER_HEIGHT, TFT_BLACK);
    }
    
    // ✅ Single endWrite() for entire frame + borders
    externalDisplay.endWrite();
}

static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects) {
    (void)num_dirties; (void)dirty_rects;
    
    if (!bmp || !bmp->line) {
        return;
    }
    
    // Render frame directly (no queue, no RTOS)
    const uint8_t **src_lines = (const uint8_t **)bmp->line;
    render_frame(src_lines);
    
    // ✅ УБРАНО: audio_tick() и do_audio_frame() - теперь звук работает независимо через ESP Timer
    // Звук теперь вызывается на 60 Hz через ESP Timer, независимо от видео FPS
}

static viddriver_t sdlDriver = {
    "Simple DirectMedia Layer",
    init, shutdown, set_mode, set_palette, clear,
    lock_write, free_write, custom_blit,
    false
};

extern "C" void osd_getvideoinfo(vidinfo_t *info) {
    info->default_width  = NES_SCREEN_WIDTH;
    info->default_height = NES_SCREEN_HEIGHT;
    info->driver = &sdlDriver;
}

// ============================================================================
// SOUND
// ============================================================================

// Audio parameters
static constexpr int kSampleRate = 22050; // Hz
static constexpr int kNesHz      = 60;    // 60 FPS
static constexpr int kChunk      = (kSampleRate + kNesHz/2) / kNesHz; // 368 samples per frame

// Audio channel
static constexpr int kChannel = 0;

// Nofrendo audio callback
static void (*s_audio_cb)(void *buffer, int length) = nullptr;

// Double buffer mono (Nofrendo to Speaker)
static int16_t s_buf[2][kChunk];
static uint8_t s_flip = 0;
static uint8_t s_volume = 80;  // Current volume (0-255)

// ESP Timer для звука (60 Hz независимо от видео)
static esp_timer_handle_t audio_timer = nullptr;

// Forward declarations
extern "C" void do_audio_frame(void);

// ============================================================================
// ESP Timer для звука (60 Hz независимо от видео)
// ============================================================================

// Callback для таймера звука
static void audio_timer_callback(void* arg) {
    // Вызываем do_audio_frame() независимо от видео
    static uint32_t callback_count = 0;
    callback_count++;
    
    // Диагностика: логируем каждые 60 вызовов (раз в секунду)
    if ((callback_count % 60) == 0) {
        Serial.printf("[AUDIO_TIMER] Callback #%u, s_audio_cb=%p\n", callback_count, s_audio_cb);
    }
    
    do_audio_frame();
}

// Инициализация таймера звука (вызывать после osd_init_sound)
static void init_audio_timer(void) {
    esp_timer_create_args_t timer_args = {
        .callback = audio_timer_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,  // Task dispatch (safe)
        .name = "NES_Audio_60Hz",
        .skip_unhandled_events = false
    };
    
    esp_err_t err = esp_timer_create(&timer_args, &audio_timer);
    if (err != ESP_OK) {
        Serial.printf("[SOUND] ERROR: Failed to create audio timer: %s\n", esp_err_to_name(err));
        return;
    }
    
    // 60 Hz = 16666.67 microseconds (16.67 ms)
    uint64_t period_us = 1000000ULL / 60;  // 16666 microseconds
    
    err = esp_timer_start_periodic(audio_timer, period_us);
    if (err != ESP_OK) {
        Serial.printf("[SOUND] ERROR: Failed to start audio timer: %s\n", esp_err_to_name(err));
        esp_timer_delete(audio_timer);
        audio_timer = nullptr;
        return;
    }
    
    Serial.printf("[SOUND] Audio timer started: 60 Hz (%llu us period)\n", period_us);
}

// Остановка таймера звука
static void stop_audio_timer(void) {
    if (audio_timer) {
        esp_timer_stop(audio_timer);
        esp_timer_delete(audio_timer);
        audio_timer = nullptr;
        Serial.println("[SOUND] Audio timer stopped");
    }
}

extern "C" int osd_init_sound(void) {
    Serial.println("[SOUND] Initializing speaker...");
    
    auto cfg = M5Cardputer.Speaker.config();
    cfg.sample_rate    = kSampleRate;
    cfg.stereo         = false;   // mono
    cfg.dma_buf_len    = 256;     // ✅ FIX NOISE: Larger buffer (256) for smoother playback
    cfg.dma_buf_count  = 3;       // ✅ FIX NOISE: 3 buffers = ~35ms buffer (prevents gaps/noise)
    cfg.task_priority  = 5;       // ✅ INCREASED: Higher priority for audio (was 4)
    cfg.task_pinned_core = 1;     // pin to core 1 (avoid conflict with other tasks)
    M5Cardputer.Speaker.config(cfg);

    if (!M5Cardputer.Speaker.isRunning()) {
        M5Cardputer.Speaker.begin();
    }
    M5Cardputer.Speaker.setVolume(s_volume); // Use variable

    M5Cardputer.Speaker.stop(kChannel);
    
    Serial.printf("[SOUND] Speaker initialized (volume: %d/255)\n", s_volume);
    
    // ✅ НОВОЕ: Запускаем таймер для звука на 60 Hz (независимо от видео)
    init_audio_timer();
    
    return 0;
}

extern "C" void osd_stopsound(void) {
    s_audio_cb = nullptr;
    
    // ✅ НОВОЕ: Останавливаем таймер звука
    stop_audio_timer();
    
    M5Cardputer.Speaker.stop(kChannel);
}

extern "C" void osd_setsound(void (*playfunc)(void *buffer, int length)) {
    s_audio_cb = playfunc;
    Serial.printf("[SOUND] Audio callback set: s_audio_cb=%p\n", s_audio_cb);
    
    // Если таймер уже запущен, но callback только что установлен - это нормально
    if (audio_timer && s_audio_cb) {
        Serial.println("[SOUND] Audio timer is running, callback is set - audio should work now");
    }
}

extern "C" void osd_getsoundinfo(sndinfo_t *info) {
    info->sample_rate = kSampleRate;
    info->bps = 16;
}

// Process audio frame (called from ESP Timer callback at 60 Hz)
extern "C" void do_audio_frame(void) {
    static uint32_t frame_count = 0;
    static uint32_t skip_count = 0;
    
    if (!s_audio_cb) {
        skip_count++;
        // Логируем каждые 60 пропусков (раз в секунду)
        if ((skip_count % 60) == 0) {
            Serial.printf("[AUDIO] do_audio_frame skipped (no callback): %u times\n", skip_count);
        }
        return;
    }
    
    frame_count++;
    // Диагностика: логируем каждые 60 кадров (раз в секунду)
    if ((frame_count % 60) == 0) {
        Serial.printf("[AUDIO] do_audio_frame #%u\n", frame_count);
    }

    // ✅ УБРАНО: Queue depth control больше не нужен
    // Звук теперь вызывается стабильно на 60 Hz через ESP Timer,
    // поэтому накопления не должно быть
    
    // Generate audio samples into buffer
    s_audio_cb((void*)s_buf[s_flip], kChunk);
    
    // Play buffer with repeat=1 (continuous playback)
    // Увеличенные буферы + repeat=1 обеспечивают плавное воспроизведение без пропусков
    (void)M5Cardputer.Speaker.playRaw(
        (const int16_t*)s_buf[s_flip], (size_t)kChunk,
        (uint32_t)kSampleRate, false /*mono*/,
        1 /*repeat=1: continuous playback*/, kChannel, false /*async*/
    );
    
    // Switch to other buffer for next frame
    s_flip ^= 1;
}

// ✅ УБРАНО: audio_tick() больше не нужен
// Звук теперь работает через ESP Timer на 60 Hz независимо от видео
// Queue depth control не нужен, так как звук вызывается стабильно

// ============================================================================
// INPUT
// ============================================================================

static void osd_initinput(void) {
    // Try to detect Joystick2 on PORT.A (G2=SDA, G1=SCL)
    // Reinitialize I2C with correct pins
    Wire.end();  // Close previous initialization
    delay(10);
    Wire.begin(2, 1, 100000);  // SDA=G2, SCL=G1, 100kHz
    delay(100);
    
    Serial.println("[INPUT] Checking for Joystick2...");
    Serial.println("[INPUT] I2C: SDA=G2, SCL=G1");
    Serial.printf("[INPUT] Looking for device at 0x%02X...\n", JOYSTICK2_ADDR);
    
    Wire.beginTransmission(JOYSTICK2_ADDR);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        joystick2_available = true;
        Serial.printf("[INPUT] ✅ Joystick2 detected at 0x%02X!\n", JOYSTICK2_ADDR);
    } else {
        joystick2_available = false;
        Serial.printf("[INPUT] ❌ Joystick2 not found (error: %d)\n", error);
        Serial.println("[INPUT] Using keyboard only");
    }
}

static void osd_freeinput(void) {
    // No cleanup needed
}

extern "C" void osd_getinput(void) {
    M5Cardputer.update();
    
    // Volume control (keys - and =)
    static uint32_t last_vol_change = 0;
    uint32_t now = millis();
    
    if (M5Cardputer.Keyboard.isKeyPressed('-')) {
        if (now - last_vol_change > 100) {  // Debounce 100ms
            if (s_volume >= 10) {
                s_volume -= 10;
            } else if (s_volume > 0) {
                s_volume = 0;
            }
            M5Cardputer.Speaker.setVolume(s_volume);
            Serial.printf("[SOUND] Volume: %d/255\n", s_volume);
            last_vol_change = now;
        }
    }
    
    if (M5Cardputer.Keyboard.isKeyPressed('=') || M5Cardputer.Keyboard.isKeyPressed('+')) {
        if (now - last_vol_change > 100) {  // Debounce 100ms
            if (s_volume <= 245) {
                s_volume += 10;
            } else if (s_volume < 255) {
                s_volume = 255;
            }
            M5Cardputer.Speaker.setVolume(s_volume);
            Serial.printf("[SOUND] Volume: %d/255\n", s_volume);
            last_vol_change = now;
        }
    }
    
    // Map keyboard to NES buttons
    const int ev[8] = {
        event_joypad1_up,    event_joypad1_down,
        event_joypad1_left,  event_joypad1_right,
        event_joypad1_select, event_joypad1_start,
        event_joypad1_a,     event_joypad1_b
    };
    
    static uint32_t old_state = 0xFFFFFFFF;
    uint32_t state = 0xFFFFFFFF;  // All buttons released by default
    
    // Get keyboard state for special keys (ENTER, etc.)
    auto keys = M5Cardputer.Keyboard.keysState();
    
    // Check keys and set bits
    if (M5Cardputer.Keyboard.isKeyPressed('w') || M5Cardputer.Keyboard.isKeyPressed('W')) {
        state &= ~(1UL << 0);  // UP
    }
    if (M5Cardputer.Keyboard.isKeyPressed('s') || M5Cardputer.Keyboard.isKeyPressed('S')) {
        state &= ~(1UL << 1);  // DOWN
    }
    if (M5Cardputer.Keyboard.isKeyPressed('a') || M5Cardputer.Keyboard.isKeyPressed('A')) {
        state &= ~(1UL << 2);  // LEFT
    }
    if (M5Cardputer.Keyboard.isKeyPressed('d') || M5Cardputer.Keyboard.isKeyPressed('D')) {
        state &= ~(1UL << 3);  // RIGHT
    }
    // ✅ SELECT: ''' (0x27, одинарная кавычка)
    if (M5Cardputer.Keyboard.isKeyPressed('\'')) {
        state &= ~(1UL << 4);  // SELECT
    }
    // ✅ START: ENTER (используем keysState().enter вместо isKeyPressed('\n'))
    if (keys.enter) {
        state &= ~(1UL << 5);  // START
    }
    // ✅ A: пробел (0x20)
    if (M5Cardputer.Keyboard.isKeyPressed(' ')) {
        state &= ~(1UL << 6);  // A
    }
    // ✅ B: '/' (0x2F)
    if (M5Cardputer.Keyboard.isKeyPressed('/')) {
        state &= ~(1UL << 7);  // B
    }
    
    // Read Joystick2 if available
    Joystick2Data joy;
    if (readJoystick2(&joy)) {
        // Deadzone threshold
        const uint8_t threshold = 40;
        const uint8_t center = 127;
        
        // D-pad mapping (инвертированные оси)
        // X axis: inverted (left ↔ right)
        if (joy.x < (center - threshold)) {
            state &= ~(1UL << 3); // RIGHT (was left)
        }
        if (joy.x > (center + threshold)) {
            state &= ~(1UL << 2); // LEFT (was right)
        }
        // Y axis: inverted (up ↔ down)
        if (joy.y < (center - threshold)) {
            state &= ~(1UL << 1); // DOWN (was up)
        }
        if (joy.y > (center + threshold)) {
            state &= ~(1UL << 0); // UP (was down)
        }
        
        // Button mapping (центральная кнопка джойстика)
        // Центральная кнопка = A (прыжок в Mario)
        if (joy.button == 1) {
            state &= ~(1UL << 6); // A button
        }
    }
    
    // Send events for changed buttons
    uint32_t changed = state ^ old_state;
    for (int i = 0; i < 8; i++) {
        if (changed & (1UL << i)) {
            event_t evh = event_get(ev[i]);
            if (evh) {
                bool pressed = (state & (1UL << i)) == 0;
                evh(pressed ? INP_STATE_MAKE : INP_STATE_BREAK);
            }
        }
    }
    
    old_state = state;
}

extern "C" void osd_getmouse(int *x, int *y, int *button) {
    (void)x; (void)y; (void)button;
    // Not supported
}

// ============================================================================
// INIT/SHUTDOWN
// ============================================================================

static int logprint(const char *string) {
    // Suppress debug malloc logs
    if (strstr(string, "_my_malloc:") != nullptr) {
        return 0;  // Skip malloc debug logs
    }
    Serial.print(string);
    return 0;
}

extern "C" int osd_init(void) {
    nofrendo_log_chain_logfunc(logprint);
    
    Serial.println("[OSD] Initializing...");
    
    // Display is already initialized in main.cpp
    // Just verify it's ready
    Serial.printf("[OSD] Display ready: %ldx%ld\n", (long)externalDisplay.width(), (long)externalDisplay.height());
    Serial.println("[OSD] Display initialized");
    
    // Initialize sound (stub)
    if (osd_init_sound() != 0) {
        Serial.println("[OSD] WARNING: Sound init failed (continuing without sound)");
    }
    
    // Initialize input
    osd_initinput();
    Serial.println("[OSD] Input initialized");
    
    Serial.println("[OSD] OSD initialized successfully");
    return 0;
}

extern "C" void osd_shutdown(void) {
    osd_stopsound();
    osd_freeinput();
    fb_initialized = false;
    myBitmap = NULL;
}

// ============================================================================
// TIMER
// ============================================================================

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

TimerHandle_t nes_timer = NULL;

extern "C" int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize) {
    (void)funcsize; (void)counter; (void)countersize;
    
    if (nes_timer) {
        xTimerDelete(nes_timer, 0);
    }
    
    nes_timer = xTimerCreate("nes", configTICK_RATE_HZ / frequency, pdTRUE, NULL, (TimerCallbackFunction_t)func);
    if (nes_timer) {
        xTimerStart(nes_timer, 0);
        return 0;
    }
    return -1;
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

char configfilename[] = "na";

extern "C" int osd_main(int argc, char *argv[]) {
    (void)argc;
    config.filename = configfilename;
    return main_loop(argv[0], system_autodetect);
}

// ============================================================================
// OTHER OSD FUNCTIONS (stubs)
// ============================================================================

extern "C" void osd_fullname(char *fullname, const char *shortname) {
    strncpy(fullname, shortname, PATH_MAX);
    fullname[PATH_MAX - 1] = '\0';
}

extern "C" char *osd_newextension(char *string, char *ext) {
    size_t l = strlen(string);
    if (l >= 3) {
        string[l - 3] = ext[1];
        string[l - 2] = ext[2];
        string[l - 1] = ext[3];
    }
    return string;
}

extern "C" int osd_makesnapname(char *filename, int len) {
    (void)filename; (void)len;
    return -1; // Not supported
}

extern "C" void osd_set_sram_ptr(uint8_t *ptr, int len) {
    (void)ptr; (void)len;
    // TODO: Implement save states
}

extern "C" const uint8_t* _get_rom_ptr(void) {
    return nullptr; // Not using XIP
}

extern "C" size_t _get_rom_size(void) {
    return 0; // Not using XIP
}

#endif // USE_EXTERNAL_DISPLAY

