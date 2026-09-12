#pragma once

#include <Arduino.h>

namespace rtk {

constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr size_t DEVICE_ID_LEN = 17;
constexpr size_t ID_LEN = 33;
constexpr size_t NAME_LEN = 48;
constexpr size_t GGA_LEN = 96;
constexpr size_t RTCM_FRAGMENT_BYTES = 180;

enum class MsgType : uint8_t {
  TELEMETRY = 1,
  MEASUREMENT = 2,
  EVENT = 3,
  SESSION = 4,
  HEARTBEAT = 5,
  ACK = 6,
  RTCM_FRAGMENT = 0x81,
  ARM_MEASUREMENT = 0x82,
  SESSION_CONTROL = 0x83,
  CONFIG = 0x84,
  TIME_SYNC = 0x85,
};

struct __attribute__((packed)) Header {
  uint8_t version;
  MsgType type;
  uint16_t payload_size;
  uint32_t seq;
  char message_id[ID_LEN];
  char source_device_id[DEVICE_ID_LEN];
};

struct __attribute__((packed)) GnssState {
  double lat;
  double lon;
  float altitude_m;
  float hdop;
  float speed_mps;
  float course_deg;
  uint8_t fix_quality;
  uint8_t satellites;
  uint32_t gnss_age_ms;
  char gnss_utc[24];
  char gga[GGA_LEN];
};

struct __attribute__((packed)) TelemetryPayload {
  Header header;
  GnssState gnss;
  uint32_t uptime_ms;
  char session_id[ID_LEN];
};

struct __attribute__((packed)) MeasurementPayload {
  Header header;
  GnssState gnss;
  char measurement_id[ID_LEN];
  char session_id[ID_LEN];
  char name[NAME_LEN];
  uint8_t trigger;
};

struct __attribute__((packed)) EventPayload {
  Header header;
  char event_id[ID_LEN];
  char session_id[ID_LEN];
  char event_type[24];
  char detail[64];
};

struct __attribute__((packed)) SessionPayload {
  Header header;
  char session_id[ID_LEN];
  uint8_t active;
};

struct __attribute__((packed)) AckPayload {
  Header header;
  char ack_id[ID_LEN];
  uint8_t ok;
};

struct __attribute__((packed)) ArmPayload {
  Header header;
  char command_id[ID_LEN];
  char measurement_id[ID_LEN];
  char session_id[ID_LEN];
  char name[NAME_LEN];
};

struct __attribute__((packed)) SessionControlPayload {
  Header header;
  char command_id[ID_LEN];
  char session_id[ID_LEN];
  uint8_t active;
};

struct __attribute__((packed)) RtcmFragmentPayload {
  Header header;
  uint32_t rtcm_message_id;
  uint16_t fragment_index;
  uint16_t fragment_count;
  uint16_t data_len;
  uint8_t data[RTCM_FRAGMENT_BYTES];
};

inline void makeId(char out[ID_LEN]) {
  uint64_t a = esp_random();
  uint64_t b = esp_random();
  snprintf(out, ID_LEN, "%08lx%08lx%08lx%08lx",
           (unsigned long)(a & 0xffffffff),
           (unsigned long)(esp_random()),
           (unsigned long)(b & 0xffffffff),
           (unsigned long)(esp_random()));
}

inline void initHeader(Header &h, MsgType type, uint16_t payload_size,
                       uint32_t seq, const char *device_id) {
  memset(&h, 0, sizeof(h));
  h.version = PROTOCOL_VERSION;
  h.type = type;
  h.payload_size = payload_size;
  h.seq = seq;
  makeId(h.message_id);
  strlcpy(h.source_device_id, device_id, sizeof(h.source_device_id));
}

}  // namespace rtk
