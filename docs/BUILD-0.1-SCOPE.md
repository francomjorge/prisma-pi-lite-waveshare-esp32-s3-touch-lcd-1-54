# Build 0.1 — local appliance vertical slice

## In scope

- Waveshare ESP32-S3-Touch-LCD-1.54 / SKU 33869.
- ESP-IDF 5.5.1 baseline.
- Power hold asserted on GPIO2 before peripheral initialization.
- ST7789 LCD, 240x240, SPI.
- CST816S touch, I2C.
- Local HOME projection with the source image.
- Touch-glitch interaction: a short tap adds local colored pixel/block artifacts.
- Horizontal movement is reserved for the future six-view navigation path.
- Single factory application partition; no OTA partition and no update runtime.
- Wi-Fi and web transport are deliberately out of this increment.

## Architecture mapping

- hardware details remain inside `waveshare_bsp`;
- local UI remains inside `prisma_ui`;
- application entry point orchestrates adapters but contains no board pin mapping;
- future network/web adapters will consume the same application state rather than scrape the LCD;
- `UpdatePort` capability for this appliance is `UNAVAILABLE` by product decision.

## Physical acceptance criteria

1. Flash the merged factory image at address `0x0`.
2. Appliance remains powered on from battery/USB without dropping the power hold.
3. LCD remains illuminated and stable.
4. Moon image is visible on the 240x240 display.
5. Touches create accumulating glitch blocks.
6. Horizontal swipe does not create a glitch burst.
7. Reboot repeats the same behavior.

Status remains **software-built, not hardware validated** until these checks pass on the target appliance.
