/*
 * Battery Monitor for M5Stack Tab5
 * Uses INA226 power monitor IC
 */

#include "battery_monitor.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>

// Forward declaration for USB-C detection (to avoid including full BSP header)
// Function is implemented in m5stack_tab5 component
extern bool bsp_usb_c_detect(void);

static const char *TAG = "BATTERY_MONITOR";

// Thresholds for battery detection (in mA)
#define BATTERY_CURRENT_THRESHOLD_MA 30.0f  // Minimum current to consider battery present
#define BATTERY_CHARGE_THRESHOLD_MA 20.0f   // Current threshold for charging detection
#define BATTERY_DISCHARGE_THRESHOLD_MA 20.0f // Current threshold for discharging detection

// Voltage classification for battery detection (Variant A: voting classifier)
typedef enum {
    VCLS_LOW,   // USB-only / no battery: V < 5.8V
    VCLS_HIGH,  // Battery/VSYS-high: V > 7.2V
    VCLS_MID    // Uncertain: 5.8V - 7.2V (ignored for decision)
} voltage_class_t;

typedef enum {
    BAT_ABSENT,
    BAT_PRESENT,
    BAT_UNKNOWN
} battery_state_t;

typedef enum {
    CHG_IDLE,              // not charging
    CHG_START_DETECT,      // charging just started, analyzing current
    CHG_ACTIVE             // charging active, coulomb counting active
} charge_state_t;

#define V_LOW_THRESHOLD_MV  5800   // mV - below this = LOW (USB/no battery)
#define V_HIGH_THRESHOLD_MV 7200   // mV - above this = HIGH (battery present)
#define WINDOW_SIZE          20     // 20 samples ~2s at 10Hz (100ms updates)
#define NEED_PERCENT         70     // 70% dominance required

// Static variables for voltage classification voting
static voltage_class_t voltage_window[WINDOW_SIZE];
static int window_index = 0;
static int low_count = 0;
static int high_count = 0;
static bool window_filled = false;
static battery_state_t cached_battery_state = BAT_UNKNOWN;

// Coulomb counting for discharge tracking
static float discharged_mah = 0.0f;  // Accumulated discharged capacity in mAh
static int initial_level_on_discharge = -1;  // Battery level when discharge started
static uint32_t last_discharge_update_time_ms = 0;  // Last time we updated discharge integration
static bool was_charging = false;  // Track if we were charging in previous call

// Coulomb counting for charge tracking - State Machine approach
static charge_state_t charge_state = CHG_IDLE;
static float charged_mah = 0.0f;  // Accumulated charged capacity in mAh
static int initial_level_on_charge = -1;  // Battery level when charge started (fixed after CHG_START_DETECT)
static uint32_t last_charge_update_time_ms = 0;  // Last time we updated charge integration
static float last_charge_current = 0.0f;  // Previous charge current for rapid increase detection
static uint32_t charge_start_time_ms = 0;  // When charging started
static float chg_max_current_ma = 0.0f;  // Maximum current during START_DETECT window
static bool charging_latched = false;  // Hysteresis to prevent flapping

// INA226 registers
#define INA226_REG_CONFIG      0x00
#define INA226_REG_SHUNTVOLTAGE 0x01
#define INA226_REG_BUSVOLTAGE   0x02
#define INA226_REG_POWER        0x03
#define INA226_REG_CURRENT      0x04
#define INA226_REG_CALIBRATION  0x05
#define INA226_REG_MASKENABLE   0x06
#define INA226_REG_ALERTLIMIT   0x07
#define INA226_REG_MANUF_ID     0xFE  // Manufacturer ID (Texas Instruments: 0x5449)

// INA226 configuration values
#define INA226_AVERAGES_16      0b010
#define INA226_BUS_CONV_TIME_1100US  0b100
#define INA226_SHUNT_CONV_TIME_1100US 0b100
#define INA226_MODE_SHUNT_BUS_CONT   0b111

#define I2C_MASTER_TIMEOUT_MS 50

static i2c_master_dev_handle_t ina226_dev_handle = NULL;
static bool initialized = false;
static float current_lsb = 0.0f;  // Current LSB for reading current from INA226

