idf_component_register(
    SRCS "prisma_bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_lcd
        driver
        espressif__esp_lcd_touch_cst816s
)
