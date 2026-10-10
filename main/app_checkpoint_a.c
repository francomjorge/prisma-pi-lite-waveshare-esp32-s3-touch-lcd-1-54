#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "storage.h"
#include "prisma_power.h"

#define synth_on prisma_shared_on
#define mode_index prisma_shared_mode
#define intensity prisma_shared_intensity
#define app_main prisma_original_main

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#include "main.c"
#pragma GCC diagnostic pop

#undef app_main
#undef intensity
#undef mode_index
#undef synth_on

#define CP_PAGE_COUNT 6
#define MOTION_COUNT 6
#define MOTION_BAR_X 110
#define MOTION_BAR_W 84
#define INTENSITY_BUTTON_X 12
#define INTENSITY_BUTTON_Y 108
#define INTENSITY_BUTTON_W 48
#define INTENSITY_BUTTON_H 28
#define INTENSITY_BUTTON_GAP 8

static const char *SYNC_TAG = "WEB_SYNC";
static bool motion_enabled[MOTION_COUNT] = {false,false,false,false,false,false};
static int motion_value[MOTION_COUNT] = {50,50,50,50,50,50};
static const char *motion_names[MOTION_COUNT] = {"PULSE","DRIFT","ROTATE","WAVE","SHAKE","FEEDBACK"};
static int intensity_button = 0;
static int intensity_level = 0;

typedef struct {
    bool fx_enabled[5];
    int fx_value[6];
    bool motion_enabled[MOTION_COUNT];
    int motion_value[MOTION_COUNT];
} cp_intensity_preset_t;

/*
 * Progressive V1 presets. State 0 is FREE and never changes user values.
 * States 1..9 map to 1G,1Y,1R,2G,2Y,2R,3G,3Y,3R.
 */
static const cp_intensity_preset_t intensity_presets[9] = {
    /* 1G */ {{false,false,false,true,false}, { 0, 6,15,12,50,45}, {true,false,false,false,false,false}, {12, 6, 0, 5, 0, 5}},
    /* 1Y */ {{false,false,true,true,false},  { 0,10,22,20,50,48}, {true,true,false,false,false,false},  {18,10, 0, 8, 0, 8}},
    /* 1R */ {{false,true,true,true,false},   { 0,16,28,30,50,52}, {true,true,false,true,false,false},   {24,14, 0,12, 0,10}},
    /* 2G */ {{false,true,true,true,false},   { 0,24,34,40,50,56}, {true,true,false,true,true,false},    {30,20, 0,18, 7,14}},
    /* 2Y */ {{false,true,true,true,false},   { 0,33,42,52,50,62}, {true,true,false,true,true,false},    {36,26, 0,25,10,20}},
    /* 2R */ {{true,true,true,true,true},     { 8,43,50,64,50,68}, {true,true,false,true,true,true},     {43,32, 0,32,14,28}},
    /* 3G */ {{false,true,true,true,true},    { 0,54,58,74,50,75}, {true,true,false,true,true,true},     {50,38, 0,42,20,38}},
    /* 3Y */ {{false,true,true,true,true},    { 0,68,68,85,50,84}, {true,true,false,true,true,true},     {60,48, 0,56,30,62}},
    /* 3R */ {{false,true,true,true,true},    { 0,86,80,96,50,95}, {true,true,false,true,true,true},     {74,62, 0,74,46,86}}
};

static int cp_intensity_state(void)
{
    if(intensity_button<=0)return 0;
    return ((intensity_button-1)*3)+intensity_level+1;
}

static void cp_apply_intensity_preset(int state)
{
    if(state<=0||state>9)return;
    const cp_intensity_preset_t *p=&intensity_presets[state-1];
    for(int i=0;i<5;i++){
        effect_enabled[i]=p->fx_enabled[i];
        effect_value[i]=p->fx_value[i];
        prisma_control_set_effect(i,effect_enabled[i]);
    }
    effect_enabled[5]=true;
    effect_value[5]=p->fx_value[5];
    prisma_control_set_effect(5,true);
    for(int i=0;i<MOTION_COUNT;i++){
        motion_enabled[i]=p->motion_enabled[i];
        motion_value[i]=p->motion_value[i];
    }
    ESP_LOGI(SYNC_TAG,"INTENSITY PRESET state=%d mix=%d glitch=%d shake=%d feedback=%d",state,effect_value[5],effect_value[1],motion_value[4],motion_value[5]);
}

