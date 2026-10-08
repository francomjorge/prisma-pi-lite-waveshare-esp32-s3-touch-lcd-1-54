#include "waveshare_bsp.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#define PRISMA_LCD_H_RES              240
#define PRISMA_LCD_V_RES              240
#define PRISMA_LCD_SPI_HOST           SPI3_HOST
#define PRISMA_LCD_PIXEL_CLK_HZ       (40 * 1000 * 1000)
#define PRISMA_LCD_DRAW_BUFFER_HEIGHT 50

#define PRISMA_PIN_BAT_POWER GPIO_NUM_2
#define PRISMA_PIN_LCD_SCLK  GPIO_NUM_38
#define PRISMA_PIN_LCD_MOSI  GPIO_NUM_39
#define PRISMA_PIN_LCD_RST   GPIO_NUM_40
#define PRISMA_PIN_LCD_DC    GPIO_NUM_45
#define PRISMA_PIN_LCD_CS    GPIO_NUM_21
#define PRISMA_PIN_LCD_BL    GPIO_NUM_46

#define PRISMA_TOUCH_I2C_PORT 0
#define PRISMA_PIN_TOUCH_SCL  GPIO_NUM_41
#define PRISMA_PIN_TOUCH_SDA  GPIO_NUM_42
#define PRISMA_PIN_TOUCH_INT  GPIO_NUM_48
#define PRISMA_PIN_TOUCH_RST  GPIO_NUM_47

static const char *TAG = "waveshare_bsp";

static esp_lcd_panel_io_handle_t s_lcd_io;
static esp_lcd_panel_handle_t s_lcd_panel;
static esp_lcd_touch_handle_t s_touch;
static i2c_master_bus_handle_t s_i2c_bus;
static lv_display_t *s_lvgl_display;
static bool s_touch_ready;

esp_err_t waveshare_bsp_power_hold_init(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PRISMA_PIN_BAT_POWER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "power hold GPIO config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(PRISMA_PIN_BAT_POWER, 1), TAG, "power hold assert failed");
    ESP_LOGI(TAG, "power hold asserted on GPIO%d", PRISMA_PIN_BAT_POWER);
    return ESP_OK;
}

static esp_err_t lcd_init(void)
{
    const gpio_config_t backlight = {
        .pin_bit_mask = 1ULL << PRISMA_PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&backlight), TAG, "backlight GPIO config failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(PRISMA_PIN_LCD_BL, 0), TAG, "backlight off failed");

    const spi_bus_config_t bus_cfg = {
        .sclk_io_num = PRISMA_PIN_LCD_SCLK,
        .mosi_io_num = PRISMA_PIN_LCD_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = PRISMA_LCD_H_RES * PRISMA_LCD_DRAW_BUFFER_HEIGHT * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(PRISMA_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "SPI init failed");

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PRISMA_PIN_LCD_DC,
        .cs_gpio_num = PRISMA_PIN_LCD_CS,
        .pclk_hz = PRISMA_LCD_PIXEL_CLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)PRISMA_LCD_SPI_HOST, &io_cfg, &s_lcd_io),
        TAG,
        "LCD panel IO failed");

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PRISMA_PIN_LCD_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_lcd_io, &panel_cfg, &s_lcd_panel), TAG, "ST7789 init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_lcd_panel), TAG, "LCD reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_lcd_panel), TAG, "LCD panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_lcd_panel, true), TAG, "LCD invert failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_lcd_panel, true), TAG, "LCD display-on failed");
    return ESP_OK;
}

static esp_err_t touch_init(void)
{
    const i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = PRISMA_TOUCH_I2C_PORT,
        .scl_io_num = PRISMA_PIN_TOUCH_SCL,
        .sda_io_num = PRISMA_PIN_TOUCH_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "touch I2C init failed");

    esp_lcd_panel_io_handle_t touch_io = NULL;
    const esp_lcd_panel_io_i2c_config_t touch_io_cfg = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .flags.disable_control_phase = 1,
        .scl_speed_hz = 400 * 1000,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &touch_io_cfg, &touch_io), TAG, "touch IO init failed");

    const esp_lcd_touch_config_t touch_cfg = {
        .x_max = PRISMA_LCD_H_RES,
        .y_max = PRISMA_LCD_V_RES,
        .rst_gpio_num = PRISMA_PIN_TOUCH_RST,
        .int_gpio_num = PRISMA_PIN_TOUCH_INT,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_cst816s(touch_io, &touch_cfg, &s_touch), TAG, "CST816S init failed");
    s_touch_ready = true;
    return ESP_OK;
}

static esp_err_t lvgl_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 10 * 1024,
        .task_affinity = -1,
        .task_max_sleep_ms = 100,
        .timer_period_ms = 5,
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port init failed");

    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = s_lcd_io,
        .panel_handle = s_lcd_panel,
        .buffer_size = PRISMA_LCD_H_RES * PRISMA_LCD_DRAW_BUFFER_HEIGHT,
        .double_buffer = true,
        .hres = PRISMA_LCD_H_RES,
        .vres = PRISMA_LCD_V_RES,
        .monochrome = false,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
        },
    };

    s_lvgl_display = lvgl_port_add_disp(&display_cfg);
    ESP_RETURN_ON_FALSE(s_lvgl_display != NULL, ESP_FAIL, TAG, "LVGL display registration failed");

    if (s_touch_ready) {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = s_lvgl_display,
            .handle = s_touch,
        };
        ESP_RETURN_ON_FALSE(lvgl_port_add_touch(&touch_cfg) != NULL, ESP_FAIL, TAG, "LVGL touch registration failed");
    }

    return ESP_OK;
}

esp_err_t waveshare_bsp_display_init(void)
{
    ESP_RETURN_ON_ERROR(lcd_init(), TAG, "LCD initialization failed");

    const esp_err_t touch_status = touch_init();
    if (touch_status != ESP_OK) {
        s_touch_ready = false;
        ESP_LOGW(TAG, "touch unavailable; continuing with display only: %s", esp_err_to_name(touch_status));
    }

    ESP_RETURN_ON_ERROR(lvgl_init(), TAG, "LVGL initialization failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(PRISMA_PIN_LCD_BL, 1), TAG, "backlight on failed");

    ESP_LOGI(TAG, "LCD ready: ST7789 240x240, touch=%s", s_touch_ready ? "CST816S" : "unavailable");
    return ESP_OK;
}

int waveshare_bsp_display_width(void)
{
    return PRISMA_LCD_H_RES;
}

int waveshare_bsp_display_height(void)
{
    return PRISMA_LCD_V_RES;
}

bool waveshare_bsp_touch_available(void)
{
    return s_touch_ready;
}

lv_display_t *waveshare_bsp_lvgl_display(void)
{
    return s_lvgl_display;
}

void waveshare_bsp_lock(void)
{
    lvgl_port_lock(0);
}

void waveshare_bsp_unlock(void)
{
    lvgl_port_unlock();
}
