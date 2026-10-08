# PRISMA-PI Lite — ESP32-S3 Implementation Order

Implementation advances only while all previous gates remain working.

## M0 — Architecture and build spine
1. Repository and dependency boundaries.
2. ESP-IDF project skeleton.
3. Verified hardware baseline.
4. Build configuration defaults.
5. Factory/full-image flashing path.
6. No OTA requirement for Lite V1.

## M1 — BSP hardware bring-up — VALIDATED ON HARDWARE
1. ST7789 240x240 display.
2. Backlight control.
3. CST816S touch.
4. Verified board GPIOs.
5. Touch/display smoke test.

## M2/M3 — Local interactive foundation — VALIDATED
1. Local renderer.
2. Touch routing.
3. NVS/settings foundation.
4. Version/diagnostic primitives.
5. Final appliance surface contract replaces the temporary M3 shell.

## M4.1 — Autonomous local Wi-Fi — CURRENT
1. Local AP starts without router/Internet.
2. SSID `PRISMA-PI-LITE`.
3. Local address `192.168.4.1`.
4. HTTP server and versioned network/health endpoints.
5. Network failure never blocks appliance operation.
6. Four final appliance pages become the local UI shell.

## M4.2 — Vintage web synthesizer `/`
1. Four switchable/swipeable pages matching appliance semantics.
2. Page 1 Intro.
3. Page 2 Now Playing / Intensity / Mode.
4. Page 3 Live Preview.
5. Page 4 Core Extended.
6. Vintage is default skin.
7. Modern remains a future presentation skin.

## M4.3 — Shared state + bidirectional synchronization
1. One authoritative synth state.
2. Semantic command queue.
3. WebSocket/event channel.
4. Appliance changes update web.
5. Web changes update appliance.
6. Random uses only enabled effects.

## M4.4 — `/admin`
1. Separate authenticated technical UI.
2. Prototype login `PrismaP / PrismaP`.
3. Network/uplink configuration.
4. System/status/diagnostics.
5. Streaming configuration surface.
6. Software Version / manual Update Status / Reset Settings.
7. No OTA subsystem in Lite V1.

## M5 — Media + streaming pipeline
1. Local image/video file source.
2. Video loop.
3. Processing/effects backend.
4. Live Preview transport.
5. Streaming output architecture.
6. Control plane remains separate from media plane.

## M6 — Audio-reactive processing
1. Integrated microphone/audio input.
2. Audio analysis.
3. Mapping into artistic controls without blocking UI/network.

## Permanent dependency rule

```text
product/application logic
          ↓
shared semantic state/commands
          ↓
UI / media / network adapters
          ↓
ESP-IDF / Waveshare BSP
```

No milestone may invert this direction for convenience.
