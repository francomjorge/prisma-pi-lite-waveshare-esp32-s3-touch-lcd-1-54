idf_component_register(
    SRCS "app_factory_v1.c" "wifi_portal_factory.c" "prisma_touch.c" "prisma_sensors.c" "storage.c" "prisma_power.c" "prisma_metrics.c" "admin_api_factory.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_driver_gpio esp_driver_spi esp_driver_i2c esp_driver_tsens esp_lcd spiffs esp_adc esp_app_format esp_timer heap spi_flash
    EMBED_TXTFILES "web/admin.html" "web/admin.css"
)

spiffs_create_partition_image(storage ../storage FLASH_IN_PROJECT)
