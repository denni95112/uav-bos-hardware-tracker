#pragma once

// Heltec Wireless Tracker V1.1 compatible pinout (Fastsaw clone, ESP32-S3 + UC6580 + ST7735).
// Cross-checked with the Meshtastic heltec_wireless_tracker variant.

// Vext rail: powers GNSS, GNSS LNA and the TFT. Active HIGH on V1.1.
#define PIN_VEXT 3
#define VEXT_ON HIGH

// UC6580 GNSS (UART1). RX/TX are from the ESP32 point of view.
#define PIN_GNSS_RX 33
#define PIN_GNSS_TX 34
#define PIN_GNSS_RST 35 // active LOW
#define PIN_GNSS_PPS 36
#define GNSS_BAUD 115200

// ST7735S 0.96" 160x80 TFT
#define PIN_TFT_CS 38
#define PIN_TFT_DC 40
#define PIN_TFT_RST 39
#define PIN_TFT_SCLK 41
#define PIN_TFT_MOSI 42
#define PIN_TFT_BL 21 // backlight, active HIGH

// Panel window offsets in native portrait orientation. Adjust if the image is shifted
// or shows a noisy stripe on one edge.
#define TFT_COL_OFFSET 26
#define TFT_ROW_OFFSET 1
#define TFT_INVERT false
#define TFT_ROTATION 1 // 1 or 3 = landscape

// User button (PRG / BOOT), active LOW
#define PIN_BUTTON 0

// Battery measurement
#define PIN_BAT_ADC 1
#define PIN_BAT_ADC_CTRL 2 // HIGH enables the voltage divider
#define BAT_ADC_MULTIPLIER (4.9f * 1.045f)

#define PIN_LED 18
