# Tab5 Battery Charger Test

**Project:** `tab5_battery_charger_test`  
**Platform:** M5Stack Tab5 (ESP32-P4)  
**Framework:** ESP-IDF  
**Date:** January 4, 2026

## Overview

Simple battery monitoring and charging test application for M5Stack Tab5. Displays real-time battery telemetry on screen including voltage, current, charge level, and status.

## Features

- ✅ Real-time battery voltage monitoring (INA226)
- ✅ Current measurement (charging/discharging) with signed values
- ✅ **Coulomb Counting** for accurate battery level tracking
  - Charge integration: tracks energy added during charging
  - Discharge integration: tracks energy consumed during discharge
  - Real-time updates during charge/discharge cycles
- ✅ Battery level calculation (0-100%) based on current integration
- ✅ Battery presence detection (two-threshold classifier with hysteresis)
- ✅ USB-C connection detection
- ✅ Charging status display
- ✅ Visual battery icon with fill level
- ✅ Automatic charging enable
- ✅ Voltage estimation from battery level (INA226 measures VSYS, not VBAT)

## Hardware

- **Battery:** 2S Li-Po (NP-F550, 7.4V nominal, 6.0V empty, 8.4V full)
- **Monitor:** INA226 @ I2C 0x41
- **Charger:** IP2326 (managed automatically)
- **Display:** ST7123 720×1280 portrait

## Display Information

The screen shows:
- **Voltage:** Battery voltage in millivolts (estimated from level if VSYS detected)
  - Shows "N/A (VSYS)" if INA226 measures VSYS instead of VBAT
  - Estimated voltage based on battery level: 6.0V (0%) → 8.122V (76.5% = 100% real charge)
  - **Note:** 76-77% displayed = 100% real charge (charging stops at 8.122V)
- **Current:** Current in milliamperes (negative = charging, positive = discharging)
  - Precision: 3 decimal places (0.000 mA)
  - Correctly handles signed values (negative for charging)
- **Level:** Battery percentage (0-77%) calculated via **Coulomb Counting**
  - Updates in real-time during charge/discharge
  - Based on current integration over time
  - **Important:** 76-77% displayed = 100% real charge (charging stops at this level)
  - Calibrated: 76.5% = 8.122V = 100% real charge (user-measured)
- **Status:** CHARGING / DISCHARGING / FULL/IDLE / NO BATTERY
- **USB:** CONNECTED / DISCONNECTED
- **Battery:** PRESENT / NOT PRESENT
- **Battery Icon:** Visual representation with color-coded fill

## Battery Detection Logic

Uses a **two-threshold classifier** with voting and hysteresis:

1. **Voltage Classification:**
   - LOW (< 5800 mV): Battery likely absent
   - HIGH (> 7200 mV): Battery likely present
   - MID (5800-7200 mV): Ambiguous, requires more samples

2. **Voting Window:**
   - Analyzes last 40 samples
   - Requires 80% HIGH votes for PRESENT
   - Requires 75% LOW votes for ABSENT

3. **Hysteresis:**
   - Requires 10 stable samples before state change
   - Prevents rapid state switching

4. **Fallback Logic:**
   - If voltage detection fails but significant current detected:
     - USB + charging current (> 30 mA) → Battery PRESENT
     - USB + idle current (> 10 mA) → Battery PRESENT
     - No USB + discharge current (> 10 mA) → Battery PRESENT

## Building

```bash
cd tab5_battery_charger_test
idf.py build
```

## Flashing

```bash
idf.py flash monitor
```

Or specify port:
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Usage

1. Flash the firmware to Tab5
2. Connect USB-C cable (optional, for charging)
3. Insert battery (optional, for testing)
4. Watch the display for real-time telemetry

## Test Scenarios

1. **No battery, USB-C connected:** Should show "NO BATTERY", voltage ~8300 mV
2. **Battery connected, USB-C disconnected:** Should show "DISCHARGING", voltage 6000-8400 mV
3. **Battery connected, USB-C connected:** Should show "CHARGING" if battery not full
4. **Battery full, USB-C connected:** Should show "FULL/IDLE", current ≈ 0 mA

## Components

- `main/app_main.cpp` - Main application code
- `components/battery_monitor/` - INA226 battery monitor component
- `components/m5stack_tab5/` - Tab5 BSP

## Battery Level Calculation (Coulomb Counting)

The battery level is calculated using **Coulomb Counting** (current integration):

