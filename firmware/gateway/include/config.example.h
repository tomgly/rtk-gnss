#pragma once

#define GATEWAY_DEVICE_ID "gateway-rev1"
#define RGB_LED_PIN 21

// Cloudflare API base URL. Set your deployed workers.dev URL in local_config.h.
#define API_BASE_URL "https://example.workers.dev"

#define WIFI_CONNECT_TIMEOUT_MS 8000
#define WIFI_RESCAN_INTERVAL_MS 30000
#define COMMAND_POLL_INTERVAL_MS 2000
#define UPLOAD_RETRY_INTERVAL_MS 5000
#define TELEMETRY_BOUNDARY_SECONDS 10
#define ROVER_TIMEOUT_MS 3000

// Replace with the real Rover ESP32 Wi-Fi MAC in local_config.h.
#define ROVER_MAC {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
