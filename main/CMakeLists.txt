idf_component_register(
    SRCS "app36.c" "wifi_portal.c" "prisma_touch.c" "prisma_sensors.c" "storage.c" "prisma_power.c" "prisma_metrics.c" "admin_api.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_driver_gpio esp_driver_spi esp_driver_i2c esp_driver_tsens esp_lcd spiffs json esp_adc esp_app_format esp_timer esp_psram spi_flash
    EMBED_TXTFILES "web/index.html" "web/style.css" "web/admin.html" "web/admin.css" "web/admin.js"
)

spiffs_create_partition_image(storage ../storage FLASH_IN_PROJECT)