### Charging Mode:
- Tracks `charged_mah` by integrating charge current over time
- Formula: `Level = Initial_Level + (Charged_mAh / 2000mAh) * 100`
- Initial level: Last known level or 75% (default)
- Updates every 100ms in real-time
- **Key feature:** Level doesn't jump when load is connected (current drops but integration continues)

### Discharging Mode:
- Tracks `discharged_mah` by integrating discharge current over time
- Formula: `Level = Initial_Level - (Discharged_mAh / 2000mAh) * 100`
- Works with or without USB connected (handles USB power output)
- Updates every 100ms in real-time

### Idle Mode:
- Uses last known level
- Resets integration counters when mode changes

### Voltage Estimation:
- INA226 measures VSYS (~1V), not VBAT
- Voltage is estimated from battery level using piecewise linear interpolation:
  - 0-76.5%: Linear from 6.0V to 8.122V
  - 76.5-77%: Remains 8.122V (charging stops, max voltage)
  - Calibration point: 76.5% = 8.122V = 100% real charge (user-measured)
  - **Important:** 76-77% displayed = 100% real charge (charging stops at this level)

## Notes

- Update rate: 100ms (10 Hz)
- Charging is automatically enabled on startup
- Display brightness: 80%
- Framebuffer: RGB565 format, allocated in PSRAM
- Battery capacity: 2000 mAh (NP-F550)
- INA226 calibration: 0x0D55 (3413) for ~2.6A max current
- Shunt resistor: 5 mΩ
- **Level calibration:** 76-77% displayed = 100% real charge (8.122V, charging stops)

## Limitations

Due to hardware architecture, INA226 measures VSYS (~1V) instead of VBAT directly. This results in:

- **No real-time voltage measurement:** Voltage is estimated from battery level using calibration
- **No voltage-based battery diagnostics:** Cannot detect battery degradation, overheating, or incorrect battery type by voltage alone
- **Level calibration required:** Battery level is calculated via Coulomb Counting, requiring initial calibration point (76.5% = 8.122V)
- **Small discharge currents:** Discharge tracking works for currents > 1mA (without USB) or > 10mA (with USB)

## Known Issues

- Voltage display shows estimated value (based on level) when VSYS is detected
- Battery level may require recalibration if battery capacity changes significantly
- Very small idle currents (< 1mA) may not be tracked accurately

## Troubleshooting

### Current always shows 0.00 mA
- Check INA226 I2C connection
- Verify shunt resistor is connected
- Check calibration values
- Verify INA226 address is 0x41 (not 0x40)

### Voltage jumps randomly
- This is expected: INA226 measures VSYS, not VBAT
- Voltage is estimated from battery level
- Check I2C bus stability if readings are completely erratic

### Battery not detected
- Verify current measurement (should be non-zero if battery present)
- Check USB-C detection works correctly
- Battery detection uses current-based fallback when voltage detection fails

## Version History

- **v1.3** (2026-01-04): Small Discharge Current Tracking
  - ✅ Added tracking for small discharge currents (> 1mA without USB)
  - ✅ Improved accuracy for self-discharge and idle consumption
  - ✅ Fixed level calculation when discharging without load

- **v1.2** (2026-01-04): Level Calibration Update
  - ✅ Updated level calibration: 76-77% = 100% real charge (8.122V)
  - ✅ Level limited to 77% maximum (charging stops at this level)
  - ✅ Updated voltage estimation: 76.5% = 8.122V = 100% real charge
  - ✅ Level display scaled to 0-100% (77% internal = 100% displayed)

- **v1.1** (2026-01-04): Coulomb Counting Implementation
  - ✅ Implemented Coulomb Counting for charge/discharge tracking
  - ✅ Real-time battery level updates during charge/discharge
  - ✅ Voltage estimation from battery level (VSYS → VBAT)
  - ✅ Fixed level calculation: no jumps when load connected during charging
  - ✅ Two-threshold classifier for stable battery detection
  - ✅ Signed current reading (negative = charging)
  - ✅ INA226 register read refactoring (esp_err_t + out-parameters)
  - ✅ Improved USB detection logic
  - ✅ Calibrated voltage estimation: 75% = 8.11V (updated to 76.5% = 8.122V in v1.2)

- **v1.0** (2026-01-04): Initial release
  - Basic battery monitoring
  - Display telemetry
  - Charging enable
  - Battery detection
