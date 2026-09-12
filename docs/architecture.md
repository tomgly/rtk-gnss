# Architecture

## Goals

RTK-GNSS Rev.1 is designed as a field-testable Rover/Gateway system with clear responsibility boundaries and offline resilience.

- Rover remains independent of the Internet.
- Gateway is the single Internet-facing device.
- ESP-NOW unicast is the normal Rover/Gateway transport; Gateway channel beacons let the Rover discover the current Wi-Fi channel.
- Cloud synchronization must never be required for measurement collection.
- NTRIP is optional and failure must degrade RTK only, not logging or standard GNSS operation.

## Rover

The Rover owns the GNSS receiver connection and physical field interaction.

Responsibilities:

- Enable 1 Hz WTRTK-982 `GNGGA` and `GNRMC` output at startup, then parse the NMEA output.
- Preserve GNSS UTC in local records.
- Track PPS input.
- Save local JSONL backup records.
- Send telemetry, measurements, events, and session state over ESP-NOW.
- Receive named-measurement arms, session commands, ACKs, and RTCM fragments from Gateway.
- Forward reassembled RTCM binary to the WTRTK-982 UART.
- Drive the on-board WS2812 status LED.
- Read the external field button.

The Rover does not connect to Wi-Fi, Cloudflare, or an NTRIP caster.

## Gateway

Responsibilities:

- Maintain ESP-NOW unicast with the Rover.
- Record received Rover data plus Gateway-side radio/network metadata.
- Keep an append-only JSONL backup/queue in LittleFS.
- Connect to up to three configured Wi-Fi networks.
- Synchronize UTC with NTP.
- Send a Gateway online status and current Rover link status at UTC one-minute boundaries while Wi-Fi is connected. The live status includes stale or no-fix GNSS data so the Web UI can distinguish a connected Rover with no data from an offline Rover.
- Synchronize records to the Cloudflare API and retry failures.
- Poll server commands and forward them to the Rover.
- Optionally connect to NTRIP, send Rover GGA, and forward RTCM to Rover.

Gateway network loss never stops Rover measurement collection.

## Cloudflare Application

One Cloudflare Worker hosts both the Astro UI and API.

D1 stores:

- devices
- sessions
- current live status
- measurements
- events
- commands

All browser pages and browser data APIs require a signed login cookie. Gateway ingestion and command polling use a device API token.

## Storage Strategy

Rover and Gateway use JSONL as short-term backup and diagnostics. D1 is the long-term source of truth for measurements, sessions, and events. Gateway online status is sent when Wi-Fi is available. GNSS live status is not queued or retained locally; it is sent while the Rover link is fresh, including stale or no-fix data.

Each originating record receives a unique ID at creation. That ID is preserved unchanged through Rover, Gateway, and server storage. Repeated uploads are safe because server IDs are unique and ingestion is idempotent.
