#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include "midi_service.h"
#include "prisma_power.h"
#include "prisma_touch.h"
#include "storage.h"
#include "wifi_portal.h"

#define LCD_HOST SPI2_HOST
#define W 240
#define H 240
#define LCD_PIN_SCLK 38
#define LCD_PIN_MOSI 39
#define LCD_PIN_CS 21
#define LCD_PIN_DC 45
#define LCD_PIN_RST 40
#define LCD_PIN_BL 46

#define PAGE_COUNT 2
#define SWIPE_THRESHOLD 55
#define HOME_COOLDOWN_TICKS 25

static const char *TAG = "FACTORY_V11";
static uint16_t frame[W * H];
static esp_lcd_panel_handle_t panel;
static int glitch_count = 0;

static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (c >> 8) | (c << 8);
}

static void rect(int x, int y, int w, int h, uint16_t c)
{
    for (int yy = y; yy < y + h && yy < H; yy++) {
        for (int xx = x; xx < x + w && xx < W; xx++) {
            if (xx >= 0 && yy >= 0) frame[yy * W + xx] = c;
        }
    }
}

static const uint8_t SP[5]={0},DA[5]={8,8,8,8,8},DT[5]={0,0x60,0x60,0,0},D0[5]={0x3e,0x51,0x49,0x45,0x3e},D1[5]={0,0x42,0x7f,0x40,0},D2[5]={0x42,0x61,0x51,0x49,0x46},D3[5]={0x21,0x41,0x45,0x4b,0x31},D4[5]={0x18,0x14,0x12,0x7f,0x10},D5[5]={0x27,0x45,0x45,0x45,0x39},D6[5]={0x3c,0x4a,0x49,0x49,0x30},D7[5]={1,0x71,9,5,3},D8[5]={0x36,0x49,0x49,0x49,0x36},D9[5]={6,0x49,0x49,0x29,0x1e};
static const uint8_t A[5]={0x7e,0x11,0x11,0x11,0x7e},B[5]={0x7f,0x49,0x49,0x49,0x36},C[5]={0x3e,0x41,0x41,0x41,0x22},D[5]={0x7f,0x41,0x41,0x22,0x1c},E[5]={0x7f,0x49,0x49,0x49,0x41},F[5]={0x7f,9,9,9,1},G[5]={0x3e,0x41,0x49,0x49,0x7a},Hh[5]={0x7f,8,8,8,0x7f},I[5]={0,0x41,0x7f,0x41,0},J[5]={0x20,0x40,0x41,0x3f,0x01},K[5]={0x7f,8,0x14,0x22,0x41},L[5]={0x7f,0x40,0x40,0x40,0x40},M[5]={0x7f,2,0x0c,2,0x7f},N[5]={0x7f,4,8,0x10,0x7f},O[5]={0x3e,0x41,0x41,0x41,0x3e},P[5]={0x7f,0x09,0x09,0x09,0x06},R[5]={0x7f,0x09,0x19,0x29,0x46},S[5]={0x46,0x49,0x49,0x49,0x31},T[5]={1,1,0x7f,1,1},U[5]={0x3f,0x40,0x40,0x40,0x3f},V[5]={0x1f,0x20,0x40,0x20,0x1f},Ww[5]={0x3f,0x40,0x38,0x40,0x3f},X[5]={0x63,0x14,0x08,0x14,0x63},Y[5]={7,8,0x70,8,7},Z[5]={0x61,0x51,0x49,0x45,0x43};

static const uint8_t *glyph(char c)
{
    if (c >= '0' && c <= '9') {
        static const uint8_t *q[] = {D0,D1,D2,D3,D4,D5,D6,D7,D8,D9};
        return q[c - '0'];
    }
    switch (c) {
        case '-': return DA; case '.': return DT;
        case 'A': return A; case 'B': return B; case 'C': return C; case 'D': return D;
        case 'E': return E; case 'F': return F; case 'G': return G; case 'H': return Hh;
        case 'I': return I; case 'J': return J; case 'K': return K; case 'L': return L;
        case 'M': return M; case 'N': return N; case 'O': return O; case 'P': return P;
        case 'R': return R; case 'S': return S; case 'T': return T; case 'U': return U;
        case 'V': return V; case 'W': return Ww; case 'X': return X; case 'Y': return Y;
        case 'Z': return Z; default: return SP;
    }
}

