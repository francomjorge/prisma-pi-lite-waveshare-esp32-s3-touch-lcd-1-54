# PRISMA-PI Lite — ESP32-S3 Implementation Strategy 1.0

**Target:** Waveshare ESP32-S3-Touch-LCD-1.54 / SKU 33869  
**Role:** hardware/runtime implementation of PRISMA-PI Lite Core 1.0  
**Status:** normative device-port strategy

## Product decision for this appliance

This controller uses a **single factory firmware**. OTA is not implemented and no OTA slots or update runtime are reserved. For the Core capability model, `UpdatePort` is `UNAVAILABLE` on this appliance.

This is a product decision, not a temporary shortcut.

## Architectural rules

1. Core semantics remain defined by `prisma-pi-lite-core`.
2. Board pins, ESP-IDF handles and controller-specific code remain inside the device/BSP layer.
3. Local appliance operation never depends on network availability.
4. Local UI and future Web UI are projections of the same logical product state.
5. Future web synchronization must not scrape the LCD framebuffer.
6. Physically validated hardware behavior becomes protected baseline behavior.
7. One independently verifiable increment is implemented at a time.

## Implementation order

### M1 — Local hardware baseline

- assert battery power hold before peripheral bring-up;
- initialize ST7789 display and backlight;
- initialize CST816S touch;
- run LVGL continuously;
- display the local image;
- provide the touch-glitch interaction;
- reserve horizontal swipe semantics for later view navigation.

### M2 — Network/web projection

Only after M1 is physically validated:

- Wi-Fi AP/network adapter;
- network status;
- HTTP/web transport;
- web image projection;
- common state/command synchronization between appliance and browser.

### M3 — Product functionality

Continue implementing the remaining Core 1.0 ports/features according to the normative architecture and capability model without changing the protected M1 hardware baseline unnecessarily.

## Flash layout

The 16 MB flash uses one `factory` application partition plus persistent/data storage. There is no `otadata`, `ota_0` or `ota_1` partition.

## Build 0.1 definition

Build 0.1 is complete in software when it:

- builds from reviewed source;
- produces a flash-at-0 factory image;
- asserts GPIO2 power hold first;
- initializes display and touch using verified Waveshare/ESP-IDF mappings;
- displays the source image;
- runs bounded-memory touch glitch effects;
- contains no network or OTA dependency.

It becomes a protected hardware baseline only after physical validation on the target appliance.
