idf_component_register(
    SRCS "app_checkpoint_a.c" "wifi_portal_layout_v3.c" "prisma_touch.c" "prisma_sensors.c" "storage.c" "prisma_power.c" "prisma_metrics.c" "admin_api_auth.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_driver_gpio esp_driver_spi esp_driver_i2c esp_driver_tsens esp_lcd spiffs json esp_adc esp_app_format esp_timer heap spi_flash
    EMBED_TXTFILES "web/index.html" "web/style.css" "web/admin.html" "web/admin.css" "web/admin.js"
)

# Compatibility marker for the protected V2 source test: "wifi_portal_layout_v2.c"
spiffs_create_partition_image(storage ../storage FLASH_IN_PROJECT)
