/*
 * UART Bridge for CrowPanel ESP32-P4 -> Mac Studio
 * Runs on M5Stack Cardputer v1.1 (ESP32-S3)
 *
 * Receives ESP_LOG data from CrowPanel UART1 (GPIO47/48)
 * via Cardputer Grove Port and forwards to USB Serial Monitor.
 *
 * Wiring (CrowPanel J2 -> Cardputer Grove):
 *   CrowPanel GPIO47 (TX) -> Cardputer G1 (RX)
 *   CrowPanel GPIO48 (RX) <- Cardputer G2 (TX)  [optional]
 *   CrowPanel GND         -> Cardputer GND
 *   DO NOT connect VCC between boards!
 */

#include <M5Cardputer.h>

#define CROWN_RX_PIN  1   // Cardputer G1 <- CrowPanel TX (GPIO47)
#define CROWN_TX_PIN  2   // Cardputer G2 -> CrowPanel RX (GPIO48)
#define BAUD_RATE     115200

HardwareSerial CrownSerial(1);

static uint32_t bytes_received = 0;
static uint32_t last_status_ms = 0;

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);

    Serial.begin(BAUD_RATE);
    CrownSerial.begin(BAUD_RATE, SERIAL_8N1, CROWN_RX_PIN, CROWN_TX_PIN);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5Cardputer.Display.setTextSize(1.5);
    M5Cardputer.Display.setCursor(0, 0);
    M5Cardputer.Display.println("=== UART Bridge ===");
    M5Cardputer.Display.println("CrowPanel -> Mac");
    M5Cardputer.Display.printf("RX pin: G%d\n", CROWN_RX_PIN);
    M5Cardputer.Display.printf("Baud: %d\n", BAUD_RATE);
    M5Cardputer.Display.println("Waiting for data...");

    Serial.println("\n=== CrowPanel UART Bridge ===");
    Serial.printf("RX from CrowPanel on GPIO%d at %d baud\n", CROWN_RX_PIN, BAUD_RATE);
    Serial.println("--- Log output below ---\n");
}

void loop() {
    while (CrownSerial.available()) {
        int c = CrownSerial.read();
        Serial.write(c);
        bytes_received++;
    }

    // Forward Mac -> CrowPanel (for interactive console)
    while (Serial.available()) {
        int c = Serial.read();
        CrownSerial.write(c);
    }

    if (millis() - last_status_ms > 5000) {
        last_status_ms = millis();
        M5Cardputer.Display.fillRect(0, 70, 240, 30, TFT_BLACK);
        M5Cardputer.Display.setCursor(0, 70);
        M5Cardputer.Display.printf("Bytes: %lu  T: %lus", bytes_received, millis() / 1000);
    }
}
