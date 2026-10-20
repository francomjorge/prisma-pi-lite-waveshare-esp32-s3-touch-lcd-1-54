# PRISMA-PI Lite — nanoPAD V1

V1 is the infrastructure checkpoint before USB-MIDI mapping begins.

## Appliance UI

Only page 1 is enabled.

- PRISMA-PI LITE home remains the existing animated/glitch surface.
- Touch destroys/glitches the home surface as before.
- Returning to the home page in later iterations must redraw it cleanly.
- Swipe routing remains in the runtime even though V1 exposes only one page.
- Page dots are centered dynamically from the active page count, so V2 and V3 can add one page at a time without changing the gesture model.

## Power

- Runtime power-off requires a continuous 5 second PWR hold.
- The existing 2 second + release restart gesture is retained.
- Battery startup retains the previously validated 3 second hold.
- With USB power still attached, OFF is emulated with backlight/Wi-Fi off and the firmware waits for a 3 second PWR hold to restart.

## Storage

The existing SPIFFS partition is retained. The 16 MB flash layout reserves 0x3E0000 bytes (3.875 MiB) for `storage`.

V1 keeps the two proven embedded image assets:

- `F1_MOON.JPG`
- `F1_TANGO.JPG`

A four-slot logical media registry is added on top of SPIFFS. V1 fills available slots from the boot-time asset scan and leaves unused slots empty. The existing visual-source API remains compatible. Future iterations can attach a type to a slot (image/sample/etc.) without replacing the filesystem.

## Wi-Fi / Web administration

The existing autonomous SoftAP and web/admin surface remain enabled without adding appliance pages for them.

- AP: `PRISMA-PI-xxxx`
- default address: `http://192.168.4.1`
- `/admin` remains available.
- Existing overview, storage, network and sensor APIs remain available.

This web path is intentionally kept as the future high-detail nanoPAD diagnostic console while the appliance UI stays compact.

## Deferred to V2

- USB Host enumeration
- nanoPAD2 identification
- 16 pad activity
- six nanoPAD control buttons
- four scene LEDs

## Deferred to V3

- XY position
- Touch Scale / Gate Arp state
- key, scale, range, octave and tempo decoding
