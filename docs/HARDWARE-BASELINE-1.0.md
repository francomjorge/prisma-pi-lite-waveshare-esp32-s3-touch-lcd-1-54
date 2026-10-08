# PRISMA-PI Lite — Hardware Baseline 1.0

**Target product:** Waveshare ESP32-S3-Touch-LCD-1.54  
**SKU:** 33869  
**Purpose:** accepted hardware facts for PRISMA-PI implementation

---

## 1. Verification policy

Only values confirmed by official Waveshare product documentation or the official example repository are accepted here.

Board-specific values live in `prisma_bsp`. Application code must not reproduce these constants.

A vendor demo is not copied wholesale into the product. Initialization sequences may be adapted behind BSP interfaces.

---

## 2. Confirmed board resources

Official Waveshare documentation for SKU 33869 identifies:

- SoC: ESP32-S3R8;
- CPU: dual-core Xtensa LX7, up to 240 MHz;
- internal SRAM: 512 KB;
- ROM: 384 KB;
- PSRAM: 8 MB;
- external Flash: 16 MB;
- Wi-Fi: 2.4 GHz 802.11 b/g/n;
- Bluetooth: BLE 5;
- LCD: 1.54 inch, 240 x 240;
- LCD controller: ST7789 family;
- LCD interface: 4-wire SPI;
- touch: capacitive;
- touch controller: CST816 family;
- touch interface: I2C;
- USB Type-C;
- onboard TF-card slot;
- onboard microphone/audio devices;
- onboard QMI8658 6-axis IMU;
- battery charging/discharging support;
- BOOT, PLUS and PWR controls.

Only the resources required by the current milestone are initialized. Presence of hardware does not imply an enabled PRISMA Core capability.

---

## 3. Confirmed display/touch reference configuration

The official ESP-IDF 5.5.1 LVGL example for the Touch-LCD-1.54 uses:

```text
LCD logical resolution: 240 x 240
LCD pixel format:       RGB565 / 16 bpp
LCD controller:         ST7789
LCD bus:                SPI
LCD SPI host example:   SPI3_HOST
LCD pixel clock:        40 MHz

LCD SCLK:               GPIO 38
LCD MOSI:               GPIO 39
LCD RESET:              GPIO 40
LCD DC:                 GPIO 45
LCD CS:                 GPIO 21
LCD BACKLIGHT:          GPIO 46

Touch controller:       CST816S driver in official example
Touch bus:              I2C
Touch bus index:        0 in simple official example
Touch I2C clock:        400 kHz
Touch SCL:              GPIO 41
Touch SDA:              GPIO 42
Touch INT:              GPIO 48
Touch RESET:            GPIO 47
```

These values are the M1 bring-up baseline. They are defined once inside the BSP implementation.

Note: another newer vendor BSP example contains broader board support and inconsistent copied board-name text. Therefore PRISMA will use the simple device-specific Touch-LCD-1.54 example plus the official product documentation as the initial reference, and will validate all active pins on the physical unit before broadening the BSP.

---

## 4. ESP-IDF baseline

Waveshare currently supplies ESP-IDF 5.5.1 examples for this board. The initial PRISMA implementation baseline is therefore:

```text
Target:            esp32s3
ESP-IDF baseline:  5.5.1
Flash mode:        QIO
Flash size:        16 MB
PSRAM:             enabled
PSRAM mode:        Octal
PSRAM speed:       80 MHz in vendor reference configuration
CPU frequency:     240 MHz in vendor reference configuration
```

We pin the development environment to a verified ESP-IDF release instead of following an unbounded latest version. An IDF upgrade is a deliberate platform change and must pass the hardware smoke tests again.

---

## 5. Power/always-on policy

PRISMA appliance behavior is **continuous operation by default**.

Firmware policy:

- no automatic deep sleep;
- no automatic light sleep unless explicitly validated and later required;
- no application timeout that powers down the appliance;
- backlight policy is separate from CPU/runtime power state;
- watchdogs recover faults rather than intentionally sleeping the unit;
- USB/external power operation must boot directly into the application;
- battery-power hold/power-button behavior must be verified against the SKU 33869 schematic/physical board during M1 before a GPIO-level power-latch implementation is accepted.

We will not copy a power-hold circuit definition from a different Waveshare model.

---

## 6. Memory strategy

### Internal RAM

Reserve for latency-sensitive runtime structures, task stacks, queues, DMA-capable memory where required, and critical state.

### PSRAM

Candidate uses after validation:

- UI draw buffers where supported;
- media decode/working buffers;
- image assets/runtime caches;
- future network serialization buffers when appropriate.

PSRAM is not treated as a substitute for ownership discipline. Large duplicated framebuffers are avoided.

### Flash

16 MB allows an A/B OTA layout plus persistent data. The exact partition table is part of M0 and must keep both OTA application slots equal-sized.

---

## 7. OTA partition design constraints

The partition table must contain at least:

- NVS;
- OTA metadata (`otadata`);
- PHY initialization data if required by the selected IDF configuration;
- `ota_0`;
- `ota_1`;
- optional persistent asset/storage partition only if justified by the media strategy.

Rules:

1. `ota_0` and `ota_1` are equal size.
2. Each application slot must fit the complete production firmware, not only Build 0.1.
3. No future Wi-Fi OTA implementation may require repartitioning a deployed unit.
4. Persistent user/settings data must not live inside an OTA app slot.
5. Partition sizing is frozen only after the first full dependency build gives a realistic firmware-size baseline; until then, the 16 MB capacity and A/B requirement are fixed, but final slot sizes remain an M0 engineering decision.

---

## 8. Hardware capabilities not enabled yet

The board includes additional hardware that is intentionally not part of the first implementation slice:

- microphone/audio codec path;
- speaker/amplifier path;
- QMI8658 IMU;
- TF card;
- Bluetooth;
- battery telemetry;
- additional buttons beyond what is needed for bring-up.

They remain future adapter candidates. Their presence must not increase Build 0.1 complexity.

---

## 9. M1 hardware acceptance tests

Before considering the BSP stable:

1. cold boot from USB power at least repeatedly without manual intervention;
2. reset/reboot loop test;
3. LCD solid-color and orientation test;
4. backlight on/off/level behavior if PWM is implemented;
5. touch coordinates at corners/center;
6. touch interrupt/release behavior;
7. 30+ minute UI interaction run without watchdog reset;
8. heap/PSRAM diagnostics recorded;
9. physical power behavior validated on USB and, if installed, battery;
10. board-specific constants confirmed against the actual SKU 33869 unit.
