PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS devices (
  device_id TEXT PRIMARY KEY,
  gateway_id TEXT,
  last_seen TEXT NOT NULL,
  last_espnow_rssi INTEGER,
  last_wifi_rssi INTEGER,
  firmware_version TEXT,
  protocol_version INTEGER
);

CREATE TABLE IF NOT EXISTS sessions (
  session_id TEXT PRIMARY KEY,
  name TEXT NOT NULL,
  started_at TEXT,
  ended_at TEXT,
  active INTEGER NOT NULL DEFAULT 0,
  source TEXT NOT NULL DEFAULT 'device',
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS telemetry (
  record_id TEXT PRIMARY KEY,
  device_id TEXT NOT NULL,
  gateway_id TEXT NOT NULL,
  session_id TEXT,
  gateway_time TEXT NOT NULL,
  gnss_time TEXT,
  lat REAL,
  lon REAL,
  alt_m REAL,
  fix INTEGER,
  satellites INTEGER,
  hdop REAL,
  speed_mps REAL,
  course_deg REAL,
  gnss_age_ms INTEGER,
  espnow_rssi INTEGER,
  packets_received INTEGER,
  packets_lost INTEGER,
  wifi_ssid TEXT,
  wifi_rssi INTEGER,
  ntrip_enabled INTEGER,
  ntrip_connected INTEGER,
  rtcm_age_ms INTEGER,
  rtcm_bytes INTEGER,
  protocol_version INTEGER,
  raw_json TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_telemetry_time ON telemetry(gateway_time DESC);
CREATE INDEX IF NOT EXISTS idx_telemetry_session ON telemetry(session_id, gateway_time DESC);

CREATE TABLE IF NOT EXISTS measurements (
  measurement_id TEXT PRIMARY KEY,
  record_id TEXT NOT NULL UNIQUE,
  device_id TEXT NOT NULL,
  gateway_id TEXT NOT NULL,
  session_id TEXT,
  name TEXT NOT NULL,
  trigger INTEGER,
  gateway_time TEXT NOT NULL,
  gnss_time TEXT,
  lat REAL,
  lon REAL,
  alt_m REAL,
  fix INTEGER,
  satellites INTEGER,
  hdop REAL,
  espnow_rssi INTEGER,
  cancelled INTEGER NOT NULL DEFAULT 0,
  raw_json TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_measurements_time ON measurements(gateway_time DESC);

CREATE TABLE IF NOT EXISTS events (
  event_id TEXT PRIMARY KEY,
  record_id TEXT NOT NULL UNIQUE,
  device_id TEXT NOT NULL,
  gateway_id TEXT NOT NULL,
  session_id TEXT,
  event_type TEXT NOT NULL,
  detail TEXT,
  gateway_time TEXT NOT NULL,
  espnow_rssi INTEGER,
  raw_json TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS commands (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  command_id TEXT NOT NULL UNIQUE,
  gateway_id TEXT NOT NULL,
  type TEXT NOT NULL,
  payload_json TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
  acknowledged_at TEXT
);
CREATE INDEX IF NOT EXISTS idx_commands_gateway ON commands(gateway_id, id);

CREATE TABLE IF NOT EXISTS admin_sessions (
  token TEXT PRIMARY KEY,
  expires_at TEXT NOT NULL,
  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
