idf_component_register(
    SRCS "prisma_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl esp_lvgl_port waveshare_bsp
)

lvgl_port_create_c_image("images/moon_240.png" "images/gen/" "RGB565" "NONE")
lvgl_port_add_images(${COMPONENT_LIB} "images/gen/")
