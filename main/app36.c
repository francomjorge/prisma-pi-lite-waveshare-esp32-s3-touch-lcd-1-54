#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.h"

/*
 * M4.5 unified runtime.
 * Keep the validated appliance renderer/runtime in main.c, but expose it as
 * prisma_runtime_main so this file can initialize storage and run the same
 * appliance-authoritative publisher pattern that was proven in Build 58.
 *
 * Important: unlike the previous wrapper, the renderer state is NOT renamed
 * to a second set of prisma_shared_* variables. The publisher reads the real
 * runtime variables directly, so there is one appliance state and one web
 * state bridge.
 */
#define app_main prisma_runtime_main
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "main.c"
#pragma GCC diagnostic pop
#undef app_main

/*
 * Build-58-style outbound mirror, extended with source and page-4 effects.
 * Setters only bump generation when a value actually changes, so publishing
 * every 100 ms does not create artificial state changes.
 */
static void appliance_publish_task(void *arg)
{
    (void)arg;

    for (;;) {
        prisma_control_set_on(synth_on);
        prisma_control_set_mode(mode_index ? 1 : 0);
        prisma_control_set_intensity(intensity);
        prisma_control_set_filename(prisma_storage_current());

        for (int i = 0; i < EFFECT_COUNT; i++) {
            prisma_control_set_effect(i, effect_enabled[i]);
            prisma_control_set_effect_value(i, effect_value[i]);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    prisma_storage_init();

    /* Publish a complete snapshot before Wi-Fi/HTTP starts. */
    prisma_control_set_on(synth_on);
    prisma_control_set_mode(mode_index ? 1 : 0);
    prisma_control_set_intensity(intensity);
    prisma_control_set_filename(prisma_storage_current());
    for (int i = 0; i < EFFECT_COUNT; i++) {
        prisma_control_set_effect(i, effect_enabled[i]);
        prisma_control_set_effect_value(i, effect_value[i]);
    }

    ESP_LOGW(TAG, "M4.5 UNIFIED STATE / BUILD58 PUBLISHER ACTIVE");

    if (xTaskCreate(appliance_publish_task, "prisma_publish", 3072, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "M4.5 publisher task creation failed");
    }

    prisma_runtime_main();
}