static void text(int x, int y, const char *s, int z, uint16_t c)
{
    while (*s) {
        const uint8_t *p = glyph(*s++);
        for (int a = 0; a < 5; a++) {
            for (int b = 0; b < 7; b++) {
                if (p[a] & (1 << b)) rect(x + a * z, y + b * z, z, z, c);
            }
        }
        x += 6 * z;
    }
}

static int text_width(const char *s, int z) { return (int)strlen(s) * 6 * z - z; }
static void text_center(int y, const char *s, int z, uint16_t c) { text((W - text_width(s, z)) / 2, y, s, z, c); }
static void clear(uint16_t c) { for (int i = 0; i < W * H; i++) frame[i] = c; }
static void show(void) { ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, W, H, frame)); }

static void page_dots(int page, uint16_t active, uint16_t dim)
{
    const int dot_w = 8;
    const int gap = 8;
    const int total_w = PAGE_COUNT * dot_w + (PAGE_COUNT - 1) * gap;
    const int start_x = (W - total_w) / 2;
    for (int i = 0; i < PAGE_COUNT; i++) rect(start_x + i * (dot_w + gap), 224, dot_w, dot_w, i == page ? active : dim);
}

static void logo_title(int y, int phase)
{
    uint16_t wh = rgb(240,245,250);
    uint16_t pal[5] = {rgb(30,120,255),rgb(175,55,255),rgb(255,35,150),rgb(255,120,25),rgb(45,225,120)};
    int z = 4;
    const char *base = "PRISMA";
    const char *fx = "-PI";
    int x = (W - text_width("PRISMA-PI", z)) / 2;
    text(x, y, base, z, wh);
    int fx_x = x + 6 * z * 6;
    for (int i = 0; fx[i]; i++) {
        char one[2] = {fx[i], 0};
        text(fx_x + i * 6 * z, y, one, z, pal[(i + phase) % 5]);
    }
}

static void draw_home(int phase)
{
    uint16_t bg=rgb(4,7,12), wh=rgb(240,245,250), cy=rgb(35,220,220), dim=rgb(45,60,75);
    clear(bg);
    logo_title(42, phase);
    text_center(100, "LITE", 3, wh);
    text_center(151, "SYSTEM CORE", 2, wh);
    page_dots(0, cy, dim);
    show();
}

static void display_file_name(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0) return;
    size_t j = 0;
    if (!src) src = "";
    for (size_t i = 0; src[i] && j + 1 < dst_size && j < 24; i++) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-') dst[j++] = c;
        else if (c == '_' || c == ' ') dst[j++] = '-';
    }
    dst[j] = 0;
}

static void draw_midi(void)
{
    uint16_t bg=rgb(4,7,12), wh=rgb(240,245,250), cy=rgb(35,220,220), dim=rgb(45,60,75);
    uint16_t green=rgb(45,225,120), red=rgb(255,70,90), orange=rgb(255,150,30);
    prisma_midi_status_t s;
    prisma_midi_get_status(&s);

    clear(bg);
    text_center(18, "MIDI CTRL", 3, wh);
    text_center(49, "USB HOST", 1, s.host_ready ? cy : dim);

    rect(18, 67, 204, 48, rgb(12,19,28));
    if (s.midi_connected) {
        text_center(82, "CONNECTED", 2, green);
    } else if (s.usb_present) {
        text_center(82, "NOT MIDI", 2, orange);
    } else {
        text_center(82, "DISCONNECTED", 2, red);
    }

    text(24, 132, "SOUND", 2, wh);
    text(168, 132, s.play_sound ? "ON" : "OFF", 2, s.play_sound ? green : dim);

    text(24, 158, "FILE", 2, wh);
    char file_name[28];
    display_file_name(s.sound_file, file_name, sizeof(file_name));
    if (!file_name[0]) strlcpy(file_name, "NO FILE", sizeof(file_name));
    text_center(184, file_name, 1, s.selected_file_present ? cy : dim);

    page_dots(1, cy, dim);
    show();
}

