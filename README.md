# PRISMA-PI Lite — Waveshare ESP32-S3 Touch LCD 1.54

Hardware/runtime port for **PRISMA-PI Lite Core 1.0** targeting Waveshare **ESP32-S3-Touch-LCD-1.54, SKU 33869**.

Product behavior remains defined by `francomjorge/prisma-pi-lite-core`. This repository contains only the device/runtime implementation.

## Current increment — Build 0.1

The first implementation is intentionally narrow:

- stable autonomous boot;
- battery power hold asserted immediately;
- 240x240 ST7789 display;
- CST816S touch;
- local HOME screen with image;
- accumulated touch-glitch interaction;
- single `factory` firmware layout;
- no OTA/update implementation;
- Wi-Fi/web introduced only after this local baseline is physically validated.

## Dependency direction

```text
Waveshare / ESP-IDF drivers
          ↓
     waveshare_bsp
          ↓
       prisma_ui
          ↓
       app_main
```

Board GPIOs and SDK handles must not leak into the application/UI contract.

## Build

Requires ESP-IDF 5.4+; CI is pinned to ESP-IDF 5.5.1.

```bash
idf.py set-target esp32s3
idf.py build
```

The CI workflow also creates `PRISMA-PI-LITE-ESP32S3-FACTORY.bin`, flashable at address `0x0`.

See `docs/BUILD-0.1-SCOPE.md` for acceptance criteria.
