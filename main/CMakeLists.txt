idf_component_register(
    SRCS "app_factory_v1.c" "wifi_portal_factory.c" "network_service.c" "security_service.c" "files_plus_api.c" "playback_http.c" "audio_service.c" "audio_api.c" "prisma_touch.c" "prisma_sensors.c" "sensor_audio_control.c" "sensors_web.c" "xy_control.c" "xy_http.c" "xy_link_web.c" "storage.c" "prisma_power.c" "prisma_metrics.c" "admin_api_v2.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server esp_driver_gpio esp_driver_spi esp_driver_i2c esp_driver_i2s esp_driver_tsens esp_lcd spiffs esp_adc esp_app_format esp_timer heap spi_flash mbedtls esp_codec_dev
    EMBED_TXTFILES "web/admin.html" "web/admin.css" "web/sensors.html"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=format-truncation)

spiffs_create_partition_image(storage ../storage FLASH_IN_PROJECT)
