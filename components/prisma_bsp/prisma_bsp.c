#include "prisma_bsp.h"

#include <stddef.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"

#define PRISMA_LCD_H_RES 240
#define PRISMA_LCD_V_RES 240
#define PRISMA_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define PRISMA_LCD_SPI_HOST SPI2_HOST

/* Verified against Waveshare ESP32-S3-Touch-LCD-1.54 factory BSP. */
#define PRISMA_PIN_LCD_MOSI GPIO_NUM_39
#define PRISMA_PIN_LCD_SCLK GPIO_NUM_38
#define PRISMA_PIN_LCD_CS   GPIO_NUM_21
#define PRISMA_PIN_LCD_DC   GPIO_NUM_45
#define PRISMA_PIN_LCD_RST  GPIO_NUM_40
#define PRISMA_PIN_LCD_BL   GPIO_NUM_46

#define PRISMA_PIN_I2C_SDA  GPIO_NUM_42
#define PRISMA_PIN_I2C_SCL  GPIO_NUM_41
#define PRISMA_PIN_TOUCH_INT GPIO_NUM_48
#define PRISMA_PIN_TOUCH_RST GPIO_NUM_47

#define PRISMA_I2C_PORT 0

#define PRISMA_BL_TIMER LEDC_TIMER_0
#define PRISMA_BL_MODE LEDC_LOW_SPEED_MODE
#define PRISMA_BL_CHANNEL LEDC_CHANNEL_0
#define PRISMA_BL_DUTY_RES LEDC_TIMER_10_BIT
#define PRISMA_BL_MAX_DUTY 1023U
#define PRISMA_BL_FREQUENCY_HZ 5000

#define PRISMA_FILL_ROWS 20

static const char *TAG = "prisma_bsp";

static esp_lcd_panel_io_handle_t s_lcd_io = NULL;
static esp_lcd_panel_handle_t s_lcd_panel = NULL;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;
static prisma_bsp_status_t s_status = {0};
static bool s_initialized = false;

/* Internal DRAM is kept alive for queued SPI DMA transfers. */
static DMA_ATTR uint16_t s_fill_buffer[PRISMA_LCD_H_RES * PRISMA_FILL_ROWS];

