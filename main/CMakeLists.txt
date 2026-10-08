idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES prisma_bsp prisma_platform prisma_ui prisma_network
)
