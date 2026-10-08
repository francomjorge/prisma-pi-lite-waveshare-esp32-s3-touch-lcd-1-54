# PRISMA-PI Lite — Waveshare ESP32-S3 Touch LCD 1.54

Hardware port for **PRISMA-PI Lite Core 1.0** targeting the Waveshare ESP32-S3-Touch-LCD-1.54 appliance.

This repository contains the device/runtime implementation only. Product behavior and domain rules remain defined by `prisma-pi-lite-core`.

## Status

**M0 — Architecture and build spine**

The repository now defines the firmware architecture, hardware baseline, ESP-IDF build spine, OTA-capable flash layout and implementation order. Hardware drivers begin in M1.

## Architectural rule

```text
product/application logic
          ↓
Core-facing contracts
          ↓
adapters/services
          ↓
ESP-IDF / Waveshare BSP
```

Application/domain code must never contain board pins, Waveshare-specific types, LCD/touch controller calls or direct ESP-IDF driver handles.

## Firmware component model

```text
main               composition root only
prisma_core        Core-facing contracts/types
prisma_app         authoritative state + command orchestration
prisma_ui          local appliance projection/input translation
prisma_media       image/frame/source pipeline
prisma_platform    NVS, clock, system health, version, restart
prisma_network     Wi-Fi + remote transport adapters
prisma_ota         UpdatePort / A-B update engine
prisma_bsp         Waveshare-specific hardware implementation
```

## Current hardware baseline

- ESP32-S3R8
- 8 MB PSRAM
- 16 MB flash
- 240 × 240 ST7789 SPI LCD
- CST816S capacitive touch over I2C
- initial ESP-IDF baseline: 5.5.1

Exact pins and electrical/runtime assumptions are maintained in `docs/HARDWARE-BASELINE-1.0.md` and, once implemented, `prisma_bsp`.

## Build 0.1 target

1. Stable autonomous local boot.
2. Display/backlight.
3. Touch/input.
4. One authoritative application state and command path.
5. Local game/test surface.
6. Static image through the normal media path.
7. Persistent settings foundation.
8. Network/web projection added without changing the application model.
9. Wi-Fi OTA layered over an update engine that already uses A/B partitions.

## OTA baseline

The flash layout reserves two equal 5 MB application slots (`ota_0`, `ota_1`), OTA metadata, persistent NVS, a future asset/media data region and crash diagnostics. Rollback support is enabled at the project baseline even though OTA transport is implemented later.

## Documents

- `docs/IMPLEMENTATION-STRATEGY-1.0.md` — normative implementation strategy and milestones.
- `docs/FIRMWARE-ARCHITECTURE-1.0.md` — component ownership, dependency rules and runtime architecture.
- `docs/HARDWARE-BASELINE-1.0.md` — verified Waveshare hardware facts and pin baseline.
- `docs/BUILD-0.1-SCOPE.md` — first vertical slice acceptance criteria.
- `docs/IMPLEMENTATION-ORDER.md` — ordered implementation gates.

## Immediate next milestone

**M1 — BSP hardware bring-up**: board facade, ST7789 display/backlight and CST816S touch strictly inside `prisma_bsp`.
