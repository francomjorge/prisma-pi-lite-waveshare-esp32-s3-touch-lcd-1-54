# PRISMA-PI Lite — UI Surfaces 1.0

## Appliance (ESP32-S3, 240x240)
The appliance exposes exactly four artistic/control surfaces. Technical administration must not leak into this UI.

1. Intro / identity
2. Now Playing + file cycle + Intensity + Mode
3. View in Browser / browser access state
4. Core Extended controller

Core Extended controls are limited to the agreed controller set:
- Pixelate (label toggles enabled state + intensity)
- Threshold (label toggles enabled state + intensity)
- Posterize (label toggles enabled state + intensity)
- Mirror (on/off)
- Random (enabled state + amount)
- Mix (enabled state + amount)

Random operates only on effects that are currently enabled.

## Web `/`
The root URL is the synthesizer surface, not administration.
- Four switchable/swipeable pages matching the appliance state model.
- Page 1: Intro.
- Page 2: Now Playing + file cycle + Intensity + Mode.
- Page 3: Live Preview only.
- Page 4: Core Extended.
- Bidirectional synchronization with the appliance.
- Vintage skin is the default for V1.
- Architecture must leave room for a future Modern skin without changing state semantics.

## Web `/admin`
Technical administration is separate from the synthesizer surface.
- Network/Wi-Fi
- Device/system state
- Input/output infrastructure
- Streaming infrastructure
- Diagnostics
- Version/update status
- Reset/maintenance

Initial prototype credentials:
- Username: PrismaP
- Password: PrismaP

## State ownership
UI skins are presentation only. Appliance UI, web synthesizer and admin must consume the same semantic state and command model so changes can propagate bidirectionally without UI-specific business logic.