// Helper functions
// Read unsigned 16-bit register
static esp_err_t ina226_read_u16(uint8_t reg, uint16_t *out)
{
    if (!out || !ina226_dev_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t w = reg;
    uint8_t r[2] = {0};
    esp_err_t err = i2c_master_transmit_receive(ina226_dev_handle, &w, 1, r, 2, I2C_MASTER_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read register 0x%02X: %s", reg, esp_err_to_name(err));
        return err;
    }
    
    // INA226 registers are big-endian: MSB first
    *out = ((uint16_t)r[0] << 8) | r[1];
    return ESP_OK;
}

// Read signed 16-bit register
static esp_err_t ina226_read_s16(uint8_t reg, int16_t *out)
{
    uint16_t u;
    esp_err_t err = ina226_read_u16(reg, &u);
    if (err != ESP_OK) return err;
    *out = (int16_t)u; // signed interpretation
    return ESP_OK;
}

// Legacy wrapper for backward compatibility (deprecated, use ina226_read_u16/s16)
static int16_t read_register16(uint8_t reg)
{
    uint16_t u;
    if (ina226_read_u16(reg, &u) == ESP_OK) {
        return (int16_t)u;
    }
    return -1; // Only for legacy code, should be replaced
}

static esp_err_t write_register16(uint8_t reg, uint16_t val)
{
    if (!ina226_dev_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t w_buffer[3] = {0};
    w_buffer[0] = reg;
    w_buffer[1] = (val >> 8) & 0xFF;
    w_buffer[2] = val & 0xFF;
    
    esp_err_t ret = i2c_master_transmit(ina226_dev_handle, w_buffer, 3, I2C_MASTER_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to write register 0x%02X: %s", reg, esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

static float read_bus_voltage(void)
{
    uint16_t bus_raw = 0;
    esp_err_t err = ina226_read_u16(INA226_REG_BUSVOLTAGE, &bus_raw);
    if (err != ESP_OK) {
        return -1.0f; // Error
    }
    
    // Bus voltage: bits 15-3 are voltage, LSB = 1.25mV
    // Shift right by 3 bits to get voltage value
    uint16_t voltage_value = bus_raw >> 3;
    return (float)voltage_value * 0.00125f; // Convert to volts
}

static float read_current(void)
{
    if (current_lsb == 0.0f) {
        ESP_LOGW(TAG, "Current LSB not set, cannot read current");
        return -1.0f;  // Not calibrated yet
    }
    
    int16_t current_raw = 0;
    esp_err_t err = ina226_read_s16(INA226_REG_CURRENT, &current_raw);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read current register: %s", esp_err_to_name(err));
        return -1.0f;
    }
    
    // Convert signed 16-bit value to current in Amperes
    // INA226 Current Register formula: Current = (Shunt Voltage × Calibration) / 2048
    // Negative value = current flowing into battery (charging)
    // Positive value = current flowing out of battery (discharging)
    float current = (float)current_raw * current_lsb;
    
    // Log occasionally for debugging
    static int log_count = 0;
    if ((log_count++ % 60) == 0) {
        int16_t shunt_raw = 0;
        if (ina226_read_s16(INA226_REG_SHUNTVOLTAGE, &shunt_raw) == ESP_OK) {
            float shunt_voltage = (float)shunt_raw * 0.0000025f;  // 2.5µV LSB
            ESP_LOGI(TAG, "Current: %.6fA (raw: %d), Shunt: %.6fV (raw: %d)", 
                     current, current_raw, shunt_voltage, shunt_raw);
        }
    }
    
    return current;
}

// Read current from shunt voltage directly (more reliable than CURRENT register)
// Returns current in mA (signed: negative = charging, positive = discharging)
static float read_current_from_shunt_ma_internal(void)
{
    int16_t shunt_raw = 0;
    esp_err_t err = ina226_read_s16(INA226_REG_SHUNTVOLTAGE, &shunt_raw);
    if (err != ESP_OK) {
        return 0.0f; // I2C error
    }
    
    // Shunt voltage LSB = 2.5µV = 0.0000025V
    float shunt_voltage_v = (float)shunt_raw * 0.0000025f;
    
    // Current = Shunt Voltage / R_shunt
    // R_shunt = 0.005 ohm (5 mOhm)
    float current_a = shunt_voltage_v / 0.005f;
    
    return current_a * 1000.0f;  // Convert to mA (signed: negative = charging)
}

// Battery detection window (sample current over time to detect battery presence)
// Returns true if battery is present (significant current detected)
static bool detect_battery_present_window(float (*read_current_ma)(void), 
                                          int samples, int period_ms, 
                                          float thr_ma)
{
    int hits = 0;
    float max_abs = 0.0f;
    
    for (int i = 0; i < samples; i++) {
        float i_ma = read_current_ma();
        float abs_current = fabsf(i_ma);
        
        if (abs_current > max_abs) {
            max_abs = abs_current;
        }
        
        if (abs_current > thr_ma) {
            hits++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(period_ms));
    }
    
    // Battery present if we see significant current at least 2 times, 
    // or if max current is significantly above threshold
    // This filters out noise and random spikes
    return (hits >= 2) || (max_abs > thr_ma * 1.5f);
}

// I2C scan function
static void scan_i2c_bus(i2c_master_bus_handle_t bus_handle) {
    ESP_LOGI(TAG, "Scanning I2C bus...");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };
        
        i2c_master_dev_handle_t dev_handle;
        esp_err_t ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
        if (ret == ESP_OK) {
            // Try to read CONFIG register (0x00) - 2 bytes
            uint8_t reg = 0x00;
            uint8_t read_buf[2] = {0};
            ret = i2c_master_transmit_receive(dev_handle, &reg, 1, read_buf, 2, I2C_MASTER_TIMEOUT_MS);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "  Found device at address 0x%02X (CONFIG: %02X %02X)", addr, read_buf[0], read_buf[1]);
                found++;
            }
            i2c_master_bus_rm_device(dev_handle);
        }
    }
    ESP_LOGI(TAG, "I2C scan complete: %d devices found", found);
}

