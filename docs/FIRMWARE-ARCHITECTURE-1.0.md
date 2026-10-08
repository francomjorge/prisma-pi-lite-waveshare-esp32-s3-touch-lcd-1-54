# PRISMA-PI Lite — ESP32-S3 Firmware Architecture 1.1

**Target:** Waveshare ESP32-S3-Touch-LCD-1.54  
**Framework:** ESP-IDF 5.5.1  
**Product rule:** local-first, no OTA subsystem in Lite V1.

## 1. Architectural objective

PRISMA-PI Lite must keep board hardware, artistic state, web presentation, networking and media processing separated so the appliance UI and browser can become two synchronized projections of the same synthesizer.

The device must remain useful without Internet, cloud or external router.

## 2. Component ownership

### `prisma_bsp`
Only component that knows exact Waveshare wiring and raw display/touch hardware.

### `prisma_platform`
NVS/settings, software version, uptime, heap/system health and reset primitives.

### `prisma_ui`
The four-page 240x240 appliance presentation and touch-to-semantic-control translation.

### `prisma_network`
Local AP lifecycle, HTTP server, future WebSocket/control transport and future optional STA/uplink configuration. Network failure must never stop local operation.

### `prisma_app`
Will become the authoritative artistic state owner and semantic command dispatcher.

### `prisma_media`
Will own local image/video files, loop/playback, processing frames, preview and media-plane adapters.

### `prisma_core`
Hardware-neutral shared types/contracts/state semantics.

## 3. V1 runtime topology

```text
                       +----------------------+
 touch ---------------->| shared synth state  |<---------------- web command
                       | / command model      |
                       +----+------------+----+
                            |            |
                            v            v
                       appliance UI    web `/`
                            |
                            v
                         display

 local AP / HTTP / WS are transport adapters, never state owners.
```

Exactly one logical synth state is allowed once bidirectional synchronization is introduced.

## 4. Boot lifecycle

```text
RESET
  -> ESP-IDF runtime
  -> platform/NVS
  -> BSP display + touch
  -> autonomous local AP
  -> local HTTP service
  -> four-page appliance UI
  -> RUNNING
```

The local UI must remain operational even if network initialization fails.

## 5. Network policy

### Current M4.1
- Device creates autonomous AP `PRISMA-PI-LITE`.
- Default local address `192.168.4.1`.
- No router required.
- No Internet required.
- HTTP is local to the appliance network.
- AP is open during prototype testing.

### Future
Optional STA/uplink configuration may be exposed only through `/admin > Network`. It must remain optional and must not replace autonomous local operation as the product baseline.

## 6. Web surface separation

### `/`
Artistic synthesizer UI. Four pages only:
1. Intro.
2. Now Playing / Intensity / Mode.
3. Live Preview.
4. Core Extended.

Vintage is default. Modern is a future skin over the same state model.

### `/admin`
Technical administration only: network, system, status, diagnostics, streaming configuration, software version, manual Update Status and Reset Settings.

Prototype credentials: `PrismaP / PrismaP`.

## 7. Appliance surface contract

Exactly four pages:
1. Intro.
2. Now Playing / file cycle / Intensity / Mode.
3. View in Browser.
4. Core Extended.

Core Extended controls:
- Pixelate.
- Threshold.
- Posterize.
- Mirror.
- Random.
- Mix.

Continuous effect rows have an enabled state and amount. Random only varies enabled processing effects.

## 8. Control plane vs media plane

The control plane includes HTTP/API/WebSocket state and commands.

The media plane includes decoded video frames, effects, preview and future stream transports.

They must remain separated so a slow stream cannot freeze touch or control synchronization.

## 9. Failure policy

- Display failure: degraded/fault state.
- Touch failure: display may remain read-only.
- Network failure: nonfatal; local synth stays alive.
- Settings failure: safe defaults where possible.
- Media failure: control UI remains responsive and reports unavailable media state.

## 10. Update policy

Lite V1 does not implement OTA firmware updates. Firmware is flashed by USB/manufacturing/recovery workflows. `Update Status` is a product/status concept only and must not imply an OTA engine.

The stale `prisma_ota` architecture is retired.

## 11. Architecture change rule

Do not bypass these boundaries for convenience. If a future feature requires a new dependency or state owner, change this document first and make the decision explicit.
