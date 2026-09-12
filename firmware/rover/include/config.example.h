#pragma once

// Public non-secret configuration. Copy to local_config.h if you want to override.
#define ROVER_DEVICE_ID "rover-rev1"

// Rev.1 confirmed wiring.
#define GNSS_RX_PIN 44
#define GNSS_TX_PIN 43
#define PPS_PIN 1
#define RGB_LED_PIN 21

// External add-on test button. GPIO2 is unused in the current Rev.1 design.
#define BUTTON_PIN 2

#define GNSS_BAUD 115200
#define TELEMETRY_INTERVAL_MS 1000
#define HEARTBEAT_INTERVAL_MS 1000
#define GATEWAY_TIMEOUT_MS 3000
#define DOUBLE_CLICK_MS 350
#define LONG_PRESS_MS 1800

// Replace with the real Gateway ESP32 Wi-Fi MAC address in local_config.h.
#define GATEWAY_MAC {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
