idf_component_register(
    SRCS "waveshare_bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_lcd esp_lvgl_port esp_lcd_touch_cst816s lvgl
)
