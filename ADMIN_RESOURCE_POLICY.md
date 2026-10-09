# PRISMA-PI LITE — Minimal Admin Resource Policy

This is a project invariant for the ESP32-S3 appliance Admin surface.

## Allowed Admin surface

ADMIN contains only three cards:

1. Wi-Fi
2. Storage free
3. Media files

No Live View, Parameters, Power/Battery, Sensors, System, Diagnostics, Appearance, or other telemetry sections belong in this Admin version.

## Runtime rule

**Idle Admin must generate zero background polling and zero recurring telemetry work.**

- Storage capacity and media inventory are calculated once per appliance boot and cached in RAM.
- `/api/admin/storage` returns the cached snapshot and must not rescan SPIFFS.
- Wi-Fi status is calculated only after an explicit user action (`CHECK WI-FI`).
- No periodic HTTP requests, recurring Wi-Fi reads, recurring SPIFFS scans, or telemetry timers are allowed.
- Existing Controller synchronization is independent and must not be changed by Admin work.

Resource conservation and appliance stability take priority over continuously refreshed Admin telemetry.
