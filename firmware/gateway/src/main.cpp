#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_now.h>
#include <mbedtls/base64.h>
#include <time.h>
#include <vector>

#if __has_include("local_config.h")
#include "local_config.h"
#else
#include "config.example.h"
#endif
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
#include "protocol.h"

using namespace rtk;

static uint8_t roverMac[] = ROVER_MAC;
static Adafruit_NeoPixel led(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;
static TelemetryPayload latestRover{};
static bool haveRoverTelemetry = false;
static int latestEspNowRssi = -127;
static uint32_t roverPackets = 0;
static uint32_t roverLastSeq = 0;
static uint32_t roverLostPackets = 0;
static uint32_t lastRoverMs = 0;
static uint32_t lastServerOkMs = 0;
static uint32_t lastWifiAttemptMs = 0;
static uint32_t lastBoundaryEpoch = 0;
static uint32_t lastCommandId = 0;
static uint32_t rtcmBytes = 0;
static uint32_t lastRtcmMs = 0;
static bool ntripConnected = false;
static WiFiClient ntripClient;
static SemaphoreHandle_t fileMutex;
static Preferences prefs;

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) { return led.Color(r, g, b); }
static uint32_t overlayColor = 0;
static uint32_t overlayUntil = 0;

static void flashWhite(uint16_t ms = 35) {
  overlayColor = rgb(100, 100, 100);
  overlayUntil = millis() + ms;
}

static void updateLed() {
  uint32_t c;
  bool roverOnline = millis() - lastRoverMs <= ROVER_TIMEOUT_MS;
  if (!roverOnline) c = rgb(100, 0, 0);
  else if (WiFi.status() != WL_CONNECTED) c = rgb(110, 50, 0);
  else if (millis() - lastServerOkMs < 15000) c = rgb(0, 100, 0);
  else c = rgb(0, 0, 100);
  if (millis() < overlayUntil) c = overlayColor;
  led.setPixelColor(0, c);
  led.show();
}

static String jsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c == '\n') out += "\\n";
    else if (c != '\r') out += c;
  }
  return out;
}

