#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#if __has_include("local_config.h")
#include "local_config.h"
#else
#include "config.example.h"
#endif
#include "protocol.h"

using namespace rtk;

static uint8_t deviceMac[] = DEVICE_MAC;
static uint8_t gatewayMac[] = GATEWAY_MAC;
static Adafruit_NeoPixel led(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
static HardwareSerial gnssSerial(1);
static GnssState gnss{};
static uint32_t seqNo = 0;
static uint32_t lastTelemetryMs = 0;
static uint32_t lastHeartbeatMs = 0;
static uint32_t lastGatewayRxMs = 0;
static bool gatewayChannelLocked = false;
static uint32_t lastNmeaMs = 0;
static char sessionId[ID_LEN] = "";
static bool sessionActive = false;
static bool armed = false;
static char armedMeasurementId[ID_LEN] = "";
static char armedName[NAME_LEN] = "";
static char lastMeasurementId[ID_LEN] = "";
static char lastDate[7] = "";
static int lastLoggedGnssSecond = -1;

struct LedOverlay {
  uint32_t color = 0;
  uint32_t until = 0;
  uint16_t onMs = 100;
  uint16_t offMs = 100;
  uint8_t flashes = 0;
  uint32_t started = 0;
} overlay;

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) { return led.Color(r, g, b); }

static void setOverlay(uint32_t color, uint8_t flashes, uint16_t onMs = 90,
                       uint16_t offMs = 90) {
  overlay.color = color;
  overlay.flashes = flashes;
  overlay.onMs = onMs;
  overlay.offMs = offMs;
  overlay.started = millis();
  overlay.until = overlay.started + flashes * (onMs + offMs);
}

static uint32_t baseColor() {
  if (armed) return rgb(0, 0, 110);           // blue
  if (millis() - lastGatewayRxMs > GATEWAY_TIMEOUT_MS) return rgb(90, 0, 90);
  if (gnss.fix_quality == 4) return rgb(0, 100, 0);
  if (gnss.fix_quality == 5) return rgb(0, 80, 90);
  if (gnss.fix_quality == 1 || gnss.fix_quality == 2) return rgb(100, 65, 0);
  return rgb(100, 0, 0);
}

static void updateLed() {
  uint32_t now = millis();
  uint32_t c = baseColor();
  if (gnss.fix_quality == 0 && !armed && (now / 700) % 2 == 0) c = 0;
  if (millis() - lastGatewayRxMs > GATEWAY_TIMEOUT_MS && !armed && (now / 300) % 2 == 0) c = 0;

  if (overlay.flashes && now < overlay.until) {
    uint32_t elapsed = now - overlay.started;
    uint32_t cycle = overlay.onMs + overlay.offMs;
    if ((elapsed % cycle) < overlay.onMs) c = overlay.color;
  } else if (overlay.flashes) {
    overlay.flashes = 0;
  }
  led.setPixelColor(0, c);
  led.show();
}


static void rotateBackupLog() {
  if (LittleFS.exists("/rover.previous.jsonl")) LittleFS.remove("/rover.previous.jsonl");
  if (LittleFS.exists("/rover.jsonl")) LittleFS.rename("/rover.jsonl", "/rover.previous.jsonl");
}