static bool init_backlight(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = PRISMA_BL_MODE,
        .duty_resolution = PRISMA_BL_DUTY_RES,
        .timer_num = PRISMA_BL_TIMER,
        .freq_hz = PRISMA_BL_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Backlight timer init failed: %s", esp_err_to_name(err));
        return false;
    }

    ledc_channel_config_t channel = {
        .gpio_num = PRISMA_PIN_LCD_BL,
        .speed_mode = PRISMA_BL_MODE,
        .channel = PRISMA_BL_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PRISMA_BL_TIMER,
        .duty = 0,
        .hpoint = 0,
    };

    err = ledc_channel_config(&channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Backlight channel init failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

static bool init_display(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PRISMA_PIN_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PRISMA_PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PRISMA_LCD_H_RES * PRISMA_FILL_ROWS * sizeof(uint16_t),
    };

    esp_err_t err = spi_bus_initialize(PRISMA_LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD SPI bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PRISMA_PIN_LCD_CS,
        .dc_gpio_num = PRISMA_PIN_LCD_DC,
        .spi_mode = 3,
        .pclk_hz = PRISMA_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)PRISMA_LCD_SPI_HOST, &io_cfg, &s_lcd_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD panel IO init failed: %s", esp_err_to_name(err));
        return false;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PRISMA_PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    err = esp_lcd_new_panel_st7789(s_lcd_io, &panel_cfg, &s_lcd_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 create failed: %s", esp_err_to_name(err));
        return false;
    }

    if ((err = esp_lcd_panel_reset(s_lcd_panel)) != ESP_OK ||
        (err = esp_lcd_panel_init(s_lcd_panel)) != ESP_OK ||
        (err = esp_lcd_panel_invert_color(s_lcd_panel, true)) != ESP_OK ||
        (err = esp_lcd_panel_disp_on_off(s_lcd_panel, true)) != ESP_OK) {
        ESP_LOGE(TAG, "ST7789 init failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

static bool init_i2c(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = PRISMA_I2C_PORT,
        .sda_io_num = PRISMA_PIN_I2C_SDA,
        .scl_io_num = PRISMA_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

static bool init_touch(void)
{
    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags.disable_control_phase = 1,
        .scl_speed_hz = 400 * 1000,
    };

    esp_err_t err = esp_lcd_new_panel_io_i2c(s_i2c_bus, &io_cfg, &touch_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Touch panel IO init failed: %s", esp_err_to_name(err));
        return false;
    }

    esp_lcd_touch_config_t touch_cfg = {
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

    err = esp_lcd_touch_new_i2c_cst816s(touch_io, &touch_cfg, &s_touch);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CST816S init failed: %s", esp_err_to_name(err));
        return false;
    }

    return true;
}

bool prisma_bsp_init(void)
{
    if (s_initialized) {
        return s_status.display_ready && s_status.touch_ready;
    }
    s_initialized = true;

    ESP_LOGI(TAG, "M1 board bring-up: Waveshare ESP32-S3-Touch-LCD-1.54");

    if (!init_backlight()) {
        return false;
    }

    s_status.display_ready = init_display();
    if (!s_status.display_ready) {
        return false;
    }

    if (!init_i2c()) {
        return false;
    }

    s_status.touch_ready = init_touch();

    prisma_bsp_set_brightness(80);

    ESP_LOGI(TAG, "Display: %s", s_status.display_ready ? "ready" : "failed");
    ESP_LOGI(TAG, "Touch: %s", s_status.touch_ready ? "ready" : "failed");

    return s_status.display_ready && s_status.touch_ready;
}

prisma_bsp_status_t prisma_bsp_get_status(void)
{
    return s_status;
}

void prisma_bsp_set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }

    uint32_t duty = ((uint32_t)percent * PRISMA_BL_MAX_DUTY) / 100U;
    esp_err_t err = ledc_set_duty(PRISMA_BL_MODE, PRISMA_BL_CHANNEL, duty);
    if (err == ESP_OK) {
        err = ledc_update_duty(PRISMA_BL_MODE, PRISMA_BL_CHANNEL);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Backlight update failed: %s", esp_err_to_name(err));
        return;
    }

    s_status.brightness_percent = percent;
}

bool prisma_bsp_display_fill(uint16_t rgb565)
{
    if (!s_status.display_ready || s_lcd_panel == NULL) {
        return false;
    }

    for (size_t i = 0; i < (sizeof(s_fill_buffer) / sizeof(s_fill_buffer[0])); ++i) {
        s_fill_buffer[i] = rgb565;
    }

    for (int y = 0; y < PRISMA_LCD_V_RES; y += PRISMA_FILL_ROWS) {
        int y_end = y + PRISMA_FILL_ROWS;
        if (y_end > PRISMA_LCD_V_RES) {
            y_end = PRISMA_LCD_V_RES;
        }

        esp_err_t err = esp_lcd_panel_draw_bitmap(
            s_lcd_panel,
            0,
            y,
            PRISMA_LCD_H_RES,
            y_end,
            s_fill_buffer);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD fill failed at y=%d: %s", y, esp_err_to_name(err));
            return false;
        }
    }

    return true;
}

bool prisma_bsp_touch_read(prisma_bsp_touch_sample_t *sample)
{
    if (sample == NULL) {
        return false;
    }

    sample->pressed = false;
    sample->x = 0;
    sample->y = 0;

    if (!s_status.touch_ready || s_touch == NULL) {
        return false;
    }

    esp_err_t err = esp_lcd_touch_read_data(s_touch);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Touch read failed: %s", esp_err_to_name(err));
        return false;
    }

    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t count = 0;

    bool pressed = esp_lcd_touch_get_coordinates(s_touch, &x, &y, NULL, &count, 1);
    if (pressed && count > 0) {
        sample->pressed = true;
        sample->x = x;
        sample->y = y;
    }

    return true;
}