static void appendLine(const char *path, const String &line) {
  if (xSemaphoreTake(fileMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
  File f = LittleFS.open(path, FILE_APPEND);
  if (f) { f.println(line); f.close(); }
  xSemaphoreGive(fileMutex);
}


static void rotateGatewayBackup() {
  if (xSemaphoreTake(fileMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
  if (LittleFS.exists("/gateway.previous.jsonl")) LittleFS.remove("/gateway.previous.jsonl");
  if (LittleFS.exists("/gateway.jsonl")) LittleFS.rename("/gateway.jsonl", "/gateway.previous.jsonl");
  xSemaphoreGive(fileMutex);
}

static void queueRecord(const String &json) {
  appendLine("/gateway.jsonl", json);
  appendLine("/queue.jsonl", json);
}

static String isoUtc(time_t t) {
  struct tm tm{};
  gmtime_r(&t, &tm);
  char out[25];
  strftime(out, sizeof(out), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return String(out);
}

static String baseEnvelope(const char *type, const char *recordId, const char *deviceId) {
  time_t now = time(nullptr);
  String j = "{\"type\":\"" + String(type) + "\",\"record_id\":\"" + String(recordId) +
             "\",\"device_id\":\"" + String(deviceId) + "\",\"gateway_id\":\"" +
             String(GATEWAY_DEVICE_ID) + "\",\"gateway_time\":\"" + isoUtc(now) + "\"";
  return j;
}

static void sendAck(const char *id, bool ok = true) {
  AckPayload p{};
  initHeader(p.header, MsgType::ACK, sizeof(p), 0, GATEWAY_DEVICE_ID);
  strlcpy(p.ack_id, id, sizeof(p.ack_id));
  p.ok = ok ? 1 : 0;
  esp_now_send(roverMac, reinterpret_cast<uint8_t *>(&p), sizeof(p));
}

static void storeMeasurement(const MeasurementPayload &p) {
  String j = baseEnvelope("measurement", p.measurement_id, p.header.source_device_id);
  j += ",\"message_id\":\"" + String(p.header.message_id) + "\",\"session_id\":\"" + String(p.session_id) +
       "\",\"measurement_id\":\"" + String(p.measurement_id) + "\",\"name\":\"" + jsonEscape(p.name) +
       "\",\"trigger\":" + String(p.trigger) + ",\"gnss_time\":\"" + String(p.gnss.gnss_utc) +
       "\",\"lat\":" + String(p.gnss.lat, 9) + ",\"lon\":" + String(p.gnss.lon, 9) +
       ",\"alt_m\":" + String(p.gnss.altitude_m, 3) + ",\"fix\":" + String(p.gnss.fix_quality) +
       ",\"satellites\":" + String(p.gnss.satellites) + ",\"hdop\":" + String(p.gnss.hdop, 2) +
       ",\"espnow_rssi\":" + String(latestEspNowRssi) + "}";
  queueRecord(j);
  sendAck(p.measurement_id);
}

static void storeEvent(const EventPayload &p) {
  String j = baseEnvelope("event", p.event_id, p.header.source_device_id);
  j += ",\"message_id\":\"" + String(p.header.message_id) + "\",\"session_id\":\"" + String(p.session_id) +
       "\",\"event_id\":\"" + String(p.event_id) + "\",\"event_type\":\"" + String(p.event_type) +
       "\",\"detail\":\"" + jsonEscape(p.detail) + "\",\"espnow_rssi\":" + String(latestEspNowRssi) + "}";
  queueRecord(j);
  if (strcmp(p.event_type, "heartbeat") != 0) sendAck(p.event_id);
}

static void storeSession(const SessionPayload &p) {
  if (p.active) rotateGatewayBackup();
  char rid[ID_LEN]; makeId(rid);
  String j = baseEnvelope("session", rid, p.header.source_device_id);
  j += ",\"message_id\":\"" + String(p.header.message_id) + "\",\"session_id\":\"" + String(p.session_id) +
       "\",\"active\":" + String(p.active ? "true" : "false") + "}";
  queueRecord(j);
  sendAck(p.header.message_id);
}

static void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < (int)sizeof(Header)) return;
  const Header *h = reinterpret_cast<const Header *>(data);
  if (h->version != PROTOCOL_VERSION) return;
  lastRoverMs = millis();
  latestEspNowRssi = info && info->rx_ctrl ? info->rx_ctrl->rssi : -127;
  roverPackets++;
  if (roverLastSeq && h->seq > roverLastSeq + 1) roverLostPackets += h->seq - roverLastSeq - 1;
  if (h->seq > roverLastSeq) roverLastSeq = h->seq;
  flashWhite();

  if (h->type == MsgType::TELEMETRY && len >= (int)sizeof(TelemetryPayload)) {
    portENTER_CRITICAL(&stateMux);
    memcpy(&latestRover, data, sizeof(TelemetryPayload));
    haveRoverTelemetry = true;
    portEXIT_CRITICAL(&stateMux);
    sendAck(h->message_id);
  } else if (h->type == MsgType::MEASUREMENT && len >= (int)sizeof(MeasurementPayload)) {
    storeMeasurement(*reinterpret_cast<const MeasurementPayload *>(data));
  } else if (h->type == MsgType::EVENT && len >= (int)sizeof(EventPayload)) {
    storeEvent(*reinterpret_cast<const EventPayload *>(data));
  } else if (h->type == MsgType::SESSION && len >= (int)sizeof(SessionPayload)) {
    storeSession(*reinterpret_cast<const SessionPayload *>(data));
  } else if (h->type == MsgType::HEARTBEAT) {
    // Heartbeats are represented by link counters; avoid writing 1 Hz debug rows.
  }
}

static void setupEspNow() {
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(onReceive);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, roverMac, 6);
  peer.channel = 0;  // Follow current STA channel.
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

static bool connectBestWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  int found = WiFi.scanNetworks(false, true);
  for (const auto &cred : WIFI_NETWORKS) {
    if (!cred.ssid || !cred.ssid[0] || String(cred.ssid).startsWith("WIFI_SSID_")) continue;
    bool visible = false;
    for (int i = 0; i < found; ++i) if (WiFi.SSID(i) == cred.ssid) { visible = true; break; }
    if (!visible) continue;
    WiFi.begin(cred.ssid, cred.password);
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT_MS) delay(100);
    if (WiFi.status() == WL_CONNECTED) {
      configTime(0, 0, "time.cloudflare.com", "time.google.com", "pool.ntp.org");
      WiFi.scanDelete();
      return true;
    }
    WiFi.disconnect(false, false);
  }
  WiFi.scanDelete();
  return false;
}

static bool postJson(const String &json) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.setTimeout(1500);
  http.begin(String(API_BASE_URL) + "/api/device/ingest");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(DEVICE_API_TOKEN));
  int code = http.POST(json);
  http.end();
  if (code >= 200 && code < 300) {
    lastServerOkMs = millis();
    flashWhite();
    return true;
  }
  return false;
}