// Dump all INA226 registers for debugging
static void dump_ina226_registers(void) {
    if (!initialized) {
        ESP_LOGW(TAG, "INA226 not initialized, cannot dump registers");
        return;
    }
    
    uint16_t r00 = 0;
    int16_t r01 = 0;
    uint16_t r02 = 0;
    uint16_t r03 = 0;
    int16_t r04 = 0;
    uint16_t r05 = 0;
    uint16_t r06 = 0;
    uint16_t r07 = 0;
    
    if (ina226_read_u16(INA226_REG_CONFIG, &r00) != ESP_OK) return;
    if (ina226_read_s16(INA226_REG_SHUNTVOLTAGE, &r01) != ESP_OK) return;
    if (ina226_read_u16(INA226_REG_BUSVOLTAGE, &r02) != ESP_OK) return;
    if (ina226_read_u16(INA226_REG_POWER, &r03) != ESP_OK) return;
    if (ina226_read_s16(INA226_REG_CURRENT, &r04) != ESP_OK) return;
    if (ina226_read_u16(INA226_REG_CALIBRATION, &r05) != ESP_OK) return;
    if (ina226_read_u16(INA226_REG_MASKENABLE, &r06) != ESP_OK) return;
    if (ina226_read_u16(INA226_REG_ALERTLIMIT, &r07) != ESP_OK) return;
    
    ESP_LOGI(TAG, "INA226 DUMP: CFG=%04X SHUNT=%04X(%d) BUS=%04X PWR=%04X CUR=%04X(%d) CAL=%04X MASK=%04X ALERT=%04X",
             r00, (uint16_t)r01, (int)r01, r02, r03, (uint16_t)r04, (int)r04, r05, r06, r07);
    
    // Decode CONFIG register
    uint8_t avg = (r00 >> 9) & 0x07;
    uint8_t bus_ct = (r00 >> 6) & 0x07;
    uint8_t shunt_ct = (r00 >> 3) & 0x07;
    uint8_t mode = r00 & 0x07;
    ESP_LOGI(TAG, "  CFG decode: AVG=%d BUS_CT=%d SHUNT_CT=%d MODE=%d", avg, bus_ct, shunt_ct, mode);
    
    // Decode shunt voltage (signed, LSB = 2.5µV)
    float shunt_v = (float)r01 * 0.0000025f;
    ESP_LOGI(TAG, "  SHUNT: %.6fV (raw=%d)", shunt_v, (int)r01);
    
    // Decode bus voltage (unsigned, LSB = 1.25mV)
    float bus_v = (float)(r02 >> 3) * 0.00125f;
    ESP_LOGI(TAG, "  BUS: %.3fV (raw=%04X, shifted=%d)", bus_v, r02, r02 >> 3);
    
    // Decode current (if calibration is set)
    if (current_lsb > 0) {
        float current_a = (float)r04 * current_lsb;
        ESP_LOGI(TAG, "  CURRENT: %.6fA (raw=%d, LSB=%.6fA)", current_a, (int)r04, current_lsb);
    } else {
        ESP_LOGI(TAG, "  CURRENT: LSB not set!");
    }
}

// NVS namespace and key for battery level storage
#define NVS_NAMESPACE "battery"
#define NVS_KEY_LEVEL "level"

// Load battery level from NVS
static int load_battery_level_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(err));
        return -1;
    }
    
    int32_t level = -1;
    err = nvs_get_i32(nvs_handle, NVS_KEY_LEVEL, &level);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK && level >= 0 && level <= 100) {
        ESP_LOGI(TAG, "Loaded battery level from NVS: %d%%", level);
        return (int)level;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No battery level found in NVS, using default");
    } else {
        ESP_LOGW(TAG, "Failed to read battery level from NVS: %s", esp_err_to_name(err));
    }
    
    return -1;
}

