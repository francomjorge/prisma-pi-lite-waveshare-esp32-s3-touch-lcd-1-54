idf_component_register(
    SRCS "prisma_platform.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_timer esp_system
)