static void syncQueue() {
  if (WiFi.status() != WL_CONNECTED) return;
  std::vector<String> lines;
  if (xSemaphoreTake(fileMutex, pdMS_TO_TICKS(300)) != pdTRUE) return;
  File f = LittleFS.open("/queue.jsonl", FILE_READ);
  if (f) {
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length()) lines.push_back(line);
    }
    f.close();
  }
  xSemaphoreGive(fileMutex);
  if (lines.empty()) return;

  size_t firstUnsent = lines.size();
  for (size_t i = 0; i < lines.size(); ++i) {
    if (!postJson(lines[i])) { firstUnsent = i; break; }
  }

  if (xSemaphoreTake(fileMutex, pdMS_TO_TICKS(300)) != pdTRUE) return;
  File tmp = LittleFS.open("/queue.tmp", FILE_WRITE);
  if (tmp) {
    for (size_t i = firstUnsent; i < lines.size(); ++i) tmp.println(lines[i]);
    tmp.close();
    LittleFS.remove("/queue.jsonl");
    LittleFS.rename("/queue.tmp", "/queue.jsonl");
  }
  xSemaphoreGive(fileMutex);
}
static void queueBoundaryTelemetry() {
  time_t now = time(nullptr);
  if (now < 1700000000) return;  // NTP not valid yet.
  if ((now % TELEMETRY_BOUNDARY_SECONDS) != 0 || (uint32_t)now == lastBoundaryEpoch) return;
  lastBoundaryEpoch = (uint32_t)now;

  TelemetryPayload p{};
  bool has;
  portENTER_CRITICAL(&stateMux);
  has = haveRoverTelemetry;
  if (has) memcpy(&p, &latestRover, sizeof(p));
  portEXIT_CRITICAL(&stateMux);
  if (!has) return;

  char rid[ID_LEN]; makeId(rid);
  String j = baseEnvelope("telemetry", rid, p.header.source_device_id);
  j += ",\"session_id\":\"" + String(p.session_id) + "\",\"gnss_time\":\"" + String(p.gnss.gnss_utc) +
       "\",\"lat\":" + String(p.gnss.lat, 9) + ",\"lon\":" + String(p.gnss.lon, 9) +
       ",\"alt_m\":" + String(p.gnss.altitude_m, 3) + ",\"fix\":" + String(p.gnss.fix_quality) +
       ",\"satellites\":" + String(p.gnss.satellites) + ",\"hdop\":" + String(p.gnss.hdop, 2) +
       ",\"speed_mps\":" + String(p.gnss.speed_mps, 3) + ",\"course_deg\":" + String(p.gnss.course_deg, 2) +
       ",\"gnss_age_ms\":" + String(p.gnss.gnss_age_ms) + ",\"espnow_rssi\":" + String(latestEspNowRssi) +
       ",\"packets_received\":" + String(roverPackets) + ",\"packets_lost\":" + String(roverLostPackets) +
       ",\"wifi_ssid\":\"" + jsonEscape(WiFi.SSID()) + "\",\"wifi_rssi\":" + String(WiFi.RSSI()) +
       ",\"ntrip_enabled\":" + String(NTRIP_ENABLED ? "true" : "false") +
       ",\"ntrip_connected\":" + String(ntripConnected ? "true" : "false") +
       ",\"rtcm_age_ms\":" + String(lastRtcmMs ? millis() - lastRtcmMs : 0) +
       ",\"rtcm_bytes\":" + String(rtcmBytes) + ",\"protocol_version\":" + String(PROTOCOL_VERSION) + "}";
  queueRecord(j);
}

static String basicAuth() {
  String raw = String(NTRIP_USERNAME) + ":" + String(NTRIP_PASSWORD);
  size_t outLen = 0;
  unsigned char out[180];
  mbedtls_base64_encode(out, sizeof(out), &outLen,
                        reinterpret_cast<const unsigned char *>(raw.c_str()), raw.length());
  if (outLen < sizeof(out)) out[outLen] = 0;
  return String(reinterpret_cast<char *>(out));
}

static void connectNtrip() {
  if (!NTRIP_ENABLED || WiFi.status() != WL_CONNECTED || ntripClient.connected()) return;
  if (!strlen(NTRIP_HOST) || !strlen(NTRIP_MOUNTPOINT)) return;
  if (!ntripClient.connect(NTRIP_HOST, NTRIP_PORT, 1500)) { ntripConnected = false; return; }
  ntripClient.print("GET /" + String(NTRIP_MOUNTPOINT) + " HTTP/1.1\r\n");
  ntripClient.print("Host: " + String(NTRIP_HOST) + "\r\n");
  ntripClient.print("Ntrip-Version: Ntrip/2.0\r\n");
  ntripClient.print("User-Agent: RTK-GNSS/0.1\r\n");
  if (strlen(NTRIP_USERNAME)) ntripClient.print("Authorization: Basic " + basicAuth() + "\r\n");
  ntripClient.print("Connection: close\r\n\r\n");
  uint32_t started = millis();
  String header;
  while (millis() - started < 1200 && ntripClient.connected()) {
    while (ntripClient.available()) {
      char c = ntripClient.read(); header += c;
      if (header.endsWith("\r\n\r\n")) {
        ntripConnected = header.indexOf("200") >= 0 || header.indexOf("ICY 200") >= 0;
        return;
      }
    }
    delay(5);
  }
  ntripClient.stop(); ntripConnected = false;
}

