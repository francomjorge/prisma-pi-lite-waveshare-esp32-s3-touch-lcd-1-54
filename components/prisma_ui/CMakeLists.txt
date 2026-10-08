idf_component_register(
    SRCS "prisma_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES prisma_bsp prisma_platform freertos
)
