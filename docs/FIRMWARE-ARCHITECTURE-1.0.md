# PRISMA-PI Lite — ESP32-S3 Firmware Architecture 1.0

**Target:** Waveshare ESP32-S3-Touch-LCD-1.54  
**Architecture status:** baseline for implementation

---

## 1. Architectural objective

Map PRISMA-PI Lite Core 1.0 onto ESP-IDF while preserving the Core Ports & Capabilities model and isolating every board-specific decision.

The architecture is intentionally local-first and transport-neutral.

---

## 2. Repository structure

```text
/
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
    │   ├── include/
    │   └── src/
    ├── prisma_app/
    │   ├── include/
    │   └── src/
    ├── prisma_ui/
    │   ├── include/
    │   └── src/
    ├── prisma_media/
    │   ├── include/
    │   └── src/
    ├── prisma_platform/
    │   ├── include/
    │   └── src/
    ├── prisma_network/
    │   ├── include/
    │   └── src/
    ├── prisma_ota/
    │   ├── include/
    │   └── src/
    └── prisma_bsp/
        ├── include/
        └── src/
```

This is deliberately less fragmented than a one-component-per-Core-port design. Core ports remain separate interfaces, but implementation units are grouped by lifecycle and dependency domain to avoid excessive ESP-IDF component coupling.

---

## 3. Component ownership

### `prisma_core`

Hardware-neutral realization of the stable Core 1.0 contracts needed by this runtime.

Owns:

- common Core-facing data types;
- `DeviceCapabilities` representation;
- semantic commands/events;
- state and view-model contract types;
- abstract port interfaces.

Must not include ESP-IDF or Waveshare headers.

### `prisma_app`

Authoritative application orchestration.

Owns:

- application lifecycle;
- authoritative state instance;
- command dispatch;
- state transitions;
- view-model production;
- game/test interaction logic when it represents product behavior;
- coordination of media, UI and services.

Must never talk directly to board drivers.

### `prisma_ui`

Local appliance UI projection and input translation.

Owns:

- local screen composition;
- view-model -> widgets/render objects;
- normalized touch event -> semantic application command translation;
- presentation scheduling/dirty updates;
- UI-specific transient state that is not product state.

It consumes the same logical model that the future web projection consumes.

### `prisma_media`

Media/source presentation pipeline.

Owns:

- canonical image/frame representation;
- source decoding adapters implemented on this runtime;
- image scaling/conversion needed above the raw LCD driver;
- future loop/playback mechanics delegated from Core policy;
- preview surfaces where applicable.

It does not own display hardware.

### `prisma_platform`

ESP-IDF system services that are not board wiring.

Owns:

- NVS/settings persistence;
- monotonic clock/timers;
- software version/build identity;
- reset/restart primitives;
- heap/system health;
- task/watchdog integration;
- persistent device identity;
- logging policy helpers.

### `prisma_network`

Network and remote transport adapters.

Owns later:

- Wi-Fi AP/STA lifecycle;
- network status;
- HTTP serving;
- WebSocket/event transport;
- web asset serving;
- serialization of Core state/view-model;
- remote command ingress.

It does not own application state.

### `prisma_ota`

Transport-neutral update engine.

Owns:

- current/inactive OTA slot discovery;
- streamed firmware writes;
- validation status;
- candidate boot selection;
- post-boot mark-valid;
- rollback policy integration;
- update progress/error model.

It does not own Wi-Fi or HTTP.

### `prisma_bsp`

Only component that knows the exact Waveshare board wiring.

Owns:

- LCD controller and SPI wiring;
- backlight GPIO;
- touch controller and I2C wiring;
- board power/peripheral setup;
- raw display/touch handles hidden behind BSP-facing interfaces;
- board-level health.

No product state or game logic is permitted here.

---

## 4. Dependency direction

Allowed high-level dependency graph:

```text
             +------------------+
             |      main        |
             +--------+---------+
                      |
                      v
             +------------------+
             |    prisma_app    |
             +--+-----+-----+---+
                |     |     |
          +-----+     |     +----------+
          v           v                v
     prisma_ui   prisma_media     service ports
          |           |          /    |      \
          v           v         v     v       v
      prisma_bsp  prisma_bsp  platform network  ota
          ^                         |      |
          |                         +--+---+
          |                            |
          +----------------------------+

All upper layers use prisma_core contract types/interfaces.
```

Normative constraints:

- `prisma_core` depends on no hardware/runtime component.
- `prisma_bsp` depends only on ESP-IDF/vendor driver components, never on `prisma_app`.
- `prisma_platform`, `prisma_network`, `prisma_ota` may depend on ESP-IDF, but not on Waveshare wiring.
- `prisma_ui` may call a display/input facade exposed by `prisma_bsp`; it may not access GPIO/SPI/I2C directly.
- `prisma_app` depends on interfaces/capabilities, not concrete driver handles.

Circular component dependencies are forbidden.

---

## 5. Runtime ownership model

### 5.1 Authoritative state

Exactly one application controller owns mutable product state.

Readers receive snapshots/view-models. Drivers and transports never mutate state directly.

### 5.2 Commands

All interaction follows:

```text
local touch --------+
                    |
future web command -+--> semantic command queue --> application controller --> state transition
                    |
system event -------+
```