// Save battery level to NVS
static esp_err_t save_battery_level_to_nvs(int level)
{
    if (level < 0 || level > 100) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_i32(nvs_handle, NVS_KEY_LEVEL, (int32_t)level);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGD(TAG, "Saved battery level to NVS: %d%%", level);
        } else {
            ESP_LOGW(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "Failed to set battery level in NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t battery_monitor_init(i2c_master_bus_handle_t i2c_bus_handle)
{
    if (initialized) {
        ESP_LOGW(TAG, "Battery monitor already initialized");
        return ESP_OK;
    }
    
    if (!i2c_bus_handle) {
        ESP_LOGE(TAG, "Invalid I2C bus handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Scan I2C bus to see what devices are present
    ESP_LOGI(TAG, "Scanning I2C bus before initialization...");
    scan_i2c_bus(i2c_bus_handle);
    
    // Add INA226 device to I2C bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA226_ADDRESS,
        .scl_speed_hz = 400000,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &ina226_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add INA226 device: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Verify INA226 by reading Manufacturer ID (should be 0x5449 for Texas Instruments)
    uint16_t manuf_id = 0;
    ret = ina226_read_u16(INA226_REG_MANUF_ID, &manuf_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read Manufacturer ID: %s", esp_err_to_name(ret));
        return ret;
    }
    if (manuf_id != 0x5449) {
        ESP_LOGW(TAG, "Manufacturer ID mismatch: expected 0x5449 (Texas Instruments), got 0x%04X", manuf_id);
        ESP_LOGW(TAG, "Device at 0x%02X may not be INA226, but continuing anyway...", INA226_ADDRESS);
    } else {
        ESP_LOGI(TAG, "INA226 verified: Manufacturer ID = 0x%04X (Texas Instruments)", manuf_id);
    }
    
    // Configure INA226
    uint16_t config = 0;
    config |= (INA226_AVERAGES_16 << 9);
    config |= (INA226_BUS_CONV_TIME_1100US << 6);
    config |= (INA226_SHUNT_CONV_TIME_1100US << 3);
    config |= INA226_MODE_SHUNT_BUS_CONT;
    
    ret = write_register16(INA226_REG_CONFIG, config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure INA226");
        return ret;
    }
    
    // Calibrate INA226
    // Tab5 uses 2S Li-Po battery, shunt resistor = 5mOhm, max current = 8.192A
    float r_shunt = 0.005f;  // 5mOhm
    float i_max_expected = 8.192f;  // 8.192A
    
    float minimum_lsb = i_max_expected / 32767.0f;
    current_lsb = ceil(minimum_lsb * 100000000.0f) / 100000000.0f;
    current_lsb = ceil(current_lsb / 0.0001f) * 0.0001f;
    
    uint16_t calibration_value = (uint16_t)(0.00512f / (current_lsb * r_shunt));
    
    ESP_LOGI(TAG, "Calibrating INA226:");
    ESP_LOGI(TAG, "  R_shunt: %.3f ohm (5mOhm)", r_shunt);
    ESP_LOGI(TAG, "  I_max: %.3fA", i_max_expected);
    ESP_LOGI(TAG, "  Current_LSB: %.6fA", current_lsb);
    ESP_LOGI(TAG, "  Calibration value: 0x%04X (%d)", calibration_value, calibration_value);
    
    ret = write_register16(INA226_REG_CALIBRATION, calibration_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write calibration register");
        return ret;
    }
    
    // Wait for calibration to take effect (INA226 needs time to process)
    // According to Tab5 MicroPython library: INA226 requires ~200ms delay after initialization
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Verify calibration register was written correctly
    uint16_t cal_readback = 0;
    esp_err_t cal_err = ina226_read_u16(INA226_REG_CALIBRATION, &cal_readback);
    if (cal_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read back calibration register: %s", esp_err_to_name(cal_err));
        return ESP_FAIL;
    }
    
    if (cal_readback != calibration_value) {
        ESP_LOGW(TAG, "Calibration register mismatch: wrote 0x%04X, read 0x%04X", 
                 calibration_value, cal_readback);
    } else {
        ESP_LOGI(TAG, "Calibration register verified: 0x%04X", calibration_value);
    }
    
    initialized = true;
    
    // Test read voltage
    float test_voltage = read_bus_voltage();
    if (test_voltage > 0) {
        ESP_LOGI(TAG, "Bus voltage: %.3fV", test_voltage);
    } else {
        ESP_LOGW(TAG, "Bus voltage read failed");
    }
    
    // Test read shunt voltage (for debugging)
    int16_t shunt_raw = 0;
    esp_err_t shunt_err = ina226_read_s16(INA226_REG_SHUNTVOLTAGE, &shunt_raw);
    if (shunt_err == ESP_OK) {
        float shunt_voltage = (float)shunt_raw * 0.0000025f;  // 2.5µV LSB
        ESP_LOGI(TAG, "Shunt voltage: %.6fV (raw: %d)", shunt_voltage, shunt_raw);
    } else {
        ESP_LOGW(TAG, "Shunt voltage read failed: %s", esp_err_to_name(shunt_err));
    }
    
    // Test read current (should work now)
    float test_current = read_current();
    ESP_LOGI(TAG, "Test current read: %.6fA", test_current);
    
    // Dump all registers after initialization
    ESP_LOGI(TAG, "Dumping INA226 registers after initialization:");
    dump_ina226_registers();
    
    return ESP_OK;
}

// Classify voltage into LOW/HIGH/MID
static voltage_class_t classify_voltage(int voltage_mv) {
    if (voltage_mv < V_LOW_THRESHOLD_MV) {
        return VCLS_LOW;
    }
    if (voltage_mv > V_HIGH_THRESHOLD_MV) {
        return VCLS_HIGH;
    }
    return VCLS_MID;  // Ignored for decision
}

// Push new classification into window and update counters
static void push_voltage_class(voltage_class_t cls) {
    // Remove old value from counters
    if (window_filled) {
        voltage_class_t old = voltage_window[window_index];
        if (old == VCLS_LOW) {
            low_count--;
        } else if (old == VCLS_HIGH) {
            high_count--;
        }
    }
    
    // Add new value
    voltage_window[window_index] = cls;
    if (cls == VCLS_LOW) {
        low_count++;
    } else if (cls == VCLS_HIGH) {
        high_count++;
    }
    
    // Advance window
    window_index = (window_index + 1) % WINDOW_SIZE;
    if (window_index == 0) {
        window_filled = true;
    }
}

// Update battery presence using voting algorithm
static battery_state_t update_battery_presence(int voltage_mv) {
    voltage_class_t cls = classify_voltage(voltage_mv);
    push_voltage_class(cls);
    
    int window_size = window_filled ? WINDOW_SIZE : window_index;
    if (window_size < 5) {
        return cached_battery_state;  // Not enough data yet
    }
    
    int need_count = (window_size * NEED_PERCENT) / 100;
    
    if (high_count >= need_count) {
        cached_battery_state = BAT_PRESENT;
    } else if (low_count >= need_count) {
        cached_battery_state = BAT_ABSENT;
    }
    // Otherwise keep previous state
    
    return cached_battery_state;
}

esp_err_t battery_monitor_read(battery_status_t *status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!initialized) {
        status->initialized = false;
        status->level = -1;
        status->voltage_mv = -1;
        status->is_charging = false;
        return ESP_ERR_INVALID_STATE;
    }
    
    status->initialized = true;
    
    // Read bus voltage (this is the battery pack voltage for 2S Li-Po)
    float bus_voltage = read_bus_voltage();
    if (bus_voltage < 0) {
        status->level = -1;
        status->voltage_mv = -1;
        status->is_charging = false;
        return ESP_FAIL;
    }
    
    // bus_voltage is already in volts (e.g., 7.602V for 2S battery)
    // Convert to mV: multiply by 1000
    int raw_voltage_mv = (int)(bus_voltage * 1000.0f);
    
    // INA226 measures VSYS (~0.98V), not VBAT (6.0-8.4V)
    // If voltage is too low (< 3V), it's VSYS, not battery voltage
    // Mark as invalid - will be calculated from battery level later
    if (raw_voltage_mv < 3000) {
        // This is VSYS, not VBAT - mark as invalid for now
        status->voltage_mv = -1;
    } else {
        // Valid voltage reading (shouldn't happen with Tab5, but keep for compatibility)
        status->voltage_mv = raw_voltage_mv;
    }
    
    // Read current from shunt voltage (more reliable than CURRENT register)
    // Returns current in mA (signed: negative = charging, positive = discharging)
    float current_ma = read_current_from_shunt_ma_internal();
    
    // Check USB-C presence (for context)
    bool usb_present = bsp_usb_c_detect();
    
    // Determine charging/discharging status based on current sign
    // Negative current = charging (current flows INTO battery)
    // Positive current = discharging (current flows OUT of battery)
    bool charging = (current_ma < -BATTERY_CHARGE_THRESHOLD_MA);
    bool discharging = (current_ma > BATTERY_DISCHARGE_THRESHOLD_MA);
    
    status->is_charging = charging;
    
    // Determine battery presence using voltage classification voting (Variant A)
    // This prevents "jumping" between states by using voting over a window instead of median filter
    battery_state_t bat_state = update_battery_presence(status->voltage_mv);
    bool battery_present = (bat_state == BAT_PRESENT);
    
    // FALLBACK: If voltage-based detection failed, use current-based detection
    // This is needed because bus voltage may not reflect battery voltage (INA226 measures VSYS, not VBAT)
    // Note: Trickle current ~5mA can exist even without battery (protection mosfet + presence sense circuit)
    // So threshold must be above 5mA to avoid false positives
    if (!battery_present) {
        float abs_current = fabsf(current_ma);
        
        if (usb_present) {
            // USB connected: any significant current (> 10mA) indicates battery present
            // Trickle current without battery is ~5mA, so 10mA threshold avoids false positives
            if (abs_current > 10.0f) {
                ESP_LOGI(TAG, "Voltage detection: ABSENT, but significant current detected (%.1f mA) with USB - battery PRESENT", current_ma);
                battery_present = true;
                bat_state = BAT_PRESENT;
            }
            // Strong charging (> 30mA) definitely indicates battery
            else if (current_ma < -30.0f) {
                ESP_LOGI(TAG, "Voltage detection: ABSENT, but strong charging current detected (%.1f mA) - battery PRESENT", current_ma);
                battery_present = true;
                bat_state = BAT_PRESENT;
            }
        }
        // USB not connected: discharging (> 10mA) indicates battery present
        else if (current_ma > 10.0f) {
            ESP_LOGI(TAG, "Voltage detection: ABSENT, but discharge current detected (%.1f mA) - battery PRESENT", current_ma);
            battery_present = true;
            bat_state = BAT_PRESENT;
        }
    }
    
    // Get shunt voltage and current for detection check
    int shunt_uv = 0;
    float current_ma_check = 0.0f;
    esp_err_t shunt_err = battery_monitor_get_shunt_voltage_uv(&shunt_uv);
    esp_err_t current_err = battery_monitor_get_current_ma(&current_ma_check);
    
    // Temporary fix: if USB is connected but no current detected, assume no battery
    if (usb_present && battery_present && shunt_err == ESP_OK && current_err == ESP_OK) {
        float abs_current = fabsf(current_ma_check);
        int shunt_uv_abs = (shunt_uv < 0) ? -shunt_uv : shunt_uv;
        
        // If USB connected but current is essentially zero for extended period, likely no battery
        static int zero_current_count = 0;
        if (abs_current < 1.0f && shunt_uv_abs < 10) {  // Less than 1mA and <10µV shunt
            zero_current_count++;
            if (zero_current_count >= 30) {  // 3 seconds at 100ms rate
                ESP_LOGW(TAG, "USB connected but no current detected for 3s - assuming NO BATTERY");
                battery_present = false;
                bat_state = BAT_ABSENT;
                zero_current_count = 0;
            }
        } else {
            zero_current_count = 0;  // Reset counter if current detected
        }
    }
    
    status->battery_present = battery_present;
    
    // Periodic register dump for debugging
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    static uint32_t last_dump_time = 0;
    if ((now - last_dump_time) >= 5000) {  // Every 5 seconds
        dump_ina226_registers();
        last_dump_time = now;
    }
    
    // Log battery check occasionally
    static uint32_t last_battery_log = 0;
    if ((now - last_battery_log) >= 2000) {
        int window_size = window_filled ? WINDOW_SIZE : window_index;
        ESP_LOGI(TAG, "Battery check: state=%s (LOW=%d HIGH=%d/%d), voltage=%d mV, current=%.2f mA, USB=%d", 
                 bat_state == BAT_PRESENT ? "PRESENT" : (bat_state == BAT_ABSENT ? "ABSENT" : "UNKNOWN"),
                 low_count, high_count, window_size,
                 status->voltage_mv, current_ma, usb_present);
        last_battery_log = now;
    }
    
    // If USB is connected but battery not detected, show "NO BATTERY"
    if (usb_present && !battery_present && status->voltage_mv > 8000) {
        // USB power without battery - voltage is high but no current flow
        status->level = -1;
        return ESP_OK;
    }
    
    // INA226 measures VSYS (~0.98V), not VBAT (6.0-8.4V)
    // We cannot directly read battery voltage, so we estimate level from charging current
    // Calibration: At 670mA charging current, battery level is 75%
    
    if (!battery_present) {
        status->level = -1;
        return ESP_OK;
    }
    
    // Estimate battery level based on charging/discharging behavior
    // Static variable to store last known battery level
    // Load from NVS on first call, then use cached value
    static int last_known_level = -1;  // -1 = not loaded yet
    static bool level_loaded_from_nvs = false;
    static uint32_t last_save_time_ms = 0;
    
    // Load level from NVS on first call
    if (!level_loaded_from_nvs) {
        int nvs_level = load_battery_level_from_nvs();
        if (nvs_level >= 0) {
            last_known_level = nvs_level;
            ESP_LOGI(TAG, "Loaded battery level from NVS: %d%%", last_known_level);
        } else {
            last_known_level = 75;  // Default if not found in NVS
            ESP_LOGI(TAG, "No battery level in NVS, using default: %d%%", last_known_level);
        }
        level_loaded_from_nvs = true;
    }
    
    // Get current time for integration
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Detect charging state with hysteresis (edge detection) - based on current, not USB!
    // Hysteresis: enter charging < -20mA, exit charging > -5mA
    if (!charging_latched) {
        charging_latched = (current_ma < -20.0f);  // Enter: stricter threshold
    } else {
        charging_latched = (current_ma < -5.0f);   // Exit: softer threshold
    }
    bool charging_now = charging_latched;
    
    if (charging_now && !was_charging) {
        // Edge: charging just started
        charge_state = CHG_START_DETECT;
        charge_start_time_ms = now_ms;
        last_charge_current = 0.0f;
        chg_max_current_ma = 0.0f;  // Reset maximum for new window
        charged_mah = 0.0f;
        initial_level_on_charge = -1;
        
        // Reset discharge tracking
        discharged_mah = 0.0f;
        initial_level_on_discharge = -1;
        
        ESP_LOGI(TAG, "Charging started: IDLE -> START_DETECT");
    }
    
    if (charge_state == CHG_START_DETECT) {
        // Check: if charging stopped before window completion - exit gracefully
        if (!charging_now) {
            charge_state = CHG_IDLE;
            initial_level_on_charge = -1;
            charged_mah = 0.0f;
            chg_max_current_ma = 0.0f;
            ESP_LOGI(TAG, "Charging stopped early: START_DETECT -> IDLE");
        } else {
            // Collect maximum current in ~1.5 second window
            float abs_charge_current = -current_ma;
            
            // Update maximum current
            if (abs_charge_current > chg_max_current_ma) {
                chg_max_current_ma = abs_charge_current;
            }
            
            // Optional: log rapid increase (for debugging)
            if (last_charge_current > 0.0f) {
                float current_increase = abs_charge_current - last_charge_current;
                uint32_t time_since_start = now_ms - charge_start_time_ms;
                if (current_increase > 200.0f && time_since_start < 5000) {
                    ESP_LOGW(TAG, "Rapid current increase detected: %.1f -> %.1f mA (%.1f mA increase)", 
                             last_charge_current, abs_charge_current, current_increase);
                }
            }
            
            // CRITICAL: update every tick for next iteration
            last_charge_current = abs_charge_current;
            
            // Wait ~1.5 seconds to see maximum startup current
            if ((now_ms - charge_start_time_ms) >= 1500) {
                // Use maximum current in window to determine initial level
                float use_current = chg_max_current_ma;
                
                // Determine initial level based on MAXIMUM charging current in window
                if (use_current > 600.0f) {
                    // Very high charge current: battery is likely discharged
                    // More realistic thresholds:
                    // 850+ mA = 0% (fully discharged)
                    // 800-850 mA = 0-5% (very low charge)
                    // 700-800 mA = 5-15% (low charge)
                    // 600-700 mA = 15-25% (partially discharged)
                    float estimated_level;
                    if (use_current > 850.0f) {
                        // Very high current (>850mA): battery fully discharged
                        estimated_level = 0.0f;
                    } else if (use_current > 800.0f) {
                        // High current (800-850mA): very low charge, 0-5%
                        estimated_level = ((850.0f - use_current) / 50.0f) * 5.0f;
                    } else if (use_current > 700.0f) {
                        // Medium-high current (700-800mA): low charge, 5-15%
                        estimated_level = 5.0f + ((800.0f - use_current) / 100.0f) * 10.0f;
                    } else {
                        // Medium current (600-700mA): partially discharged, 15-25%
                        estimated_level = 15.0f + ((700.0f - use_current) / 100.0f) * 10.0f;
                    }
                    initial_level_on_charge = (int)estimated_level;
                    ESP_LOGW(TAG, "High charge current (%.1f mA max) detected - starting from %d%%", 
                             use_current, initial_level_on_charge);
                } else if (use_current > 300.0f) {
                    // Medium charge current: use last known level, but be conservative if it's 0%
                    if (last_known_level == 0) {
                        // If NVS had 0% but current is medium, battery may be higher
                        initial_level_on_charge = 20;  // Conservative estimate for medium current
                        ESP_LOGI(TAG, "Medium charge current (%.1f mA max) with 0%% in NVS - using conservative estimate: %d%%", 
                                 use_current, initial_level_on_charge);
                    } else {
                        initial_level_on_charge = (last_known_level >= 0) ? last_known_level : 50;
                        ESP_LOGI(TAG, "Medium charge current (%.1f mA max) - using last known level: %d%%", 
                                 use_current, initial_level_on_charge);
                    }
                } else {
                    // Low charge current: battery is likely nearly full
                    initial_level_on_charge = (last_known_level >= 0 && last_known_level > 70) ? last_known_level : 70;
                    ESP_LOGI(TAG, "Low charge current (%.1f mA max) - battery likely nearly full: %d%%", 
                             use_current, initial_level_on_charge);
                }
                
                // LOCK: Transition to CHG_ACTIVE - NEVER change initial_level_on_charge again
                charge_state = CHG_ACTIVE;
                last_charge_update_time_ms = now_ms;
                ESP_LOGI(TAG, "Charge init done: maxI=%.1f mA, initial=%d%%, START_DETECT -> ACTIVE", 
                         use_current, initial_level_on_charge);
            }
        }
    }
    
    if (charge_state == CHG_ACTIVE) {
        // Coulomb Counting - only current integration, NO recalculations
        float abs_charge_current = -current_ma;
        
        if (initial_level_on_charge >= 0) {
            float dt_hours = (float)(now_ms - last_charge_update_time_ms) / 3600000.0f;
            if (dt_hours < 0.0f) dt_hours = 0.0f;  // Protection against negative time
            
            float charge_current_ma = abs_charge_current;
            
            // Add charged capacity: Q = I * t
            charged_mah += charge_current_ma * dt_hours;
            
            // Calculate level: Level = Initial + (Charged / Capacity) * 100
            const float battery_capacity_mah = 2000.0f;
            float level_percent = initial_level_on_charge + (charged_mah / battery_capacity_mah) * 100.0f;
            
            status->level = (int)level_percent;
            if (status->level < 0) status->level = 0;
            if (status->level > 100) status->level = 100;  // FIXED: 100 instead of 77
            
            // Update last known level
            last_known_level = status->level;
            
            // Save to NVS periodically (every 10 seconds) or on significant change
            if ((now_ms - last_save_time_ms) >= 10000) {
                save_battery_level_to_nvs(status->level);
                last_save_time_ms = now_ms;
            }
            
            // Log periodically for debugging
            static uint32_t last_charge_log = 0;
            if ((now_ms - last_charge_log) >= 5000) {
                ESP_LOGI(TAG, "Charging: current=%.1f mA, charged=%.2f mAh, level=%d%%", 
                         charge_current_ma, charged_mah, status->level);
                last_charge_log = now_ms;
            }
        }
        
        last_charge_update_time_ms = now_ms;
    }
    
    // Exit from charging
    if (!charging_now && was_charging) {
        // Charging stopped
        if (charge_state == CHG_ACTIVE && status->level >= 0) {
            // Save final level to NVS
            save_battery_level_to_nvs(status->level);
            ESP_LOGI(TAG, "Charging stopped, final level: %d%%, ACTIVE -> IDLE", status->level);
        } else if (charge_state == CHG_START_DETECT) {
            ESP_LOGI(TAG, "Charging stopped, START_DETECT -> IDLE");
        }
        charge_state = CHG_IDLE;
        charged_mah = 0.0f;
        initial_level_on_charge = -1;
        last_charge_update_time_ms = 0;
        chg_max_current_ma = 0.0f;  // Reset maximum
    }
    
    was_charging = charging_now;
    
    // Handle discharging (only if not charging)
    if (!charging_now && ((!usb_present && current_ma > 1.0f) || (usb_present && current_ma > 10.0f))) {
        // Discharging: use coulomb counting
        // Without USB: track even small currents (1-5mA for self-discharge and idle consumption)
        // With USB: only track significant discharge (> 10mA, USB power output)
        // Tab5 battery: NP-F550, 2000mAh capacity
        
        // Reset charge tracking when discharging starts
        if (charge_state != CHG_IDLE) {
            charge_state = CHG_IDLE;
            charged_mah = 0.0f;
            initial_level_on_charge = -1;
            ESP_LOGI(TAG, "Discharge started, reset charge tracking");
        }
        
        // Initialize discharge tracking if this is the first discharge reading
        if (initial_level_on_discharge < 0) {
            // Use last known level from charging as starting point
            initial_level_on_discharge = (last_known_level >= 0) ? last_known_level : 75;
            discharged_mah = 0.0f;
            last_discharge_update_time_ms = now_ms;
            ESP_LOGI(TAG, "Discharge started (USB=%d, current=%.1f mA), initial level: %d%%", 
                     usb_present ? 1 : 0, current_ma, initial_level_on_discharge);
        }
        
        // Integrate discharge current over time
        if (last_discharge_update_time_ms > 0) {
            float dt_hours = (float)(now_ms - last_discharge_update_time_ms) / 3600000.0f;  // Convert ms to hours
            float discharge_current_ma = current_ma;  // Positive current = discharging
            
            // Add discharged capacity: Q = I * t
            discharged_mah += discharge_current_ma * dt_hours;
            
            // Calculate level: Level = Initial - (Discharged / Capacity) * 100
            // Tab5 battery capacity: 2000mAh
            const float battery_capacity_mah = 2000.0f;
            float level_percent = initial_level_on_discharge - (discharged_mah / battery_capacity_mah) * 100.0f;
            
            status->level = (int)level_percent;
            if (status->level < 0) status->level = 0;
            if (status->level > 100) status->level = 100;  // FIXED: 100 instead of 77
            
            // Update last known level
            last_known_level = status->level;
            
            // Save to NVS periodically (every 10 seconds) or on significant change
            if ((now_ms - last_save_time_ms) >= 10000) {
                save_battery_level_to_nvs(status->level);
                last_save_time_ms = now_ms;
            }
            
            // Log periodically for debugging
            static uint32_t last_discharge_log = 0;
            if ((now_ms - last_discharge_log) >= 5000) {  // Every 5 seconds
                ESP_LOGI(TAG, "Discharging: current=%.1f mA, discharged=%.2f mAh, level=%d%%", 
                         discharge_current_ma, discharged_mah, status->level);
                last_discharge_log = now_ms;
            }
        } else {
            // First reading, use initial level
            status->level = initial_level_on_discharge;
        }
        
        last_discharge_update_time_ms = now_ms;
        
    } else if (!charging_now && !usb_present) {
        // USB disconnected, no significant current: use last known level
        // Reset discharge and charge tracking
        if (charge_state != CHG_IDLE) {
            charge_state = CHG_IDLE;
            charged_mah = 0.0f;
            initial_level_on_charge = -1;
        }
        
        if (last_known_level >= 0) {
            status->level = last_known_level;
        } else {
            status->level = 75;  // Default
            last_known_level = 75;
            save_battery_level_to_nvs(last_known_level);  // Save default
        }
        
    } else if (!charging_now) {
        // USB connected but idle (very small current, |current| <= 10mA): use last known level
        status->level = (last_known_level >= 0) ? last_known_level : 75;
    }
    
    // If voltage was invalid (VSYS), estimate from battery level
    // Calibration: Level 76.5% = 8.122V = 100% real charge (charging stops at this level)
    // User observation: 76-77% displayed = 100% real charge, voltage 8.122V
    if (status->voltage_mv < 0 && status->level >= 0) {
        // Estimate battery voltage from level
        // Calibration point: 76.5% = 8.122V (100% real charge)
        // Linear interpolation: V = 6.0V (0%) to 8.122V (76.5% = 100% real)
        float level_f = (float)status->level;
        float estimated_voltage_v;
        
        if (level_f <= 76.5f) {
            // Below calibration point: linear from 6.0V to 8.122V
            estimated_voltage_v = 6.0f + (level_f / 76.5f) * 2.122f;
        } else {
            // Above calibration point: remains 8.122V (charging stops, max voltage)
            estimated_voltage_v = 8.122f;
        }
        
        status->voltage_mv = (int)(estimated_voltage_v * 1000.0f);
    }
    
    return ESP_OK;
}

int battery_monitor_get_level(void)
{
    battery_status_t status = {0};
    if (battery_monitor_read(&status) == ESP_OK) {
        return status.level;
    }
    return -1;
}

int battery_monitor_get_voltage_mv(void)
{
    battery_status_t status = {0};
    if (battery_monitor_read(&status) == ESP_OK) {
        return status.voltage_mv;
    }
    return -1;
}

bool battery_monitor_is_charging(void)
{
    battery_status_t status = {0};
    if (battery_monitor_read(&status) == ESP_OK) {
        return status.is_charging;
    }
    return false;
}

esp_err_t battery_monitor_get_current_ma(float *out_ma)
{
    if (!initialized || !out_ma) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int16_t shunt_raw = 0;
    esp_err_t err = ina226_read_s16(INA226_REG_SHUNTVOLTAGE, &shunt_raw);
    if (err != ESP_OK) {
        return err;
    }
    
    // Shunt voltage LSB = 2.5µV = 0.0000025V
    float shunt_voltage_v = (float)shunt_raw * 0.0000025f;
    
    // Current = Shunt Voltage / R_shunt
    // R_shunt = 0.005 ohm (5 mOhm)
    float current_a = shunt_voltage_v / 0.005f;
    
    *out_ma = current_a * 1000.0f; // Convert to mA (signed: negative = charging)
    return ESP_OK;
}

esp_err_t battery_monitor_get_shunt_voltage_uv(int *out_uv)
{
    if (!initialized || !out_uv) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int16_t shunt_raw = 0;
    esp_err_t err = ina226_read_s16(INA226_REG_SHUNTVOLTAGE, &shunt_raw);
    if (err != ESP_OK) {
        return err;
    }
    
    // Shunt voltage LSB = 2.5µV, can be negative (charging)
    *out_uv = (int)(shunt_raw * 2.5f);
    return ESP_OK;
}
