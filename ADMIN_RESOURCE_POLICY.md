# PRISMA-PI LITE — Admin Surface Policy

This is a project invariant for the ESP32-S3 appliance Admin surface.

## Navigation

ADMIN uses the desktop sidebar layout with exactly three sections, in this order:

1. Live View Extended
2. Sensors
3. System

### Live View Extended
Layout placeholder only for now. No device reads or background work.

### Sensors
Layout placeholder only for now. Reserved for motion sensors, microphone and future sensor inputs. No sensor initialization or polling from Admin yet.

### System
Contains exactly two cards:

1. Wi-Fi
2. Settings

Wi-Fi remains on-demand. Settings is layout-only until its controls are implemented.

## Runtime rule

**Idle Admin must generate zero background polling and zero recurring telemetry work.**

- Wi-Fi status is calculated only after an explicit user action (`CHECK WI-FI`).
- Live View Extended must not start a preview pipeline until explicitly implemented.
- Sensors must not initialize or read motion/microphone hardware until explicitly implemented.
- No periodic HTTP requests, recurring Wi-Fi reads, recurring SPIFFS scans, sensor reads, microphone reads, or telemetry timers are allowed.
- Existing Controller synchronization is independent and must not be changed by Admin work.

Resource conservation and appliance stability take priority over continuously refreshed Admin telemetry.
