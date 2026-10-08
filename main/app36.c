#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "storage.h"

#define synth_on prisma_shared_on
#define mode_index prisma_shared_mode
#define intensity prisma_shared_intensity
#define app_main prisma_legacy_main

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "main.c"
#pragma GCC diagnostic pop

#undef app_main
#undef intensity
#undef mode_index
#undef synth_on

static const char *SYNC_TAG = "BUILD58_SYNC";

/* Build 58 pattern: appliance variables are the authority and are mirrored
 * into the HTTP state object every 100 ms. No web-to-appliance control path. */
static void mirror_task(void *arg)
{
    bool last_on = !prisma_shared_on;
    int last_mode = -1;
    int last_intensity = -1;

    for (;;) {
        prisma_control_set_on(prisma_shared_on);
        prisma_control_set_mode(prisma_shared_mode ? 1 : 0);
        prisma_control_set_intensity(prisma_shared_intensity);

        if (last_on != prisma_shared_on ||
            last_mode != prisma_shared_mode ||
            last_intensity != prisma_shared_intensity) {
            ESP_LOGI(SYNC_TAG,
                     "MIRROR TX on=%s intensity=%d mode=%s",
                     prisma_shared_on ? "ON" : "OFF",
                     prisma_shared_intensity,
                     prisma_shared_mode ? "BYPASS" : "RANDOM");
            last_on = prisma_shared_on;
            last_mode = prisma_shared_mode;
            last_intensity = prisma_shared_intensity;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    prisma_shared_mode = 0;
    prisma_shared_intensity = 61;

    prisma_control_set_on(prisma_shared_on);
    prisma_control_set_mode(prisma_shared_mode);
    prisma_control_set_intensity(prisma_shared_intensity);

    prisma_storage_init();
    prisma_control_set_filename(prisma_storage_current());

    ESP_LOGI(SYNC_TAG, "BUILD58 READ-ONLY PAGE2 SYNC ACTIVE");
    xTaskCreate(mirror_task, "prisma_mirror", 3072, NULL, 4, NULL);
    prisma_legacy_main();
}