static void glitch(uint16_t x, uint16_t y)
{
    uint16_t pal[5] = {rgb(30,120,255),rgb(175,55,255),rgb(255,35,150),rgb(255,120,25),rgb(45,225,120)};
    for (int i = 0; i < 9; i++) {
        int dx = ((i * 17 + glitch_count * 11) % 55) - 27;
        int dy = ((i * 23 + glitch_count * 7) % 55) - 27;
        int sz = 4 + ((i + glitch_count) % 4) * 3;
        rect((int)x + dx, (int)y + dy, sz, sz, pal[(i + glitch_count) % 5]);
    }
    glitch_count++;
    show();
}

static void draw_page(int page, int phase)
{
    if (page == 0) draw_home(phase);
    else draw_midi();
}

void app_main(void)
{
    prisma_power_init();
    prisma_storage_init();
    ESP_LOGI(TAG, "PRISMA-PI LITE FACTORY V1.1 MIDI");

    gpio_config_t bl = {.pin_bit_mask=1ULL<<LCD_PIN_BL,.mode=GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&bl));
    gpio_set_level(LCD_PIN_BL,0);

    spi_bus_config_t bus = {.sclk_io_num=LCD_PIN_SCLK,.mosi_io_num=LCD_PIN_MOSI,.miso_io_num=-1,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=W*40*2};
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST,&bus,SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t ioc = {.dc_gpio_num=LCD_PIN_DC,.cs_gpio_num=LCD_PIN_CS,.pclk_hz=40000000,.lcd_cmd_bits=8,.lcd_param_bits=8,.spi_mode=0,.trans_queue_depth=10};
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,&ioc,&io));
    esp_lcd_panel_dev_config_t pc = {.reset_gpio_num=LCD_PIN_RST,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,.bits_per_pixel=16};
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io,&pc,&panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));

    int phase = 0;
    draw_page(0, phase);
    gpio_set_level(LCD_PIN_BL,1);

    bool touch = prisma_touch_init();
    ESP_LOGI(TAG, "TOUCH: %s", touch ? "READY" : "FAILED");
    prisma_wifi_start();
    prisma_midi_init();

    int page = 0;
    uint16_t x=0,y=0,startx=0,starty=0;
    bool down=false;
    int miss=0,anim=0,home_cooldown=0,midi_poll=0;
    uint32_t midi_revision_seen=0;

    while (1) {
        if (home_cooldown > 0) home_cooldown--;
        bool touched = touch && prisma_touch_read(&x,&y);
        if (touched) {
            miss = 0;
            if (!down) {
                startx=x; starty=y; down=true;
            } else {
                int dx = (int)x - (int)startx;
                if (dx < -SWIPE_THRESHOLD) {
                    if (page < PAGE_COUNT-1) page++;
                    glitch_count=0;
                    draw_page(page,phase);
                    home_cooldown=HOME_COOLDOWN_TICKS;
                    down=false;
                } else if (dx > SWIPE_THRESHOLD) {
                    if (page > 0) page--;
                    glitch_count=0;
                    draw_page(page,phase);
                    home_cooldown=HOME_COOLDOWN_TICKS;
                    down=false;
                }
            }
        } else if (down && ++miss > 4) {
            if (page==0 && home_cooldown==0) glitch(startx,starty);
            down=false;
            miss=0;
        }

        if (page==0 && glitch_count==0 && ++anim>=6) {
            anim=0;
            phase=(phase+1)%5;
            draw_page(0,phase);
        }

        if (page==1 && ++midi_poll>=10) {
            midi_poll=0;
            prisma_midi_status_t s;
            prisma_midi_get_status(&s);
            if (s.revision != midi_revision_seen) {
                midi_revision_seen = s.revision;
                draw_midi();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
