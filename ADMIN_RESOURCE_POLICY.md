# PRISMA-PI LITE — Admin Resource Policy

This is a project invariant.

## Core rule

**When the Admin UI is idle, it must generate zero background polling and zero recurring telemetry work on the appliance.**

The Admin interface must not introduce periodic HTTP requests, recurring sensor reads, recurring Wi-Fi scans/status reads, recurring SPIFFS scans, or background timers whose purpose is telemetry refresh.

## Allowed behavior

- Storage capacity and media-file inventory are calculated once per appliance boot/session snapshot and cached in RAM.
- Re-reading the Storage endpoint returns the cached snapshot and must not rescan SPIFFS or `/storage`.
- Wi-Fi status is calculated only when explicitly requested by the user.
- Controller state is read only when opening the relevant Admin section or pressing Refresh.
- System/battery information is read only when opening the relevant Admin section or pressing Refresh.
- QMI8658 sensor information is read only when opening the Sensors section or pressing Refresh.
- UI-only one-shot timers for layout/fullscreen rendering are allowed; they must not trigger recurring device telemetry.

## Important distinction

This rule applies to the Admin telemetry layer. Existing Controller synchronization behavior is independent and must not be changed by Admin work unless explicitly requested.

## Design priority

Resource conservation and appliance stability take priority over continuously updating Admin telemetry. Prefer stale-but-explicit snapshots plus manual refresh over background polling.
