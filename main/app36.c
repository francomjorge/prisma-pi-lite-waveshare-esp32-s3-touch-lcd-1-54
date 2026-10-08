#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.h"
#define synth_on prisma_shared_on
#define mode_index prisma_shared_mode
#define intensity prisma_shared_intensity
#define app_main prisma_legacy_main
#include "main.c"
#undef app_main
#undef intensity
#undef mode_index
#undef synth_on

/* LITE: appliance is authoritative. Web is read-only. */
static void mirror_task(void *arg){
 for(;;){
  prisma_control_set_on(prisma_shared_on);
  prisma_control_set_mode(prisma_shared_mode ? 1 : 0);
  prisma_control_set_intensity(prisma_shared_intensity);
  vTaskDelay(pdMS_TO_TICKS(100));
 }
}
void app_main(void){
 prisma_shared_mode=0;
 prisma_shared_intensity=61;
 prisma_control_set_on(prisma_shared_on);
 prisma_control_set_mode(prisma_shared_mode);
 prisma_control_set_intensity(prisma_shared_intensity);
 prisma_storage_init();
 prisma_control_set_filename(prisma_storage_current());
 xTaskCreate(mirror_task,"prisma_mirror",3072,NULL,4,NULL);
 prisma_legacy_main();
}