static void cp_select_intensity_button(int button)
{
    if(button<=0){
        intensity_button=0;
        intensity_level=0;
    }else if(button==intensity_button){
        intensity_level=(intensity_level+1)%3;
    }else{
        intensity_button=button;
        intensity_level=0;
    }
    int state=cp_intensity_state();
    cp_apply_intensity_preset(state);
    ESP_LOGI(SYNC_TAG,"INTENSITY button=%d level=%d state=%d",intensity_button,intensity_level,state);
}

static void cp_reset_performance(void)
{
    prisma_shared_mode=1;
    prisma_shared_intensity=0;
    intensity_button=0;
    intensity_level=0;
    for(int i=0;i<5;i++){
        effect_enabled[i]=false;
        effect_value[i]=0;
        prisma_control_set_effect(i,false);
    }
    effect_enabled[5]=true;
    effect_value[5]=0;
    prisma_control_set_effect(5,true);
    for(int i=0;i<MOTION_COUNT;i++){
        motion_enabled[i]=false;
        motion_value[i]=0;
    }
    ESP_LOGI(SYNC_TAG,"PERFORMANCE RESET -> BYPASS intensity=0 static=OFF/0 motion=OFF/0 mix=0");
}

bool prisma_appliance_get_on(void){return prisma_shared_on;}
int prisma_appliance_get_mode(void){return prisma_shared_mode ? 1 : 0;}
int prisma_appliance_get_intensity(void){return cp_intensity_state();}
const char *prisma_appliance_get_filename(void){return prisma_storage_current();}
bool prisma_appliance_get_effect(int effect){if(effect<0||effect>=EFFECT_COUNT)return false;if(effect==5)return true;return effect_enabled[effect];}
int prisma_appliance_get_effect_value(int effect){if(effect<0||effect>=EFFECT_COUNT)return 0;return effect_value[effect];}
bool prisma_appliance_get_motion_effect(int effect){if(effect<0||effect>=MOTION_COUNT)return false;return motion_enabled[effect];}
int prisma_appliance_get_motion_value(int effect){if(effect<0||effect>=MOTION_COUNT)return 0;return motion_value[effect];}

static void cp_dots(int page, uint16_t cyan, uint16_t dim)
{
    int xs[6]={76,92,108,124,140,156};
    for(int i=0;i<CP_PAGE_COUNT;i++) rect(xs[i],224,8,8,i==page?cyan:dim);
}

static void cp_page_intro(int phase)
{
    uint16_t bg=rgb(4,7,12),wh=rgb(240,245,250),cy=rgb(35,220,220),dim=rgb(45,60,75);
    clear(bg);logo_title(42,phase);text_center(100,"LITE",3,wh);text_center(151,"VIDEO SYNTHESIZER",2,wh);cp_dots(0,cy,dim);show();
}

