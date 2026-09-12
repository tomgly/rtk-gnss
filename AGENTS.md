# RTK-GNSS Agent Instructions

## Working Approach

- Communicate with the user in Japanese and keep responses concise.
- Before changing code, read `README.md`, relevant `docs/` files, and the affected implementation.
- Preserve the established Rover, Gateway, Web, database, and protocol architecture unless a change is necessary.
- Keep changes minimal and scoped. Do not refactor unrelated code or add unnecessary abstractions.
- Use pnpm only for the Web project. Do not use npm, Yarn, or Bun.
- Do not access, expose, commit, or modify secrets unless explicitly requested.
- Public repository files must never contain real Wi-Fi credentials, NTRIP credentials, API tokens, or other secrets. Use example configuration files for public documentation.

## Architecture

- Keep Rover independent from the Internet. Rover communicates with Gateway over ESP-NOW unicast.
- Gateway handles Wi-Fi, server communication, NTRIP, RTCM forwarding, synchronization, and backup logging.
- Rover and Gateway should continue operating when Internet or NTRIP is unavailable.
- Preserve unique IDs across Rover, Gateway, and Server so records can be traced end-to-end.
- Keep protocol changes versioned and documented.
- Prefer simple local JSON/JSONL backup storage on embedded devices unless requirements change.

## Web

- Use Astro, TypeScript strict mode, Tailwind CSS v4, Cloudflare Workers, and D1.
- Prefer Astro and browser-native features over unnecessary client frameworks.
- Use client-side JavaScript only where interactivity or live updates require it.
- Keep the interface simple, information-dense, responsive, and suitable for field use.
- Avoid generic AI-style dashboards, excessive rounded cards, gradients, decorative effects, and unnecessary whitespace.
- Keep LED status meanings accessible from the Web UI.

## Firmware

- Keep Rover and Gateway responsibilities clearly separated.
- Avoid blocking network operations that could interrupt ESP-NOW, logging, RTCM forwarding, or button handling.
- Logging and measurement collection must continue even when server synchronization fails.
- NTRIP must remain optional. The system must operate normally with NTRIP disabled.
- Do not silently discard measurements, events, failed uploads, or protocol errors that are useful for debugging.

## Quality and Documentation

- Keep source files at or below 500 lines when a meaningful split exists.
- Follow Prettier for Web formatting.
- Use Tailwind CSS utilities for standard Web styling and validate them with tailwind-lint.
- Run relevant checks before delivery: `pnpm format:check`, `pnpm lint:tailwind:check`, `pnpm lines:check`, `pnpm check`, and `pnpm build`.
- Update existing documentation when architecture, protocol, controls, or public behavior changes.
- Keep documentation concise. Prefer updating an existing document over creating a new one.
- Do not commit, push, deploy, or release unless explicitly requested.
- Use concise English Conventional Commit messages when committing.
- Report changed files, validation performed, and any checks that could not run.
