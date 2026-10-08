# PRISMA-PI Lite — ESP32-S3 Implementation Order

Implementation advances only while all previous gates remain working.

## M0 — Architecture and build spine
1. Repository and dependency boundaries.
2. ESP-IDF project skeleton.
3. Verified hardware baseline.
4. 16 MB A/B OTA partition table.
5. Build configuration defaults.
6. Build 0.1 acceptance scope.

## M1 — BSP hardware bring-up
1. Board initialization facade.
2. ST7789 SPI display initialization.
3. Backlight control.
4. CST816S I2C touch initialization.
5. Normalized touch events.
6. Display/touch health.
7. Hardware smoke and stability tests.

## M2 — Local vertical slice
1. Core contract subset required by Build 0.1.
2. Authoritative application controller and command queue.
3. Read-only view-model snapshot/version.
4. Local UI renderer.
5. Game/test surface through semantic commands.
6. Static image source via media abstraction.
7. Settings persistence foundation.

## M3 — Network/web projection
1. NetworkPort adapter.
2. Wi-Fi lifecycle independent of local runtime.
3. HTTP asset server.
4. state/view-model snapshot API.
5. WebSocket/event synchronization if required.
6. remote commands through the same command queue.
7. browser image/state projection.

## M4 — OTA over network
1. UpdatePort implementation.
2. inactive-slot selection/write.
3. image validation.
4. candidate boot selection.
5. reboot and application health gate.
6. mark-valid on success.
7. rollback on failed/unconfirmed candidate.
8. Wi-Fi firmware transport layered above the update engine.

## M5 — Hardening
1. watchdog/recovery policy.
2. memory/PSRAM telemetry.
3. long-running soak tests.
4. network fault/restart tests.
5. OTA interruption and rollback tests.
6. power-cycle/brownout recovery tests.
7. release/version diagnostics.

## Permanent dependency rule

```text
product/application logic
          ↓
Core-facing contracts
          ↓
adapters/services
          ↓
ESP-IDF / Waveshare BSP
```

No milestone may invert this direction for convenience.
