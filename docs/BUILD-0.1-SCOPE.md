# PRISMA-PI Lite — Build 0.1 Scope

**Purpose:** first architecture-valid local appliance and local-web foundation on Waveshare ESP32-S3-Touch-LCD-1.54.

## Objective

Prove the complete local application spine while keeping the device autonomous, web-ready and independent from Internet/cloud services.

## Required behavior

### Appliance
- autonomous boot;
- stable display and touch;
- exactly four final artistic/control pages;
- local operation without router or Internet;
- useful serial/runtime diagnostics.

### Network
- autonomous AP `PRISMA-PI-LITE`;
- local address normally `192.168.4.1`;
- local HTTP server;
- network failure does not stop local UI;
- future router/uplink configuration is optional and belongs in `/admin > Network`.

### Web
- `/` is reserved for the four-page synthesizer UI;
- `/admin` is reserved for technical administration;
- Vintage is the default synth skin;
- Modern remains a future skin;
- appliance and web must ultimately share one semantic artistic state.

## Final four appliance pages

1. Intro.
2. Now Playing / file cycle / Intensity / Mode.
3. View in Browser.
4. Core Extended.

Initial file examples include MOON and TANGO.

Core Extended is limited to Pixelate, Threshold, Posterize, Mirror, Random and Mix. Random uses enabled effects only.

## Explicit non-goals for current local/network slice

- Internet dependency;
- cloud services;
- STA/uplink configuration in the normal boot path;
- complete Vintage web synth;
- complete `/admin`;
- WebSocket synchronization;
- media streaming;
- final video processing backend;
- audio-reactive processing;
- OTA firmware update subsystem.

## Flash/update policy

Lite V1 uses full/factory or application USB flashing workflows. OTA is intentionally out of scope.

## Current acceptance criteria

- [x] hardware display/touch bring-up validated on real device;
- [x] factory/full BIN flashing path validated;
- [x] local renderer/touch foundation validated;
- [ ] final four-page appliance shell validated on hardware;
- [ ] autonomous AP visible after boot;
- [ ] browser can connect directly to `PRISMA-PI-LITE`;
- [ ] `http://192.168.4.1` loads without Internet/router;
- [ ] `/api/v1/network/status` reports local AP state;
- [ ] network client connect/disconnect does not disturb appliance UI;
- [ ] Vintage `/` consumes shared synth state;
- [ ] bidirectional appliance/web synchronization validated;
- [ ] `/admin` is isolated from artistic UI.

## Stop conditions

Do not stack further features on top of:
- unstable boot;
- unexplained resets;
- broken touch/display;
- network code that blocks local operation;
- duplicated artistic state between web and appliance;
- media work inside control/UI critical paths;
- undocumented changes to the four-page surface contract.
