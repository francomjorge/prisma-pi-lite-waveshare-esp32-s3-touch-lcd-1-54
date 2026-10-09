# M4.11 Controller Baseline

Stable checkpoint after hardware validation.

Scope:
- Page 2 read-only sync from appliance state.
- Page 4 read-only sync from appliance effect controls.
- Global serialized web sync regardless of selected page.
- Page 3 Live View follows current source.
- OFF state renders Page 3 as pure black/no-signal while background state remains synchronized.
- Full-screen Live View retained.
- Page 5 / Sync Lab removed.
- No administration functionality in this checkpoint.

Validated release filesystem:
- storage offset: 0xC20000
- storage size: 0x3E0000 (3968 KiB)
- storage SHA-256: 6b0671fd28b3e4ced2e1f9762ac4431057043856568848a79913984417096aa9
- F1_MOON.JPG: validated 300x400 JPEG
- F1_TANGO.JPG: validated 300x400 JPEG

Release factory is assembled from the successful M4.11 firmware build plus the validated filesystem above.