The command queue defines ordering and prevents UI/network tasks from racing on product state.

### 5.3 View-model

After relevant state transitions, the application publishes a read-only logical snapshot/version.

Consumers:

```text
                         +--> local UI renderer --> LCD
application view-model -|
                         +--> future web serializer --> browser
                         +--> diagnostics/status
```

The LCD framebuffer is therefore an output artifact, not the source of truth.

---

## 6. Concurrency strategy

Initial strategy favors a small number of explicit owners rather than a task per subsystem.

Recommended logical execution contexts:

1. **Application controller task** — owns state, consumes semantic commands.
2. **UI/LVGL task** — owns LVGL objects and local presentation.
3. **Network service task(s)** — introduced in M3 by ESP-IDF networking/server facilities.
4. **OTA operation context** — activated only during update; reports progress but never mutates application state directly.
5. Driver callbacks/ISRs — minimal; enqueue normalized events and return.

Rules:

- no blocking network operation in application/UI ownership contexts;
- no UI object access outside the UI ownership/lock rules;
- ISR callbacks never perform product logic;
- long media operations must not block touch/UI servicing;
- bounded queues and explicit overflow behavior are required.

---

## 7. Boot lifecycle

Target boot sequence:

```text
RESET
  |
  v
ESP-IDF runtime
  |
  v
platform early init
  |-- logging/build identity
  |-- NVS
  |-- reset reason
  |
  v
OTA boot-state check
  |
  v
BSP init
  |-- display
  |-- backlight
  |-- touch
  |
  v
Core capability profile
  |
  v
application state/defaults + persisted settings
  |
  v
local UI/input start
  |
  v
local application RUNNING
  |
  +--> optional network start (M3)
  |
  +--> candidate firmware mark-valid after health gate (M4)
```

Network initialization is intentionally after local functionality.

---

## 8. Failure/degraded-mode policy

### Display failure

Critical for this appliance's local-control profile. Record diagnostics and enter a deterministic degraded/fault state; do not report successful local UI capability.

### Touch failure

Display may remain alive in read-only/degraded mode. Input capability is reported unhealthy/unavailable. The process must not reboot-loop solely because touch is absent.

### Network failure

Nonfatal. Local UI/application continue. Network service may retry/restart independently.

### Storage/settings failure

Boot with safe defaults where possible, report degraded persistence health, and avoid corrupt writes.

### OTA failure before boot selection

Abort update and retain current running image.

### Candidate OTA boot failure

Rollback to previous valid slot once OTA rollback is enabled.

---

## 9. Display and memory strategy

The physical display is 240 x 240 RGB565.

Do not allocate redundant full-frame buffers by default merely to support web mirroring. The UI renderer owns the buffers required by its rendering library/driver. Web state is produced from the shared view-model.

If a future PreviewPort requires exact pixel snapshots, it is added explicitly with memory/FPS limits in `DeviceCapabilities`.

External PSRAM may be used for suitable non-latency-critical allocations after M1 validation, but correctness must not rely on arbitrary heap placement.

---

## 10. Network/web architecture for M3

```text
Browser
   |
HTTP/WebSocket
   |
prisma_network
   |             +--------------------+
   +-- query --->| view-model snapshot|
   |             +--------------------+
   |
   +-- command --> semantic command queue --> prisma_app
```

The browser is another control/presentation client, not a second application runtime owner.

---

## 11. OTA architecture for M4

```text
HTTP/Wi-Fi/file transport
          |
          v
   firmware byte stream
          |
          v
      prisma_ota
   validate/write slot
          |
          v
      ota_0 / ota_1
          |
          v
 reboot -> health gate -> mark valid
                    \
                     +-> rollback on failure
```

Transport can later change without changing `prisma_ota`.

---

## 12. Planned port mapping

Core ports are implemented across the components above rather than becoming one ESP-IDF component each.

| Core contract | ESP32-S3 implementation owner |
|---|---|
| DisplayPort | `prisma_ui` + `prisma_bsp` |
| InputPort | `prisma_ui` + `prisma_bsp` |
| StoragePort | `prisma_media` / `prisma_platform` |
| SettingsPort | `prisma_platform` |
| NetworkPort | `prisma_network` |
| WebTransportPort | `prisma_network` |
| DecoderPort | `prisma_media` |
| VideoProcessorPort | later processing backend inside `prisma_media` or dedicated component when justified |
| AudioInputPort | future adapter, capability initially reflects implementation status |
| AudioProcessorPort | future adapter |
| PreviewPort | `prisma_media` + transport/display adapters |
| OutputPort | future output adapter |
| ClockPort | `prisma_platform` |
| SystemPort | `prisma_platform` |
| UpdatePort | `prisma_ota` |
| ModuleExecutionPort | future backend; separate component only when implemented |

Unsupported or not-yet-implemented functions are declared through `DeviceCapabilities`; they are never simulated as available.

---

## 13. Naming rule

- Core concepts use PRISMA/Core terminology.
- ESP-IDF concepts remain inside platform/driver layers.
- Board names and controller names remain inside BSP implementation/configuration.
- Public component headers expose semantic concepts rather than ESP-IDF handles whenever practical.

---

## 14. Architecture change rule

This document is the implementation baseline. If a later feature requires violating a boundary here, change the architecture explicitly first and document why; do not silently bypass it in feature code.
