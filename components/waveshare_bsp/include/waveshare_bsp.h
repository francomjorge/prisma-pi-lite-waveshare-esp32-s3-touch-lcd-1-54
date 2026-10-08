#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t waveshare_bsp_power_hold_init(void);
esp_err_t waveshare_bsp_display_init(void);

int waveshare_bsp_display_width(void);
int waveshare_bsp_display_height(void);
bool waveshare_bsp_touch_available(void);

lv_display_t *waveshare_bsp_lvgl_display(void);
void waveshare_bsp_lock(void);
void waveshare_bsp_unlock(void);

#ifdef __cplusplus
}
#endif