static void appendJsonLine(const String &line) {
  File f = LittleFS.open("/rover.jsonl", FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
}

static double nmeaCoord(const char *value, const char hemi) {
  if (!value || !*value) return 0;
  double raw = atof(value);
  int degrees = (int)(raw / 100.0);
  double minutes = raw - degrees * 100.0;
  double result = degrees + minutes / 60.0;
  if (hemi == 'S' || hemi == 'W') result = -result;
  return result;
}

static int splitCsv(char *line, char *fields[], int maxFields) {
  int count = 0;
  char *p = line;
  while (p && count < maxFields) {
    fields[count++] = p;
    char *comma = strchr(p, ',');
    if (!comma) break;
    *comma = '\0';
    p = comma + 1;
  }
  return count;
}

static void buildGnssUtc(const char *hhmmss) {
  if (!hhmmss || strlen(hhmmss) < 6) return;
  if (strlen(lastDate) == 6) {
    // RMC date DDMMYY; preserve UTC in a compact ISO-like form.
    snprintf(gnss.gnss_utc, sizeof(gnss.gnss_utc), "20%c%c-%c%c-%c%cT%c%c:%c%c:%c%cZ",
             lastDate[4], lastDate[5], lastDate[2], lastDate[3], lastDate[0], lastDate[1],
             hhmmss[0], hhmmss[1], hhmmss[2], hhmmss[3], hhmmss[4], hhmmss[5]);
  } else {
    snprintf(gnss.gnss_utc, sizeof(gnss.gnss_utc), "%c%c:%c%c:%c%cZ",
             hhmmss[0], hhmmss[1], hhmmss[2], hhmmss[3], hhmmss[4], hhmmss[5]);
  }
}

static void parseNmea(const String &sentence) {
  if (!sentence.startsWith("$")) return;
  char buf[180];
  sentence.substring(0, sizeof(buf) - 1).toCharArray(buf, sizeof(buf));
  char original[180];
  strlcpy(original, buf, sizeof(original));
  char *star = strchr(buf, '*');
  if (star) *star = '\0';
  char *fields[24]{};
  int n = splitCsv(buf, fields, 24);
  if (n < 2) return;

  if (strstr(fields[0], "GGA")) {
    if (n < 10) return;
    gnss.lat = nmeaCoord(fields[2], fields[3][0]);
    gnss.lon = nmeaCoord(fields[4], fields[5][0]);
    gnss.fix_quality = (uint8_t)atoi(fields[6]);
    gnss.satellites = (uint8_t)atoi(fields[7]);
    gnss.hdop = atof(fields[8]);
    gnss.altitude_m = atof(fields[9]);
    gnss.gnss_age_ms = 0;
    lastNmeaMs = millis();
    strlcpy(gnss.gga, original, sizeof(gnss.gga));
    buildGnssUtc(fields[1]);

    if (strlen(fields[1]) >= 6) {
      int sec = (fields[1][4] - '0') * 10 + (fields[1][5] - '0');
      if (sec % 10 == 0 && sec != lastLoggedGnssSecond) {
        lastLoggedGnssSecond = sec;
        String j = "{\"type\":\"telemetry\",\"gnss_time\":\"" + String(gnss.gnss_utc) +
                   "\",\"lat\":" + String(gnss.lat, 9) + ",\"lon\":" + String(gnss.lon, 9) +
                   ",\"alt_m\":" + String(gnss.altitude_m, 3) + ",\"fix\":" + String(gnss.fix_quality) +
                   ",\"satellites\":" + String(gnss.satellites) + ",\"hdop\":" + String(gnss.hdop, 2) +
                   ",\"session_id\":\"" + String(sessionId) + "\"}";
        appendJsonLine(j);
      }
    }
  } else if (strstr(fields[0], "RMC")) {
    if (n > 9) {
      if (strlen(fields[9]) >= 6) {
        strncpy(lastDate, fields[9], 6);
        lastDate[6] = '\0';
      }
      if (strlen(fields[7])) gnss.speed_mps = atof(fields[7]) * 0.514444f;
      if (strlen(fields[8])) gnss.course_deg = atof(fields[8]);
      buildGnssUtc(fields[1]);
    }
  }
}

static bool sendRaw(const void *data, size_t len) {
  esp_err_t result = esp_now_send(gatewayMac, reinterpret_cast<const uint8_t *>(data), len);
  if (result == ESP_OK) setOverlay(rgb(90, 90, 90), 1, 30, 30);  // white radio flash
  return result == ESP_OK;
}

static MeasurementGnssState measurementGnss(const GnssState &source) {
  MeasurementGnssState snapshot{};
  snapshot.lat = source.lat;
  snapshot.lon = source.lon;
  snapshot.altitude_m = source.altitude_m;
  snapshot.hdop = source.hdop;
  snapshot.speed_mps = source.speed_mps;
  snapshot.course_deg = source.course_deg;
  snapshot.fix_quality = source.fix_quality;
  snapshot.satellites = source.satellites;
  snapshot.gnss_age_ms = source.gnss_age_ms;
  strlcpy(snapshot.gnss_utc, source.gnss_utc, sizeof(snapshot.gnss_utc));
  return snapshot;
}

static void sendTelemetry() {
  TelemetryPayload p{};
  initHeader(p.header, MsgType::TELEMETRY, sizeof(p), ++seqNo, ROVER_DEVICE_ID);
  p.gnss = gnss;
  p.gnss.gnss_age_ms = millis() - lastNmeaMs;
  p.uptime_ms = millis();
  strlcpy(p.session_id, sessionId, sizeof(p.session_id));
  sendRaw(&p, sizeof(p));
}

static void sendHeartbeat() {
  EventPayload p{};
  initHeader(p.header, MsgType::HEARTBEAT, sizeof(p), ++seqNo, ROVER_DEVICE_ID);
  makeId(p.event_id);
  strlcpy(p.session_id, sessionId, sizeof(p.session_id));
  strlcpy(p.event_type, "heartbeat", sizeof(p.event_type));
  snprintf(p.detail, sizeof(p.detail), "uptime=%lu", (unsigned long)millis());
  sendRaw(&p, sizeof(p));
}

static void captureMeasurement(uint8_t trigger) {
  MeasurementPayload p{};
  initHeader(p.header, MsgType::MEASUREMENT, sizeof(p), ++seqNo, ROVER_DEVICE_ID);
  GnssState snapshot = gnss;
  snapshot.gnss_age_ms = millis() - lastNmeaMs;
  p.gnss = measurementGnss(snapshot);
  if (armed && armedMeasurementId[0]) strlcpy(p.measurement_id, armedMeasurementId, sizeof(p.measurement_id));
  else makeId(p.measurement_id);
  strlcpy(lastMeasurementId, p.measurement_id, sizeof(lastMeasurementId));
  strlcpy(p.session_id, sessionId, sizeof(p.session_id));
  if (armed && armedName[0]) strlcpy(p.name, armedName, sizeof(p.name));
  else snprintf(p.name, sizeof(p.name), "Point-%04lu", (unsigned long)(seqNo % 10000));
  p.trigger = trigger;

  String j = "{\"type\":\"measurement\",\"measurement_id\":\"" + String(p.measurement_id) +
             "\",\"name\":\"" + String(p.name) + "\",\"gnss_time\":\"" + String(gnss.gnss_utc) +
             "\",\"lat\":" + String(gnss.lat, 9) + ",\"lon\":" + String(gnss.lon, 9) +
             ",\"fix\":" + String(gnss.fix_quality) + ",\"session_id\":\"" + String(sessionId) + "\"}";
  appendJsonLine(j);
  sendRaw(&p, sizeof(p));
  armed = false;
  armedName[0] = '\0';
  armedMeasurementId[0] = '\0';
}

static void sendSession(bool active) {
  if (active && !sessionActive) rotateBackupLog();
  SessionPayload p{};
  initHeader(p.header, MsgType::SESSION, sizeof(p), ++seqNo, ROVER_DEVICE_ID);
  if (active && !sessionId[0]) makeId(sessionId);
  strlcpy(p.session_id, sessionId, sizeof(p.session_id));
  p.active = active ? 1 : 0;
  appendJsonLine("{\"type\":\"session\",\"session_id\":\"" + String(sessionId) +
                 "\",\"active\":" + String(active ? "true" : "false") + "}");
  sendRaw(&p, sizeof(p));
  sessionActive = active;
  setOverlay(rgb(0, 0, 120), active ? 2 : 1, active ? 100 : 500, 100);
  if (!active) sessionId[0] = '\0';
}

static void cancelLastMeasurement() {
  if (!lastMeasurementId[0]) return;
  EventPayload p{};
  initHeader(p.header, MsgType::EVENT, sizeof(p), ++seqNo, ROVER_DEVICE_ID);
  makeId(p.event_id);
  strlcpy(p.session_id, sessionId, sizeof(p.session_id));
  strlcpy(p.event_type, "measurement_cancel", sizeof(p.event_type));
  strlcpy(p.detail, lastMeasurementId, sizeof(p.detail));
  appendJsonLine("{\"type\":\"measurement_cancel\",\"measurement_id\":\"" + String(lastMeasurementId) + "\"}");
  sendRaw(&p, sizeof(p));
  setOverlay(rgb(120, 45, 0), 2, 110, 90);
}

static void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (memcmp(mac, gatewayMac, 6) != 0) return;
  if (len < (int)sizeof(Header)) return;
  auto *h = reinterpret_cast<const Header *>(data);
  if (h->version != PROTOCOL_VERSION) return;
  lastGatewayRxMs = millis();
  gatewayChannelLocked = true;

  if (h->type == MsgType::CHANNEL_BEACON && len >= (int)sizeof(ChannelBeaconPayload)) return;

  if (h->type == MsgType::ACK && len >= (int)sizeof(AckPayload)) {
    auto *p = reinterpret_cast<const AckPayload *>(data);
    if (p->ok && lastMeasurementId[0] && strcmp(p->ack_id, lastMeasurementId) == 0)
      setOverlay(rgb(0, 120, 0), 2, 90, 80);
    else
      setOverlay(rgb(90, 90, 90), 1, 30, 30);
  } else if (h->type == MsgType::ARM_MEASUREMENT && len >= (int)sizeof(ArmPayload)) {
    auto *p = reinterpret_cast<const ArmPayload *>(data);
    armed = true;
    strlcpy(armedMeasurementId, p->measurement_id, sizeof(armedMeasurementId));
    strlcpy(armedName, p->name, sizeof(armedName));
    if (p->session_id[0]) strlcpy(sessionId, p->session_id, sizeof(sessionId));
  } else if (h->type == MsgType::SESSION_CONTROL && len >= (int)sizeof(SessionControlPayload)) {
    auto *p = reinterpret_cast<const SessionControlPayload *>(data);
    if (p->active) {
      if (!sessionActive) rotateBackupLog();
      strlcpy(sessionId, p->session_id, sizeof(sessionId));
      sessionActive = true;
      setOverlay(rgb(0, 0, 120), 2);
    } else {
      sessionActive = false;
      setOverlay(rgb(0, 0, 120), 1, 500, 100);
      sessionId[0] = '\0';
    }
  } else if (h->type == MsgType::RTCM_FRAGMENT && len >= (int)(sizeof(RtcmFragmentPayload) - RTCM_FRAGMENT_BYTES)) {
    auto *p = reinterpret_cast<const RtcmFragmentPayload *>(data);
    if (p->data_len <= RTCM_FRAGMENT_BYTES) gnssSerial.write(p->data, p->data_len);
  }
}

