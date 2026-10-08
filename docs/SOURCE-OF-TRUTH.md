# PRISMA-PI Lite — Source of Truth

Hardware target: Waveshare ESP32-S3-Touch-LCD-1.54.

From branch `m4.3-consolidated` onward this repository is the only firmware source for this hardware model.

## Active appliance source

- `main/app36.c` — appliance entry point / read-only web mirror wrapper.
- `main/main.c` — appliance LCD UI, touch/swipe navigation and effect controls.
- `main/prisma_touch.c` — CST816S touch driver.
- `main/storage.c` — local source storage.

## Active web source

There is one web source only:

- `main/web/index.html`
- `main/web/style.css`
- `main/web/app.js`

`main/CMakeLists.txt` embeds these exact three files with `EMBED_TXTFILES`.

`main/wifi_portal.c` serves them at:

- `/` -> `index.html`
- `/style.css` -> `style.css`
- `/app.js` -> `app.js`
- `/api/state` -> appliance read-only state
- `/source/<filename>` -> image from SPIFFS storage

No HTML embedded inline in a C source is part of the active application build.

## Baseline rule

M4.2i is the imported appliance baseline. Web page 4 must be developed and tested on top of this consolidated branch, never in another repository.

Before a firmware binary is delivered, web layout/navigation must be tested independently at a phone-sized viewport and CI must complete successfully.
