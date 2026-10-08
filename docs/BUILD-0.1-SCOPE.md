# PRISMA-PI Lite — Build 0.1 Scope

**Purpose:** first useful vertical slice on Waveshare ESP32-S3-Touch-LCD-1.54  
**Architecture:** PRISMA-PI Lite Core 1.0 + ESP32-S3 Firmware Architecture 1.0

---

## 1. Build 0.1 objective

Prove the complete local application spine on the real appliance while keeping the implementation ready for web synchronization and OTA.

Build 0.1 is not a feature-complete PRISMA release. It is the first architecture-valid executable baseline.

---

## 2. Required behavior

### Appliance

Build 0.1 must eventually:

- boot autonomously;
- remain running continuously;
- initialize the 240 x 240 display;
- initialize touch;
- show a local application surface;
- run the agreed local interactive game/test surface;
- display the selected/embedded test image through the normal media/presentation path;
- accept local touch interaction through semantic commands;
- expose useful boot/runtime diagnostics over the development console.

### Shared state/model

The image, game/application state and UI state that are relevant remotely must originate from the normal application/view-model path.

No feature may be implemented in a way that forces the future web UI to read the LCD pixels to understand device state.

### Network

Network is architecturally present but does **not** block the initial local implementation.

When introduced during the Build 0.1 sequence, the appliance will provide a Wi-Fi path and web projection without changing application ownership.

### OTA

Build 0.1 project/flash structure must be OTA-ready from the beginning.

Wireless OTA transport itself is enabled only after local hardware and network behavior are stable.

---

## 3. Explicit non-goals for the first local slice

Do not implement yet unless required by a dependency test:

- audio capture/processing;
- speaker playback;
- IMU interaction;
- Bluetooth;
- production media library;
- advanced video effects;
- external program-output sinks;
- full account/login flow;
- cloud service dependencies;
- production-grade web UI;
- automatic OTA update checking.

These features remain represented by the Core architecture/capability model but are reported unavailable until implemented.

---

## 4. Implementation sequence inside Build 0.1

### Step 0 — Project spine

- ESP-IDF project files;
- component skeleton;
- build/version identity;
- sdkconfig baseline;
- OTA-compatible partition table;
- boot log/lifecycle framework.

**Exit:** clean compile/link with no fake hardware-ready status.

### Step 1 — BSP/display

- SPI/LCD initialization;
- backlight;
- display health;
- deterministic test pattern.

**Exit:** repeated boots produce a stable screen.

### Step 2 — BSP/touch

- I2C/touch initialization;
- coordinate normalization;
- touch health;
- event queue into the UI/input adapter.

**Exit:** touch points are visible in diagnostics/test UI without direct application mutation.

### Step 3 — Application state/command/view-model

- one authoritative state owner;
- bounded semantic command queue;
- immutable/versioned view-model snapshot;
- capability profile for implemented features.

**Exit:** application behavior can be driven without any LCD-specific code.

### Step 4 — Local UI/game

- LVGL/local renderer integration;
- game/test surface;
- touch -> semantic commands;
- renderer consumes view-model only.

**Exit:** interactive local appliance works without Wi-Fi.

### Step 5 — Image/media path

- canonical image/frame abstraction;
- embedded/static test source initially if appropriate;
- decode/convert/scale path;
- same image identity/state represented in view-model.

**Exit:** image is displayed through the production-shaped pipeline, not a one-off LCD bitmap call from the application.

### Step 6 — Persistence minimum

- NVS initialization;
- settings schema/version;
- safe defaults;
- reset path.

**Exit:** reboot behavior is deterministic and settings storage cannot corrupt product state.

### Step 7 — Network/web

- Wi-Fi service;
- local address/status;
- HTTP transport;
- web view receives the same logical state/image description;
- remote command path enters the same command queue.

**Exit:** appliance and browser reflect the same authoritative application model.

### Step 8 — OTA transport

- update endpoint/delivery path;
- inactive-slot write;
- progress/errors;
- reboot/candidate validation;
- rollback test.

**Exit:** a new compatible firmware can be installed over Wi-Fi without USB and a bad candidate cannot permanently remove the last valid firmware.

---

## 5. Build 0.1 acceptance criteria

Build 0.1 is accepted only when all are true:

- [ ] repeated autonomous boot works;
- [ ] no network is needed for local boot/use;
- [ ] display and touch use `prisma_bsp` boundaries;
- [ ] local touch never changes product state directly;
- [ ] one application controller owns state;
- [ ] local renderer consumes the shared view-model;
- [ ] local game/test surface is responsive;
- [ ] test image uses the media/presentation path;
- [ ] web projection, once enabled, consumes the same state model;
- [ ] remote commands, once enabled, use the same semantic command path;
- [ ] OTA A/B partitions exist before OTA transport is enabled;
- [ ] current valid firmware survives interrupted/failed update testing;
- [ ] no vendor demo application logic has leaked above BSP/platform adapters;
- [ ] all intentional shortcuts are documented.

---

## 6. Stop conditions

Do not continue stacking features if one of these occurs:

- unstable boot;
- unexplained reset/watchdog behavior;
- board-specific calls appear above BSP;
- multiple mutable state owners appear;
- web implementation requires duplicating application state;
- OTA requires changing the deployed partition architecture;
- memory usage is unknown or repeatedly exhausts internal RAM/PSRAM.

Fix the architectural/runtime issue first, then continue.
