# PRISMA-PI Lite — ESP32-S3 Implementation Strategy 1.0

**Target:** Waveshare ESP32-S3-Touch-LCD-1.54  
**Role:** hardware/runtime implementation of PRISMA-PI Lite Core 1.0  
**Status:** normative implementation strategy

---

## 1. Objective

Build a production-oriented ESP32-S3 appliance implementation without changing the PRISMA-PI Lite Core 1.0 product contracts.

The implementation must grow incrementally, but every increment must fit the final architecture. A fast proof-of-concept is acceptable only when it is isolated behind the same adapter boundary that the final implementation will use.

The first vertical build will eventually provide:

1. autonomous stable boot;
2. local LCD presentation;
3. local touch input;
4. the local interactive game/test surface;
5. a shared application state/view-model that can later be projected to the web;
6. a network layer that can be added without making the local appliance dependent on Wi-Fi;
7. an OTA layout that is present before Wi-Fi OTA is enabled.

---

## 2. Source-of-truth hierarchy

Implementation decisions follow this order:

1. **PRISMA-PI Lite Core 1.0 documentation** — product behavior, state, commands, views, ports and capabilities.
2. **This repository's architecture documents** — mapping of Core contracts onto the ESP32-S3 runtime.
3. **Official Waveshare hardware material/examples** — board wiring, controllers and board-specific initialization.
4. **ESP-IDF official APIs/components** — operating system, drivers, network, storage, OTA and runtime services.
5. Vendor/example application code — bring-up reference only.

A lower level may not redefine a higher-level contract.

---

## 3. Non-negotiable architectural rules

### 3.1 Product logic is hardware-agnostic

No application, state, command or view-model code may contain:

- GPIO numbers;
- SPI/I2C host identifiers;
- Waveshare-specific types;
- LCD/touch controller-specific calls;
- ESP-IDF driver handles.

Those details live in the BSP or a platform adapter.

### 3.2 One authoritative application state

There is one authoritative state owner. Local touch, future web commands and internal system events all enter the same semantic command path.

There must never be a separate "web state" and "device state".

### 3.3 One view-model, multiple projections

The local LCD and future web UI consume the same logical state/view-model.

The web implementation must not be built by scraping the LCD framebuffer. If exact pixel capture is later useful, it is an optional preview/snapshot capability, not the synchronization architecture.

### 3.4 Local-first operation

Network availability is never required for:

- boot;
- local display;
- local touch;
- local application/game operation.

Wi-Fi/HTTP/WebSocket may fail or restart independently while the appliance keeps running.

### 3.5 OTA is a system capability, not a Wi-Fi feature

The update engine is independent from the transport that delivers firmware bytes.

The device layout is OTA-capable from the first firmware generation. Wi-Fi OTA will later be only one transport into the update service.

### 3.6 Capabilities, not hardware identity

Application behavior is enabled through the Core `DeviceCapabilities` model. Code above the board layer must not branch on "Waveshare" or a board name.

---

## 4. Shortcut policy

A temporary implementation is allowed only when all four conditions are true:

1. it accelerates hardware validation materially;
2. it remains entirely behind the correct final interface;
3. it is explicitly marked `TEMPORARY` with the intended replacement;
4. it does not leak vendor/demo assumptions into application state or public contracts.

Examples:

- **Allowed:** adapting a Waveshare LCD initialization sequence inside `prisma_bsp` while the final driver is validated.
- **Not allowed:** letting the game call `gpio_set_level()` or `esp_lcd_panel_draw_bitmap()` directly.
- **Not allowed:** creating a second application model solely for the web page.

Whenever a shortcut is introduced, it must be mentioned in the implementation status/changelog.

---

## 5. Implementation milestones

### M0 — Architecture and build spine

Goal: establish a compile-oriented project skeleton and freeze ownership boundaries before feature work.

Deliverables:

- repository structure;
- component dependency rules;
- hardware baseline;
- ESP-IDF baseline;
- OTA-capable partition strategy;
- boot/lifecycle sequence;
- error/degraded-mode policy;
- empty/stub interfaces only where needed to compile the skeleton.

No fake hardware success is permitted in M0.

### M1 — Hardware bring-up

Goal: validate the appliance hardware through final BSP boundaries.

Deliverables:

- boot diagnostics;
- LCD init and backlight control;
- framebuffer/draw path;
- touch init and normalized input events;
- display/touch health reporting;
- no network dependency.

### M2 — Local application vertical slice

Goal: prove the Core-facing application path end to end.

Deliverables:

- authoritative state;
- command engine path;
- view-model snapshot;
- local renderer;
- touch -> semantic command path;
- local interactive game/test surface;
- static image/media presentation path.

### M3 — Network and web projection

Goal: expose the same application remotely without forking it.

Deliverables:

- appliance Wi-Fi mode;
- network status capability;
- HTTP server;
- state snapshot/event transport;
- web renderer/control surface;
- remote semantic commands through the same command engine.

### M4 — OTA over Wi-Fi

Goal: remotely update firmware without physical reflashing.

Deliverables:

- firmware upload/download transport;
- inactive-slot write;
- image validation;
- reboot into candidate image;
- post-boot health confirmation;
- rollback if candidate boot is not confirmed.

### M5 — Hardening

Goal: production behavior.

Deliverables:

- watchdog strategy;
- fault recovery;
- network restart isolation;
- memory/heap monitoring;
- long-running soak tests;
- update failure tests;
- brownout/reboot recovery tests;
- build/version diagnostics.

---

## 6. Build 0.1 definition

Build 0.1 is the first user-visible vertical slice. Its intended acceptance state is:

- appliance boots repeatedly and remains powered/running;
- local screen is usable;
- touch interaction works;
- local game/test interaction works;
- one image/state is rendered through the normal presentation pipeline;
- the same logical image/state is ready to be serialized to a future web client;
- Wi-Fi is not required for local operation;
- OTA A/B layout already exists even if wireless delivery is not yet enabled.

The web/network portion may be enabled after the local vertical slice; it must not alter the application model.

---

## 7. Definition of done for each implementation step

A step is complete only when:

1. ownership is in the correct layer;
2. failure behavior is defined;
3. no board-specific details leak upward;
4. the local appliance remains independent from network unless the feature itself is network-only;
5. logs identify initialization and failure state;
6. memory/task ownership is explicit;
7. the implementation can be extended without replacing the interface just introduced.

---

## 8. Change discipline

Before adding a feature, classify it as one of:

- Core contract implementation;
- application orchestration;
- UI projection;
- hardware/BSP;
- platform/system service;
- transport/network;
- update service.

If a feature appears to require crossing those boundaries, the architecture is reviewed before coding it.
