#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.h"

#define synth_on prisma_shared_on
#define mode_index prisma_shared_mode
#define intensity prisma_shared_intensity
#define app_main prisma_legacy_main

/*
 * M4.2i was validated with its compact legacy source formatting.
 * Keep behavior byte-for-byte at source level during consolidation and
 * suppress only the GCC formatting diagnostic introduced by the newer CI.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "main.c"
#pragma GCC diagnostic pop

#undef app_main
#undef intensity
#undef mode_index
#undef synth_on

/* LITE: appliance is authoritative. Web is read-only. */
static void mirror_task(void *arg)
{
    for (;;) {
        prisma_control_set_on(prisma_shared_on);
        prisma_control_set_mode(prisma_shared_mode ? 1 : 0);
        prisma_control_set_intensity(prisma_shared_intensity);
        for (int i = 0; i < EFFECT_COUNT; i++) {
            prisma_control_set_effect(i, effect_enabled[i]);
            prisma_control_set_effect_value(i, effect_value[i]);
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
    for (int i = 0; i < EFFECT_COUNT; i++) {
        prisma_control_set_effect(i, effect_enabled[i]);
        prisma_control_set_effect_value(i, effect_value[i]);
    }
    xTaskCreate(mirror_task, "prisma_mirror", 3072, NULL, 4, NULL);
    prisma_legacy_main();
}