static void cp_page_controls(void)
{
    uint16_t bg=rgb(4,7,12),wh=rgb(240,245,250),cy=rgb(35,220,220),dim=rgb(45,60,75),disabled=rgb(75,82,90),blue=rgb(25,65,145),green=rgb(35,200,110),yellow=rgb(245,190,35),red=rgb(230,55,65);
    uint16_t fg=prisma_shared_on?wh:disabled,accent=prisma_shared_on?cy:disabled;clear(bg);
    text(12,10,"PRISMA-PI",2,wh);rect(174,7,54,28,prisma_shared_on?green:red);text_box_center(174,54,14,prisma_shared_on?"ON":"OFF",2,wh);text(12,47,"NOW PLAYING",1,accent);text(12,60,prisma_storage_current(),2,fg);text(12,91,"INTENSITY",2,fg);
    for(int i=0;i<4;i++){
        int x=INTENSITY_BUTTON_X+i*(INTENSITY_BUTTON_W+INTENSITY_BUTTON_GAP);
        uint16_t box=dim;
        if(i==intensity_button){
            if(i==0)box=cy;
            else if(intensity_level==0)box=green;
            else if(intensity_level==1)box=yellow;
            else box=red;
        }
        rect(x,INTENSITY_BUTTON_Y,INTENSITY_BUTTON_W,INTENSITY_BUTTON_H,box);
        char label[2]={(char)('0'+i),0};
        text_box_center(x,INTENSITY_BUTTON_W,INTENSITY_BUTTON_Y+7,label,2,wh);
    }
    text(12,151,"MODE",2,fg);rect(86,143,142,34,prisma_shared_on?blue:dim);text_box_center(86,142,154,prisma_shared_mode?"BYPASS":"RANDOM",2,fg);cp_dots(1,prisma_shared_on?cy:disabled,dim);show();
}

static void cp_page_monitor(void)
{
    uint16_t bg=rgb(4,7,12),wh=rgb(240,245,250),cy=rgb(35,220,220),dim=rgb(45,60,75);
    clear(bg);text_center(48,"VISUAL MONITOR",2,wh);rect(28,82,184,2,cy);text_center(106,"OPEN IN BROWSER",2,wh);text_center(143,"192.168.4.1",2,cy);cp_dots(2,cy,dim);show();
}

static void cp_page_effects(void)
{
    uint16_t bg=rgb(4,7,12),wh=rgb(240,245,250),cy=rgb(35,220,220),dim=rgb(45,60,75),disabled=rgb(75,82,90),green=rgb(35,200,110),red=rgb(230,55,65);clear(bg);
    for(int i=0;i<EFFECT_COUNT;i++){
        int y=FX_ROW_Y+i*FX_ROW_STEP;bool enabled=(i==5)?true:effect_enabled[i];uint16_t label=!prisma_shared_on?disabled:(enabled?wh:dim);text(FX_LABEL_X,y,effect_names[i],FX_TEXT_SCALE,label);
        if(i==4){int sy=y-2;rect(FX_BAR_X,sy,48,20,enabled?(prisma_shared_on?green:disabled):dim);text_box_center(FX_BAR_X,48,sy+7,"ON",1,wh);rect(FX_BAR_X+54,sy,48,20,!enabled?(prisma_shared_on?red:disabled):dim);text_box_center(FX_BAR_X+54,48,sy+7,"OFF",1,wh);}
        else{char value[4];snprintf(value,sizeof(value),"%d",effect_value[i]);rect(FX_BAR_X,y+3,FX_BAR_W,8,dim);int fill=FX_BAR_W*effect_value[i]/100;if(fill>0)rect(FX_BAR_X,y+3,fill,8,(prisma_shared_on&&enabled)?cy:disabled);text(FX_VALUE_X,y+3,value,1,label);}
    }
    cp_dots(3,prisma_shared_on?cy:disabled,dim);show();
}

static void cp_page_fx_lab(void)
{
    uint16_t bg=rgb(4,7,12),wh=rgb(240,245,250),cy=rgb(35,220,220),dim=rgb(45,60,75);
    clear(bg);text_center(48,"FX LAB",3,wh);rect(28,82,184,2,cy);text_center(106,"OPEN IN BROWSER",2,wh);text_center(143,"192.168.4.1",2,cy);cp_dots(4,cy,dim);show();
}

