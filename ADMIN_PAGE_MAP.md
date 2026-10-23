# PRISMA PI Lite — Administrative page map

Baseline: GitHub Actions build 357 / commit `1cb48af274c5d27fa3b0b7f61bcc8accd5264931`.

This file locks the administrative page mapping before changing the validated factory UI.

## Configuration navigation

1. DEVICE
2. NETWORK
3. SECURITY
4. STORAGE
5. AUDIO
6. SENSORS
7. SYSTEM

The normal instrument pages (SENSOR AUDIO, MUSICAL SCALE, ORIENTATION, GYROSCOPE, SPEAKER) are outside this mapping and must not be changed by the administrative-page work.

## Page responsibilities

### DEVICE
- Device family / ID
- Device name
- MAC address
- Network hostname only when it represents a real configured/active facility
- BACK TO PRISMA is available here

### NETWORK
- Wi-Fi mode
- SSID
- STA connection state
- IP address
- AP state
- Network recovery/forget action only when backed by the existing network implementation

### SECURITY
- Administrative user
- Password state (configured / not configured)
- Web-control protection state
- Never display a password

### STORAGE
- Storage readiness
- File count
- Used/free/total capacity when available
- Filesystem/storage state

### AUDIO
- Audio engine state
- Local speaker state
- Root
- Musical scale
- Waveform
- This page reports/controls existing audio state; it must not replace or duplicate the normal instrument pages

### SENSORS
- Display/touch state where applicable
- IMU state
- Other sensors only when actually detected/supported by the firmware
- No placeholder hardware presented as real

### SYSTEM
- Firmware/build identity
- Uptime
- Heap / PSRAM information
- General runtime status

## UI rules

- `SETTINGS_PAGE_COUNT` remains 7.
- Order is exactly: DEVICE -> NETWORK -> SECURITY -> STORAGE -> AUDIO -> SENSORS -> SYSTEM -> DEVICE.
- Seven page dots are retained.
- BACK TO PRISMA belongs only to DEVICE; pages 2–7 use the recovered vertical space for their own information/actions.
- No mDNS implementation is part of this iteration.
- No Wi-Fi architecture change is part of this iteration.
- No audio-engine redesign is part of this iteration.
- Build 357 remains the rollback/golden reference.
