# Protocol and Data

## Protocol Version

Initial ESP-NOW protocol version: `1`.

Every binary message starts with a common header:

- protocol version
- message type
- payload size
- message ID
- source device ID
- monotonic sequence

## Rover to Gateway

| Type | Purpose |
| --- | --- |
| `TELEMETRY` | Latest GNSS and Rover state |
| `MEASUREMENT` | Physical or named point capture |
| `EVENT` | Button, GNSS, link, error, and debug events |
| `SESSION` | Session start/stop state |
| `HEARTBEAT` | Link health |
| `ACK` | Acknowledge Gateway commands |

## Gateway to Rover

| Type | Purpose |
| --- | --- |
| `ACK` | Confirm record persistence/receipt |
| `RTCM_FRAGMENT` | Binary RTCM fragment |
| `ARM_MEASUREMENT` | Set the next named point |
| `SESSION_CONTROL` | Start/stop the active session |
| `CONFIG` | Runtime-safe configuration |
| `TIME_SYNC` | Optional Gateway UTC reference |

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

## Telemetry

Gateway uploads telemetry on UTC boundaries ending in `00`, `10`, `20`, `30`, `40`, and `50` seconds.

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

A measurement captures a snapshot immediately rather than waiting for the next telemetry boundary.

Measurements may be:

- quick unnamed points with automatic names
- named points armed from the Web UI

Measurement names are editable later, but the measurement ID is immutable.

Canceling a measurement does not delete it. The server marks it canceled and preserves the audit trail.

## Server Ingestion

The Gateway sends records in batches. D1 applies unique primary keys so retries do not create duplicates.

A successful API response acknowledges each accepted ID. Gateway local data remains a backup until synchronization succeeds.