static void cp_page_motion(void)
{
    uint16_t bg=rgb(4,7,12),wh=rgb(240,245,250),cy=rgb(35,220,220),dim=rgb(45,60,75),disabled=rgb(75,82,90);
    clear(bg);
    for(int i=0;i<MOTION_COUNT;i++){
        int y=FX_ROW_Y+i*FX_ROW_STEP;bool enabled=motion_enabled[i];uint16_t label=!prisma_shared_on?disabled:(enabled?wh:dim);
        text(FX_LABEL_X,y,motion_names[i],FX_TEXT_SCALE,label);
        char value[4];snprintf(value,sizeof(value),"%d",motion_value[i]);
        rect(MOTION_BAR_X,y+3,MOTION_BAR_W,8,dim);int fill=MOTION_BAR_W*motion_value[i]/100;
        if(fill>0)rect(MOTION_BAR_X,y+3,fill,8,(prisma_shared_on&&enabled)?cy:disabled);
        text(FX_VALUE_X,y+3,value,1,label);
    }
    cp_dots(5,prisma_shared_on?cy:disabled,dim);show();
}

static void cp_draw_page(int page,int phase)
{
    if(page==0)cp_page_intro(phase);
    else if(page==1)cp_page_controls();
    else if(page==2)cp_page_monitor();
    else if(page==3)cp_page_effects();
    else if(page==4)cp_page_fx_lab();
    else cp_page_motion();
}

static bool cp_page_needs_input_debounce(int page){return page==1||page==3||page==5;}

