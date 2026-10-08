# PRISMA-PI Lite — UI Surfaces 1.0

This document freezes the V1 surface model agreed for the appliance and local web UI.

## 1. Appliance ESP32-S3 — exactly four pages

The 240x240 appliance exposes artistic controls only. Technical administration must not leak into this UI.

### Page 1 — Intro
- PRISMA-PI Lite identity.
- VIDEO SYNTHESIZER.
- Four-page position indicator.

### Page 2 — Now Playing / main performance control
- NOW PLAYING file selector.
- Initial file set includes `MOON` and `TANGO`; more files may be added later.
- Touching/cycling the file selector changes the current source.
- INTENSITY control.
- MODE / BYPASS control.
- This page is part of the shared synth state and must later mirror bidirectionally with web `/`.

### Page 3 — View in Browser
- Local network/browser access information.
- For the current local-AP milestone the device address is normally `192.168.4.1`.
- This appliance page does not carry technical administration controls.

### Page 4 — Core Extended
Controller set is intentionally limited to the agreed processing controls:
- Pixelate — title toggles enabled state; adjacent bar controls amount.
- Threshold — title toggles enabled state; adjacent bar controls amount.
- Posterize — title toggles enabled state; adjacent bar controls amount.
- Mirror — ON/OFF.
- Random — amount control.
- Mix — amount control.

`Random` operates only on processing effects that are currently enabled. Disabled effects are excluded from random generation.

The appliance may use simple page switching and/or swipe navigation. It must stay small, direct and performance-oriented.

## 2. Web `/` — synthesizer, not administration

The root URL is the browser version of the synthesizer.

- Exactly four switchable/swipeable pages corresponding to the appliance model.
- Page 1: Intro.
- Page 2: Now Playing + file cycle + Intensity + Mode.
- Page 3: Live Preview only.
- Page 4: Core Extended.
- No permanent left administration sidebar.
- Controls use the same semantic state as the appliance.
- Changes must eventually propagate bidirectionally: appliance -> web and web -> appliance.

### Skins
- `Vintage` is the default and the only skin implemented first.
- Architecture must allow a future `Modern` skin without changing state semantics or control behavior.
- A skin is presentation only; it must not fork product logic.

## 3. Web `/admin` — technical administration

`/admin` is a separate, expanded technical interface. It is not one of the four synth pages.

Planned administration domains:
- Network / Wi-Fi / future uplink configuration.
- Device and system status.
- Input/output infrastructure.
- Streaming configuration and diagnostics.
- Web/control status.
- Software version.
- Manual Update Status check only.
- Reset Settings and maintenance tools.

Initial prototype login:
- Username: `PrismaP`
- Password: `PrismaP`

These prototype credentials are not a production security policy.

## 4. State ownership and synchronization

There is one logical synth state. Appliance UI and web `/` are two projections of that state.

Examples of shared state:
- current file;
- intensity;
- bypass/mode;
- effect enabled flags;
- effect values;
- random amount;
- mix;
- live-preview state.

`/admin` owns technical configuration views but must not create a second artistic state model.

## 5. Scope boundary

Current milestone M4.1 implements the autonomous local network and the final four-page appliance shell. The complete Vintage `/`, authenticated `/admin`, WebSocket synchronization and live media preview are subsequent M4.x increments built on this contract.
