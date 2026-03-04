# UART Bridge: CrowPanel ESP32-P4 -> Mac Studio

Uses M5Stack Cardputer v1.1 (ESP32-S3) as a UART bridge to forward ESP_LOG output from CrowPanel ESP32-P4 to your computer's Serial Monitor.

## Why?

CrowPanel ESP32-P4 UART output is not directly accessible via USB in some configurations. This bridge uses the Cardputer's Grove port to receive UART data and forward it over USB Serial.

## Wiring

| CrowPanel J2 | Cardputer Grove |
|--------------|-----------------|
| GPIO47 (TX)  | G1 (RX)         |
| GPIO48 (RX)  | G2 (TX) *optional* |
| GND          | GND             |

**Do NOT connect VCC between boards!**

## Features

- Bidirectional UART bridge (CrowPanel <-> Mac)
- 115200 baud
- Byte counter on Cardputer display (updates every 5 seconds)
- Interactive console support (type in Serial Monitor -> sends to CrowPanel)

## Usage

1. Flash `uart_bridge_crowpanel.ino` to Cardputer v1.1
2. Connect wires as shown above
3. Open Serial Monitor at 115200 baud
4. Power on CrowPanel -- ESP_LOG output appears in your Serial Monitor

## Board Settings (Arduino IDE)

| Setting | Value |
|---------|-------|
| Board | M5Stack-StampS3 (or M5Cardputer) |
| USB CDC On Boot | Enabled |
| Upload Speed | 921600 |

## Required Libraries

- M5Cardputer (via Library Manager)
