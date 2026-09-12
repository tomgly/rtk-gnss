# Protocol and Data

## Protocol Version

Current ESP-NOW protocol version: `1`.

Every binary message starts with a common header:

- protocol version
- message type
- payload size
- message ID
- source device ID
- monotonic sequence

## Rover to Gateway

| Type          | Purpose                                     |
| ------------- | ------------------------------------------- |
| `TELEMETRY`   | Latest GNSS and Rover state                 |
| `MEASUREMENT` | Physical or named point capture             |
| `EVENT`       | Button, GNSS, link, error, and debug events |
| `SESSION`     | Session start/stop state                    |
| `HEARTBEAT`   | Link health                                 |
| `ACK`         | Acknowledge Gateway commands                |

At Rover startup, the GNSS UART sends `GNGGA 1` and `GNRMC 1` to enable the required 1 Hz NMEA output from the WTRTK-982.

## Gateway to Rover

| Type              | Purpose                            |
| ----------------- | ---------------------------------- |
| `ACK`             | Confirm record persistence/receipt |
| `RTCM_FRAGMENT`   | Binary RTCM fragment               |
| `ARM_MEASUREMENT` | Set the next named point           |
| `SESSION_CONTROL` | Start/stop the active session      |
| `CONFIG`          | Runtime-safe configuration         |
| `TIME_SYNC`       | Optional Gateway UTC reference     |

RTCM remains binary and is never converted to JSON during ESP-NOW transport.

## Unique IDs

IDs are generated at the point where a logical object originates and never replaced downstream.

Recommended logical identifiers:

- `device_id`
- `session_id`
- `record_id`
- `measurement_id`
- `event_id`
- `command_id`

The firmware implementation uses compact UUID-like hexadecimal IDs to keep ESP-NOW payload size predictable.

Every ESP-NOW message is limited to 250 bytes. Telemetry includes the latest GGA sentence and fits this limit exactly. Measurement snapshots omit GGA because the Gateway does not need it to store a point.

The Gateway follows the connected 2.4 GHz Wi-Fi channel and broadcasts a `CHANNEL_BEACON` every 250 ms. The Rover scans channels only until it receives a beacon from its configured Gateway MAC, then remains on that channel. It resumes scanning after the Gateway link timeout.

## Telemetry

Gateway sends `gateway_status` at UTC one-minute boundaries while Wi-Fi is connected. When the Rover link is fresh, it also sends `rover_status`, which creates or refreshes the Rover device entry independently of GNSS fix quality. It sends current telemetry whenever fresh Rover telemetry is present, including no-fix or stale GNSS data. This lets the Web UI distinguish a connected Rover with no current data from an offline Rover. The server replaces the previous live-status record for that Gateway; it does not retain a telemetry history or queue live status for later upload.

Telemetry includes enough state to debug later:

- GNSS UTC
- Gateway UTC
- latitude / longitude / altitude
- fix quality
- satellite count
- HDOP
- speed / course
- GNSS age
- Rover uptime and sequence
- ESP-NOW RSSI and packet counters
- Gateway Wi-Fi SSID / RSSI
- Internet/server status
- NTRIP state and RTCM age/bytes when enabled
- active session ID
- firmware/protocol version

## Measurements

A measurement captures a snapshot immediately rather than waiting for the next live-status update.

Measurements may be:

- quick unnamed points with automatic names
- named points armed from the Web UI

Measurement names are editable later, but the measurement ID is immutable.

Canceling a measurement does not delete it. The server marks it canceled and preserves the audit trail.

## Server Ingestion

The Gateway sends records in batches. D1 applies unique primary keys so retries do not create duplicates.

A successful API response acknowledges each accepted ID. Gateway local data remains a backup until synchronization succeeds.
