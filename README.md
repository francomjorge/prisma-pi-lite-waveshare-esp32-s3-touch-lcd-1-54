# PRISMA-PI Lite — Waveshare ESP32-S3 Touch LCD 1.54

Hardware/runtime implementation of **PRISMA-PI Lite Core 1.0** for the Waveshare **ESP32-S3-Touch-LCD-1.54, SKU 33869**.

Product behavior, state, commands, views, ports and capabilities remain defined by `francomjorge/prisma-pi-lite-core`. This repository maps those contracts onto the ESP32-S3 appliance; it does not redefine them.

## Status

**M0 — architecture and build-spine definition**

The architecture is frozen before hardware feature implementation so each incremental build remains on the path to the complete product.

## Verified target baseline

- ESP32-S3R8, dual-core LX7 up to 240 MHz
- 16 MB external Flash
- 8 MB PSRAM
- 1.54 inch 240 x 240 LCD
- ST7789 display controller over SPI
- CST816-family capacitive touch over I2C
- 2.4 GHz Wi-Fi and BLE 5 hardware
- ESP-IDF 5.5.1 used as initial verified vendor baseline

## Architecture

```text
                    PRISMA-PI Lite Core 1.0 contracts
                                  |
                                  v
                           +-------------+
                           | prisma_core |
                           +------+------+ 
                                  |
                                  v
                           +-------------+
                           | prisma_app  |  <- single authoritative state
                           +--+----+--+--+
                              |    |  |
                 +------------+    |  +-------------+
                 v                 v                v
             prisma_ui        prisma_media    service adapters
                 |                 |          /      |       \
                 v                 v         v       v        v
             prisma_bsp        prisma_bsp platform network   ota
```

Core rules:

- board wiring exists only in `prisma_bsp`;
- local appliance operation never depends on network availability;
- touch and future web control enter the same semantic command path;
- local LCD and future web UI consume the same logical application/view-model;
- the LCD framebuffer is an output, not the remote synchronization source;
- OTA is transport-neutral and the flash layout is A/B-capable before Wi-Fi OTA is enabled;
- unsupported hardware/product features are represented honestly through `DeviceCapabilities`.

## Planned repository structure

```text
.
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── README.md
├── docs/
│   ├── IMPLEMENTATION-STRATEGY-1.0.md
│   ├── FIRMWARE-ARCHITECTURE-1.0.md
│   ├── HARDWARE-BASELINE-1.0.md
│   └── BUILD-0.1-SCOPE.md
├── main/
│   ├── CMakeLists.txt
│   └── app_main.c
└── components/
    ├── prisma_core/
    ├── prisma_app/
    ├── prisma_ui/
    ├── prisma_media/
    ├── prisma_platform/
    ├── prisma_network/
    ├── prisma_ota/
    └── prisma_bsp/
```

Directories/components are created as implementation begins; Git does not retain empty directories.

## Milestones

1. **M0 — Architecture/build spine**
2. **M1 — Stable boot + LCD + backlight + touch**
3. **M2 — Shared state/view-model + local game + image path**
4. **M3 — Wi-Fi + web projection/control**
5. **M4 — Wi-Fi OTA A/B + validation/rollback**
6. **M5 — hardening, recovery, soak testing and diagnostics**

## Implementation policy

Vendor examples may accelerate hardware bring-up, but any temporary/vendor-specific implementation must remain behind the final BSP/platform interface and be explicitly identified. No quick solution is allowed to leak into application contracts.

## Normative documents

- [`docs/IMPLEMENTATION-STRATEGY-1.0.md`](docs/IMPLEMENTATION-STRATEGY-1.0.md)
- [`docs/FIRMWARE-ARCHITECTURE-1.0.md`](docs/FIRMWARE-ARCHITECTURE-1.0.md)
- [`docs/HARDWARE-BASELINE-1.0.md`](docs/HARDWARE-BASELINE-1.0.md)
- [`docs/BUILD-0.1-SCOPE.md`](docs/BUILD-0.1-SCOPE.md)
