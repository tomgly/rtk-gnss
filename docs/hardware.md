# Hardware

## Rev.1

Rev.1 uses:

- Witte Intelligent WTRTK-982 (Unicore UM982)
- Waveshare ESP32-S3-Zero
- Q39 active multi-band GNSS antenna
- 1x 18650 Li-ion cell
- TP4056 protected charging module
- MT3608 boost module configured for 5 V
- SS12D01 power switch

Base and Rover use one WTRTK-982 and one Q39 antenna each. Dual-antenna heading is not used in Rev.1.

## Confirmed MCU Connections

The current KiCad Rev.1 design maps:

| Function | ESP32-S3-Zero |
| --- | --- |
| PPS | GPIO1 |
| GNSS UART RX | GPIO44 |
| GNSS UART TX | GPIO43 |
| On-board WS2812 | GPIO21 |

The external field-test button defaults to GPIO2, an unused broken-out GPIO in the current Rev.1 design. It is an add-on connection and is not currently routed by the Rev.1 PCB.

## Planned Public Hardware Release

The repository `hardware/` directory is intentionally minimal during the field-test phase. The completed public Rev.1 release is intended to include:

- KiCad project, schematic, and PCB
- custom symbol and footprint libraries
- BOM
- manufacturing/Gerber outputs
- STEP and other useful 3D models
- assembly notes

Hardware sources should remain revisioned with the firmware and Web software so a release can be reproduced end-to-end.
