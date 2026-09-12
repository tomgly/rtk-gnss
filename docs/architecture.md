# Architecture

## Goals

RTK-GNSS Rev.1 is designed as a field-testable Rover/Gateway system with clear responsibility boundaries and offline resilience.

- Rover remains independent of the Internet.
- Gateway is the single Internet-facing device.
- ESP-NOW unicast is the only normal Rover/Gateway transport.
- Cloud synchronization must never be required for measurement collection.
- NTRIP is optional and failure must degrade RTK only, not logging or standard GNSS operation.

## Rover

The Rover owns the GNSS receiver connection and physical field interaction.

Responsibilities:

- Parse WTRTK-982 NMEA output.
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
- Generate telemetry uploads at UTC 10-second boundaries.
- Synchronize records to the Cloudflare API and retry failures.
- Poll server commands and forward them to the Rover.
- Optionally connect to NTRIP, send Rover GGA, and forward RTCM to Rover.

Gateway network loss never stops Rover measurement collection.

## Cloudflare Application

One Cloudflare Worker hosts both the Astro UI and API.

D1 stores:

- devices
- sessions
- telemetry
- measurements
- events
- commands
- admin sessions

Public routes are read-only. Control operations require an authenticated browser session. Gateway ingestion and command polling use a device API token.

## Storage Strategy

Rover and Gateway use JSONL as short-term backup and diagnostics. D1 is the long-term source of truth.

Each originating record receives a unique ID at creation. That ID is preserved unchanged through Rover, Gateway, and server storage. Repeated uploads are safe because server IDs are unique and ingestion is idempotent.