void app_main(void)
{
    prisma_shared_mode=0;prisma_shared_intensity=61;
    prisma_power_init();prisma_storage_init();
    power_hold();ESP_LOGI(SYNC_TAG,"PRISMA-PI LITE CHECKPOINT A - SIX APPLIANCE PAGES + MOTION STATE");
    gpio_config_t bl={.pin_bit_mask=1ULL<<LCD_PIN_BL,.mode=GPIO_MODE_OUTPUT};ESP_ERROR_CHECK(gpio_config(&bl));gpio_set_level(LCD_PIN_BL,0);
    spi_bus_config_t bus={.sclk_io_num=LCD_PIN_SCLK,.mosi_io_num=LCD_PIN_MOSI,.miso_io_num=-1,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=W*40*2};ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST,&bus,SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_handle_t io=NULL;esp_lcd_panel_io_spi_config_t ioc={.dc_gpio_num=LCD_PIN_DC,.cs_gpio_num=LCD_PIN_CS,.pclk_hz=40000000,.lcd_cmd_bits=8,.lcd_param_bits=8,.spi_mode=0,.trans_queue_depth=10};ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,&ioc,&io));
    esp_lcd_panel_dev_config_t pc={.reset_gpio_num=LCD_PIN_RST,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,.bits_per_pixel=16};ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io,&pc,&panel));ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,true));ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
    int phase=0;cp_draw_page(0,phase);gpio_set_level(LCD_PIN_BL,1);
    bool touch=prisma_touch_init();ESP_LOGI(SYNC_TAG,"TOUCH: %s",touch?"READY":"FAILED");prisma_wifi_start();for(int i=0;i<EFFECT_COUNT;i++)prisma_control_set_effect(i,effect_enabled[i]);
    int page=0;uint16_t x=0,y=0,startx=0,starty=0;bool down=false;bool input_lock=false;int input_lock_ticks=0;int miss=0,anim=0,home_cooldown=0;
    while(1){
        if(home_cooldown>0)home_cooldown--;
        bool touched=touch&&prisma_touch_read(&x,&y);
        if(input_lock){down=false;miss=0;if(touched)input_lock_ticks=CONTROL_DEBOUNCE_TICKS;else if(input_lock_ticks>0)input_lock_ticks--;else input_lock=false;}
        else if(touched){miss=0;if(!down){startx=x;starty=y;down=true;}else if((int)startx-(int)x>55&&page<CP_PAGE_COUNT-1){page++;glitch_count=0;cp_draw_page(page,phase);down=false;if(cp_page_needs_input_debounce(page)){input_lock=true;input_lock_ticks=CONTROL_DEBOUNCE_TICKS;}}else if((int)x-(int)startx>55&&page>0){page--;glitch_count=0;if(page==0)home_cooldown=HOME_COOLDOWN_TICKS;cp_draw_page(page,phase);down=false;if(cp_page_needs_input_debounce(page)){input_lock=true;input_lock_ticks=CONTROL_DEBOUNCE_TICKS;}}}
        else if(down&&++miss>4){
            if(page==0&&home_cooldown==0)glitch(startx,starty);
            else if(page==1&&startx>=170&&startx<=232&&starty>=5&&starty<=42){bool was_on=prisma_shared_on;toggle_power();if(!was_on&&prisma_shared_on)cp_reset_performance();cp_draw_page(1,phase);}
            else if(page==1&&startx>=12&&startx<=165&&starty>=45&&starty<=85){const char *src=prisma_storage_next();prisma_control_set_filename(src);cp_draw_page(1,phase);ESP_LOGI(SYNC_TAG,"SOURCE -> %s",src);}
            else if(page==1&&starty>=INTENSITY_BUTTON_Y-4&&starty<=INTENSITY_BUTTON_Y+INTENSITY_BUTTON_H+4&&startx>=INTENSITY_BUTTON_X&&startx<=228){int rel=(int)startx-INTENSITY_BUTTON_X;int stride=INTENSITY_BUTTON_W+INTENSITY_BUTTON_GAP;int button=rel/stride;int inside=rel%stride;if(button>=0&&button<4&&inside<INTENSITY_BUTTON_W){cp_select_intensity_button(button);cp_draw_page(1,phase);}}
            else if(page==1&&prisma_shared_on&&startx>=82&&startx<=232&&starty>=139&&starty<=181){prisma_shared_mode=prisma_shared_mode?0:1;cp_draw_page(1,phase);}
            else if(page==3&&starty>=FX_ROW_Y-6&&starty<FX_ROW_Y-6+EFFECT_COUNT*FX_ROW_STEP){int effect=((int)starty-(FX_ROW_Y-6))/FX_ROW_STEP;if(effect>=0&&effect<EFFECT_COUNT){if(effect==4){if(startx>=FX_BAR_X&&startx<FX_BAR_X+51){effect_enabled[4]=true;prisma_control_set_effect(4,true);}else if(startx>=FX_BAR_X+51&&startx<=FX_BAR_X+FX_BAR_W){effect_enabled[4]=false;prisma_control_set_effect(4,false);}}else if(startx<FX_BAR_X){if(effect<EFFECT_COUNT-1){effect_enabled[effect]=!effect_enabled[effect];prisma_control_set_effect(effect,effect_enabled[effect]);}else{effect_enabled[5]=true;prisma_control_set_effect(5,true);}}else if(prisma_shared_on&&startx<=FX_BAR_X+FX_BAR_W){int value=((int)startx-FX_BAR_X)*100/FX_BAR_W;if(value<0)value=0;if(value>100)value=100;effect_value[effect]=value;}cp_draw_page(3,phase);}}
            else if(page==5&&starty>=FX_ROW_Y-6&&starty<FX_ROW_Y-6+MOTION_COUNT*FX_ROW_STEP){int motion=((int)starty-(FX_ROW_Y-6))/FX_ROW_STEP;if(motion>=0&&motion<MOTION_COUNT){if(startx<MOTION_BAR_X)motion_enabled[motion]=!motion_enabled[motion];else if(prisma_shared_on&&startx<=MOTION_BAR_X+MOTION_BAR_W){int value=((int)startx-MOTION_BAR_X)*100/MOTION_BAR_W;if(value<0)value=0;if(value>100)value=100;motion_value[motion]=value;}ESP_LOGI(SYNC_TAG,"MOTION %s -> %s %d",motion_names[motion],motion_enabled[motion]?"ON":"OFF",motion_value[motion]);cp_draw_page(5,phase);}}
            down=false;miss=0;
        }
        if(page==0&&glitch_count==0&&++anim>=6){anim=0;phase=(phase+1)%5;cp_draw_page(0,phase);}vTaskDelay(pdMS_TO_TICKS(20));
    }
}