static void setupEspNow() {
  WiFi.mode(WIFI_STA);
  if (esp_wifi_set_mac(WIFI_IF_STA, deviceMac) != ESP_OK) Serial.println("Failed to set Wi-Fi MAC");
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(onReceive);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, gatewayMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

static void recoverEspNowChannel() {
  static uint32_t lastHop = 0;
  static uint8_t channel = 1;
  if (gatewayChannelLocked && millis() - lastGatewayRxMs <= GATEWAY_TIMEOUT_MS) return;
  gatewayChannelLocked = false;
  if (millis() - lastHop < 350) return;
  lastHop = millis();
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  channel = channel >= 11 ? 1 : channel + 1;
}

static void handleButton() {
  static bool prev = HIGH;
  static uint32_t pressStart = 0;
  static uint32_t releasedAt = 0;
  static uint8_t clickCount = 0;
  bool current = digitalRead(BUTTON_PIN);
  recoverEspNowChannel();
  uint32_t now = millis();

  if (prev == HIGH && current == LOW) pressStart = now;
  if (prev == LOW && current == HIGH) {
    uint32_t held = now - pressStart;
    if (held >= LONG_PRESS_MS) {
      clickCount = 0;
      cancelLastMeasurement();
    } else {
      clickCount++;
      releasedAt = now;
    }
  }

  if (current == LOW && pressStart && now - pressStart > 1200 && now - pressStart < LONG_PRESS_MS) {
    led.setPixelColor(0, rgb(120, 45, 0));  // cancel preview
    led.show();
  }

  if (clickCount && now - releasedAt > DOUBLE_CLICK_MS) {
    if (clickCount >= 2) sendSession(!sessionActive);
    else captureMeasurement(1);
    clickCount = 0;
  }
  prev = current;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PPS_PIN, INPUT);
  led.begin();
  led.setBrightness(32);
  LittleFS.begin(true);
  gnssSerial.begin(GNSS_BAUD, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
  setupEspNow();
}

void loop() {
  static String line;
  while (gnssSerial.available()) {
    char c = (char)gnssSerial.read();
    if (c == '\n') {
      line.trim();
      parseNmea(line);
      line = "";
    } else if (c != '\r' && line.length() < 175) {
      line += c;
    }
  }

  uint32_t now = millis();
  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    sendTelemetry();
  }
  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }
  handleButton();
  updateLed();
  delay(2);
}
