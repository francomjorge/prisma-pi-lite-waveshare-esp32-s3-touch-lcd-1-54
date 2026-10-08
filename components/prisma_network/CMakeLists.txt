idf_component_register(
    SRCS "prisma_network.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif esp_http_server prisma_app freertos
)
