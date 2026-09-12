#pragma once

#define GATEWAY_DEVICE_ID "gateway-rev1"
#define RGB_LED_PIN 21

// Cloudflare API base URL. Set your deployed workers.dev URL in local_config.h.
#define API_BASE_URL "https://example.workers.dev"

#define WIFI_CONNECT_TIMEOUT_MS 8000
#define WIFI_RESCAN_INTERVAL_MS 30000
#define COMMAND_POLL_INTERVAL_MS 2000
#define UPLOAD_RETRY_INTERVAL_MS 5000
#define LIVE_STATUS_INTERVAL_SECONDS 60
#define ROVER_TIMEOUT_MS 3000

// Locally administered Wi-Fi MAC addresses for the prototype pair.
#define DEVICE_MAC {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
#define ROVER_MAC {0x02, 0x00, 0x00, 0x00, 0x00, 0x02}
