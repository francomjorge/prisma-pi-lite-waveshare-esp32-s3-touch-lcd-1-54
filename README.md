# PRISMA-PI Lite — Waveshare ESP32-S3 Touch LCD 1.54

Hardware port for **PRISMA-PI Lite Core 1.0** targeting the Waveshare ESP32-S3-Touch-LCD-1.54 appliance.

This repository contains the device/runtime implementation only. Product behavior and domain rules remain defined by `prisma-pi-lite-core`.

## Status

**Architecture bootstrap / Build 0.1 preparation**

Initial objective:

- stable autonomous boot;
- LCD and touch support;
- local appliance UI and game loop;
- image source presentation on the appliance;
- architecture-ready web synchronization;
- Wi-Fi and OTA infrastructure prepared but introduced incrementally;
- no browser dependency for appliance operation.

## Architectural rule

This port follows the Core 1.0 **Ports & Adapters / Hexagonal Architecture**.

Dependencies point inward:

```text
Waveshare / ESP-IDF hardware drivers
              ↓
         device adapters
              ↓
       runtime services
              ↓
       Core-facing ports
              ↓
       application logic
```

Application/domain code must never include Waveshare-specific, ESP-IDF driver-specific or board pin-specific details.

## Repository layout

```text
.
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── CMakeLists.txt
│   └── app_main.c
├── components/
│   ├── prisma_contracts/
│   ├── prisma_runtime/
│   ├── prisma_device_profile/
│   ├── prisma_display/
│   ├── prisma_input/
│   ├── prisma_storage/
│   ├── prisma_media/
│   ├── prisma_network/
│   ├── prisma_web/
│   ├── prisma_update/
│   ├── prisma_system/
│   └── waveshare_bsp/
└── docs/
    ├── ARCHITECTURE-ESP32-S3-1.0.md
    ├── BUILD-0.1-SCOPE.md
    └── IMPLEMENTATION-ORDER.md
```

## Build strategy

The first executable firmware is intentionally narrow. We establish a stable hardware/runtime spine first, then add features vertically without bypassing Core contracts.

Build 0.1 implementation order:

1. Board/BSP boot.
2. Display adapter.
3. Touch/input adapter.
4. Runtime/event loop.
5. Local appliance UI/game.
6. Static image pipeline.
7. Persistent settings/NVS.
8. Network adapter.
9. Web transport and synchronization.
10. OTA update adapter.

See `docs/ARCHITECTURE-ESP32-S3-1.0.md` for the normative port structure.
