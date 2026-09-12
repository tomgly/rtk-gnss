# RTK-GNSS

[![Astro](https://img.shields.io/badge/Astro-7-BC52EE?logo=astro&logoColor=white)](https://astro.build/)
[![Cloudflare Workers](https://img.shields.io/badge/Cloudflare-Workers-F38020?logo=cloudflareworkers&logoColor=white)](https://workers.cloudflare.com/)
[![Cloudflare D1](https://img.shields.io/badge/Cloudflare-D1-F38020?logo=cloudflare&logoColor=white)](https://developers.cloudflare.com/d1/)
[![ESP32--S3](https://img.shields.io/badge/ESP32--S3-ESP--NOW-E7352C?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![TypeScript](https://img.shields.io/badge/TypeScript-Strict-3178C6?logo=typescript&logoColor=white)](https://www.typescriptlang.org/)
[![pnpm](https://img.shields.io/badge/pnpm-Package_Manager-F69220?logo=pnpm&logoColor=white)](https://pnpm.io/)

Open-source portable RTK-GNSS field platform built around the Witte Intelligent WTRTK-982 / Unicore UM982 and Waveshare ESP32-S3-Zero.

The project separates field sensing from Internet access: the Rover acquires GNSS data and communicates only with the Gateway over ESP-NOW unicast, while the Gateway handles Wi-Fi, Cloudflare synchronization, optional NTRIP/RTCM forwarding, local backup logging, and network diagnostics.

## Development Status

The current repository targets Rev.1 field testing and includes:

- Rover firmware for GNSS parsing, local JSONL backup, physical-button controls, LED status, ESP-NOW telemetry, measurements, sessions, and RTCM forwarding to the receiver.
- Gateway firmware for ESP-NOW, three saved Wi-Fi networks, one-minute Gateway and Rover-link status updates, local JSONL backup for durable records, Cloudflare API synchronization, retry handling, command polling, and optional NTRIP.
- Astro + TypeScript + Tailwind CSS v4 Web UI deployed as one Cloudflare Worker with D1.
- Public read-only live/history views and authenticated control operations.
- Session, measurement, event, device, command, and debug records with end-to-end unique IDs, plus one current live-status record per Gateway.

NTRIP is optional. The system continues to collect and synchronize standard GNSS data when no NTRIP service is configured or available.

## Architecture

```text
WTRTK-982
    |
    | UART / PPS
    v
  Rover
    |
    | ESP-NOW unicast
    v
 Gateway
    |  \
    |   \ optional NTRIP <-> RTCM -> Rover -> WTRTK-982
    |
    | HTTPS
    v
Cloudflare Worker / Astro
    |
    v
Cloudflare D1
    |
    v
Web UI
```

Rev.1 wiring used by the firmware defaults:

| Signal                      | ESP32-S3-Zero |
| --------------------------- | ------------- |
| GNSS TX -> MCU RX           | GPIO44        |
| MCU TX -> GNSS RX           | GPIO43        |
| PPS                         | GPIO1         |
| External measurement button | GPIO2         |
| On-board WS2812             | GPIO21        |

The GPIO2 button is an add-on for field testing and is not part of the current Rev.1 PCB routing.

## Repository Layout

```text
firmware/
  rover/
  gateway/
  shared/
web/
docs/
hardware/
```

`hardware/` is reserved for the complete public Rev.1 hardware release, including KiCad sources, custom symbols/footprints, manufacturing data, BOM, and 3D models.

## Controls

| Input               | Action                          |
| ------------------- | ------------------------------- |
| Single click        | Save a measurement point        |
| Double click        | Start or stop a test session    |
| Long press (~1.8 s) | Cancel the previous measurement |

A named measurement can be armed from the Web UI. The Rover LED turns blue until the physical button saves that point.

## Live Status Timing

The Gateway sends a lightweight online status and the current Rover link status at each UTC one-minute boundary while Wi-Fi is connected. The Web UI distinguishes a connected Rover with no current GNSS data from an offline Rover. The server replaces the prior live-status record for that Gateway rather than retaining a telemetry history.

The Gateway uses NTP-derived UTC. The Rover also preserves GNSS time so standalone Rover logs remain useful without Internet access.

## Web Stack

- Astro
- TypeScript strict mode
- Tailwind CSS v4
- Cloudflare Workers
- Cloudflare D1
- Prettier + prettier-plugin-astro
- tailwind-lint
- pnpm

The Web UI intentionally uses a compact field-console design: high information density, restrained styling, clear status colors, and minimal decorative UI.

## Commands

Run Web commands from `web/`.

| Command                    | Description                              |
| -------------------------- | ---------------------------------------- |
| `pnpm dev`                 | Start local Astro development            |
| `pnpm build`               | Build the Cloudflare Worker application  |
| `pnpm check`               | Run Astro and TypeScript checks          |
| `pnpm format`              | Format Web sources                       |
| `pnpm format:check`        | Check formatting                         |
| `pnpm lint:tailwind`       | Fix Tailwind utility issues              |
| `pnpm lint:tailwind:check` | Check Tailwind utility usage             |
| `pnpm lines:check`         | Check the 500-line source-file guideline |
| `pnpm deploy`              | Deploy with Wrangler                     |

Firmware can be built from each PlatformIO project:

```bash
cd firmware/rover
pio run
pio run --target upload

cd ../gateway
pio run
pio run --target upload
```

## API Endpoints

Public:

- `GET /api/public/live`
- `GET /api/public/sessions`
- `GET /api/public/measurements`
- `GET /api/public/led-reference`

Gateway/device:

- `POST /api/device/ingest`
- `GET /api/device/commands?device_id=...&after=...`

Authenticated Web control:

- `POST /api/auth/login`
- `POST /api/auth/logout`
- `POST /api/session`
- `DELETE /api/session/:id`
- `POST /api/measurement/arm`
- `PATCH /api/measurement/:id`

## Configuration and Secrets

Public firmware configuration examples are committed, but real secrets are not.

Copy:

```text
firmware/gateway/include/secrets.example.h
-> firmware/gateway/include/secrets.h
```

Real Wi-Fi credentials, NTRIP credentials, and device API credentials belong only in `secrets.h`, which is ignored by Git.

Cloudflare secrets should be set with Wrangler, for example:

```bash
pnpm exec wrangler secret put DEVICE_API_TOKEN
pnpm exec wrangler secret put LOGIN_PASSWORD
```

## D1 Migrations

From `web/`:

```bash
pnpm exec wrangler d1 migrations list rtk-gnss --remote
pnpm exec wrangler d1 migrations apply rtk-gnss --remote
```

Review migrations before applying them to production.

## Documentation

- [Architecture](./docs/architecture.md)
- [Protocol and Data](./docs/protocol.md)
- [Field Testing](./docs/field-testing.md)
- [Hardware](./docs/hardware.md)

See [AGENTS.md](./AGENTS.md) for development rules.

## License

MIT. The license file is intentionally not included in this initial bundle and can be added by the repository owner.