static void serviceNtrip() {
  static uint32_t lastGgaMs = 0;
  if (!NTRIP_ENABLED) return;
  if (!ntripClient.connected()) { ntripConnected = false; connectNtrip(); return; }
  ntripConnected = true;

  if (millis() - lastGgaMs > 5000) {
    lastGgaMs = millis();
    TelemetryPayload p{};
    portENTER_CRITICAL(&stateMux); memcpy(&p, &latestRover, sizeof(p)); portEXIT_CRITICAL(&stateMux);
    if (p.gnss.gga[0]) ntripClient.print(String(p.gnss.gga) + "\r\n");
  }

  while (ntripClient.available()) {
    RtcmFragmentPayload frag{};
    initHeader(frag.header, MsgType::RTCM_FRAGMENT, sizeof(frag), 0, GATEWAY_DEVICE_ID);
    frag.rtcm_message_id = esp_random();
    frag.fragment_index = 0;
    frag.fragment_count = 1;
    frag.data_len = ntripClient.read(frag.data, RTCM_FRAGMENT_BYTES);
    if (frag.data_len) {
      rtcmBytes += frag.data_len;
      lastRtcmMs = millis();
      esp_now_send(roverMac, reinterpret_cast<uint8_t *>(&frag), sizeof(frag));
    }
    if (frag.data_len < RTCM_FRAGMENT_BYTES) break;
  }
}

static void sendArmCommand(const JsonObjectConst &cmd) {
  ArmPayload p{};
  initHeader(p.header, MsgType::ARM_MEASUREMENT, sizeof(p), 0, GATEWAY_DEVICE_ID);
  strlcpy(p.command_id, cmd["command_id"] | "", sizeof(p.command_id));
  strlcpy(p.measurement_id, cmd["measurement_id"] | "", sizeof(p.measurement_id));
  strlcpy(p.session_id, cmd["session_id"] | "", sizeof(p.session_id));
  strlcpy(p.name, cmd["name"] | "", sizeof(p.name));
  esp_now_send(roverMac, reinterpret_cast<uint8_t *>(&p), sizeof(p));
}

static void sendSessionCommand(const JsonObjectConst &cmd) {
  SessionControlPayload p{};
  initHeader(p.header, MsgType::SESSION_CONTROL, sizeof(p), 0, GATEWAY_DEVICE_ID);
  strlcpy(p.command_id, cmd["command_id"] | "", sizeof(p.command_id));
  strlcpy(p.session_id, cmd["session_id"] | "", sizeof(p.session_id));
  p.active = cmd["active"] | false;
  esp_now_send(roverMac, reinterpret_cast<uint8_t *>(&p), sizeof(p));
}

static void pollCommands() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(1200);
  String url = String(API_BASE_URL) + "/api/device/commands?device_id=" + GATEWAY_DEVICE_ID + "&after=" + lastCommandId;
  http.begin(url);
  http.addHeader("Authorization", "Bearer " + String(DEVICE_API_TOKEN));
  int code = http.GET();
  if (code != 200) { http.end(); return; }
  JsonDocument doc;
  if (deserializeJson(doc, http.getString()) != DeserializationError::Ok) { http.end(); return; }
  http.end();
  for (JsonObjectConst cmd : doc["commands"].as<JsonArrayConst>()) {
    uint32_t id = cmd["id"] | 0;
    const char *type = cmd["type"] | "";
    if (!strcmp(type, "arm_measurement")) sendArmCommand(cmd);
    else if (!strcmp(type, "session_control")) sendSessionCommand(cmd);
    if (id > lastCommandId) { lastCommandId = id; prefs.putUInt("last_cmd", lastCommandId); }
  }
}

static void networkTask(void *) {
  uint32_t lastSync = 0, lastPoll = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttemptMs > WIFI_RESCAN_INTERVAL_MS) {
      lastWifiAttemptMs = millis();
      connectBestWifi();
    }
    queueBoundaryTelemetry();
    if (millis() - lastSync > UPLOAD_RETRY_INTERVAL_MS) { lastSync = millis(); syncQueue(); }
    if (millis() - lastPoll > COMMAND_POLL_INTERVAL_MS) { lastPoll = millis(); pollCommands(); }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  led.begin(); led.setBrightness(32);
  LittleFS.begin(true);
  fileMutex = xSemaphoreCreateMutex();
  prefs.begin("rtk", false);
  lastCommandId = prefs.getUInt("last_cmd", 0);
  WiFi.mode(WIFI_STA);
  connectBestWifi();
  setupEspNow();
  xTaskCreatePinnedToCore(networkTask, "network", 12288, nullptr, 1, nullptr, 0);
}

void loop() {
  serviceNtrip();
  updateLed();
  delay(2);
}
