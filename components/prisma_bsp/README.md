# prisma_bsp

The only layer allowed to know the exact Waveshare ESP32-S3-Touch-LCD-1.54 wiring and controller details.

M1 owns ST7789 SPI display initialization, backlight, CST816S I2C touch initialization, board power/peripheral setup and raw hardware health.

No product state, game logic, web logic or Core policy is allowed here.
