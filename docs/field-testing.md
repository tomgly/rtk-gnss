# Field Testing

## Button Controls

| Input | Result |
| --- | --- |
| Single click | Capture a measurement |
| Double click | Start/stop a session |
| Long press for about 1.8 s | Cancel the previous measurement |

Double-click window: approximately 350 ms.

A long press is previewed by the LED before the cancel action is committed.

## Rover LED

Normal color represents the most important current state. Short white flashes indicate radio activity. Temporary patterns acknowledge user actions.

| Pattern | Meaning |
| --- | --- |
| Red slow blink | No GNSS fix |
| Yellow solid | Single GNSS fix |
| Cyan solid | RTK Float |
| Green solid | RTK Fixed |
| Purple blink | Gateway link lost |
| Blue solid | Named measurement armed |
| White short flash | ESP-NOW communication |
| Green double flash | Measurement saved/ACKed |
| Red triple flash | Measurement failed |
| Blue double flash | Session started |
| Blue long flash | Session stopped |
| Orange double flash | Previous measurement canceled |

The Web UI includes the same LED reference at all times.

## Gateway LED

| Pattern | Meaning |
| --- | --- |
| Red | Rover unavailable |
| Orange | Rover available, Internet unavailable |
| Blue | Internet connected |
| Green | Cloud API healthy |
| White short flash | Rover/server traffic |
| Red fast blink | Persistent local/system error |

## Recommended Rev.1 Field Tests

1. Static GNSS drift at one physical point.
2. Repeatability by leaving and returning to the same point.
3. Known-distance comparison using a tape or other reference.
4. ESP-NOW range with unobstructed line of sight.
5. RSSI and packet loss versus distance.
6. Body, vegetation, and building obstruction tests.
7. Rover orientation sensitivity.
8. Gateway Wi-Fi roaming between configured networks.
9. Internet-loss logging and later synchronization.
10. Named measurement workflow from Web ARM to physical button save.
11. Session start/stop from both Web and physical double click.
12. NTRIP / RTK Float / RTK Fixed when a suitable caster is available.

NTRIP availability is not required for the rest of the test suite.
