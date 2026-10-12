# PRISMA-PI Lite Waveshare ESP32-S3 — Mobile Protocol 2.0 Preparation

**Status:** Preparation only  
**Date:** 2026-10-12

This branch exists to prepare the current physically validated ESP32-S3 appliance for future PRISMA mobile-controller interoperability without altering the validated runtime baseline.

## Rules

- Do not modify current touch/UI behavior on this branch until the mobile protocol contract is frozen.
- Do not replace the current web controller or `/api/state` compatibility behavior yet.
- Do not change filesystem partitions or physical media assets yet.
- Do not introduce security mechanisms until RAM/flash/network constraints have been measured.
- Add new Protocol 2.0 services beside legacy services first; remove legacy behavior only after physical validation.

## Existing compatibility surface to inventory

- local AP/network initialization;
- current state endpoint(s);
- current command endpoint(s);
- `/source/<filename>` media access;
- current device state serialization;
- current storage/media lookup;
- current browser synchronization assumptions.

## Target additions after Core 2.0 freeze

1. Device identity endpoint.
2. Capability endpoint.
3. Protocol version negotiation.
4. Authenticated pairing/session mechanism suitable for ESP32-S3 constraints.
5. Persistent state event channel if resource measurements permit it.
6. Preview endpoint capability declaration.
7. File-transfer capability declaration.
8. Diagnostics/health endpoint.
9. Compatibility adapter for legacy `/api/state` clients.

## Validation principle

Every addition must be validated on physical hardware for boot stability, AP stability, heap usage, stack usage, filesystem usage, latency and reconnect behavior before it is allowed into the validated appliance baseline.
