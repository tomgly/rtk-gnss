#pragma once

// Copy this file to secrets.h. Never commit secrets.h.

struct WifiCredential {
  const char *ssid;
  const char *password;
};

static constexpr WifiCredential WIFI_NETWORKS[3] = {
    {"WIFI_SSID_1", "WIFI_PASSWORD_1"},
    {"WIFI_SSID_2", "WIFI_PASSWORD_2"},
    {"WIFI_SSID_3", "WIFI_PASSWORD_3"},
};

#define DEVICE_API_TOKEN "REPLACE_WITH_DEVICE_API_TOKEN"

// Optional NTRIP. Leave disabled until credentials/mountpoint are available.
#define NTRIP_ENABLED false
#define NTRIP_HOST ""
#define NTRIP_PORT 2101
#define NTRIP_MOUNTPOINT ""
#define NTRIP_USERNAME ""
#define NTRIP_PASSWORD ""
