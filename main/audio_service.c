#include "audio_service.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

#define AUDIO_RATE 16000
#define AUDIO_BITS 16
#define AUDIO_PA GPIO_NUM_7
#define AUDIO_MCLK GPIO_NUM_8
#define AUDIO_BCLK GPIO_NUM_9
#define AUDIO_WS GPIO_NUM_10
#define AUDIO_DOUT GPIO_NUM_12
#define ES8311_PROBE_ADDR ((uint16_t)(ES8311_CODEC_DEFAULT_ADDR >> 1))
#define ES7210_PROBE_ADDR ((uint16_t)(ES7210_CODEC_DEFAULT_ADDR >> 1))
#define FILES_NS "files_cfg"
#define FILES_SAMPLE "sample"
#define AUDIO_NS "audio_cfg"
#define AUDIO_VOLUME "volume"
#define DEFAULT_VOLUME 70
#define PLAY_STACK 8192
#define SENSOR_TONE_MIN_HZ 80.0f
#define SENSOR_TONE_MAX_HZ 2000.0f

static const char *TAG="PRISMA_AUDIO";
static SemaphoreHandle_t s_lock;
static i2s_chan_handle_t s_tx;
static esp_codec_dev_handle_t s_output;
static prisma_audio_status_t s_status;
static volatile bool s_stop;
static TaskHandle_t s_play_task;
static volatile bool s_sensor_tone_stop;
static volatile bool s_sensor_tone_active;
static volatile float s_sensor_tone_hz=440.0f;
static volatile int s_sensor_tone_voices=1;
static TaskHandle_t s_sensor_tone_task;

typedef struct{uint16_t format,channels;uint32_t rate;uint16_t bits;uint32_t data_offset,data_bytes;}wav_info_t;
static uint16_t le16(const uint8_t*p){return(uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t*p){return(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void lock(void){if(s_lock)xSemaphoreTake(s_lock,portMAX_DELAY);}
static void unlock(void){if(s_lock)xSemaphoreGive(s_lock);}
static void set_error(const char*m){lock();strlcpy(s_status.last_error,m?m:"",sizeof(s_status.last_error));unlock();}
static void load_selected(char out[64]){out[0]=0;nvs_handle_t h;if(nvs_open(FILES_NS,NVS_READONLY,&h)!=ESP_OK)return;size_t n=64;if(nvs_get_str(h,FILES_SAMPLE,out,&n)!=ESP_OK)out[0]=0;nvs_close(h);}
static int load_volume(void){int32_t v=DEFAULT_VOLUME;nvs_handle_t h;if(nvs_open(AUDIO_NS,NVS_READONLY,&h)==ESP_OK){nvs_get_i32(h,AUDIO_VOLUME,&v);nvs_close(h);}return(v<0||v>100)?DEFAULT_VOLUME:(int)v;}
static esp_err_t save_volume(int v){nvs_handle_t h;esp_err_t e=nvs_open(AUDIO_NS,NVS_READWRITE,&h);if(e!=ESP_OK)return e;e=nvs_set_i32(h,AUDIO_VOLUME,v);if(e==ESP_OK)e=nvs_commit(h);nvs_close(h);return e;}

static bool parse_wav(FILE*f,wav_info_t*w){memset(w,0,sizeof(*w));uint8_t hdr[12];rewind(f);if(fread(hdr,1,12,f)!=12||memcmp(hdr,"RIFF",4)||memcmp(hdr+8,"WAVE",4))return false;bool hf=false,hd=false;while(!hf||!hd){uint8_t ch[8];if(fread(ch,1,8,f)!=8)break;uint32_t len=le32(ch+4);long payload=ftell(f);if(!memcmp(ch,"fmt ",4)){uint8_t fmt[40]={0};size_t take=len<sizeof(fmt)?len:sizeof(fmt);if(fread(fmt,1,take,f)!=take||take<16)return false;w->format=le16(fmt);w->channels=le16(fmt+2);w->rate=le32(fmt+4);w->bits=le16(fmt+14);hf=true;}else if(!memcmp(ch,"data",4)){w->data_offset=(uint32_t)payload;w->data_bytes=len;hd=true;}if(fseek(f,payload+(long)len+(len&1U),SEEK_SET)!=0)break;}return hf&&hd&&w->format==1&&(w->channels==1||w->channels==2)&&(w->bits==8||w->bits==16)&&w->rate>=8000&&w->rate<=48000;}
static int16_t sample_at(const uint8_t*p,int bits){return bits==8?(int16_t)(((int)p[0]-128)<<8):(int16_t)le16(p);}
static bool write_pcm(const int16_t*s,size_t n){return s_output&&n&&esp_codec_dev_write(s_output,(void*)s,(int)(n*sizeof(int16_t)))==ESP_CODEC_DEV_OK;}

static bool play_wav_file(const char*name){char path[128];snprintf(path,sizeof(path),"/storage/%s",name);FILE*f=fopen(path,"rb");if(!f){set_error("Selected WAV not found");return false;}wav_info_t w;if(!parse_wav(f,&w)){fclose(f);set_error("WAV must be PCM 8/16-bit mono/stereo 8-48 kHz");return false;}if(fseek(f,(long)w.data_offset,SEEK_SET)!=0){fclose(f);set_error("Cannot seek WAV data");return false;}int fb=(w.bits/8)*w.channels;uint32_t total=fb?w.data_bytes/(uint32_t)fb:0;lock();s_status.source_rate=(int)w.rate;s_status.source_channels=w.channels;s_status.source_bits=w.bits;s_status.duration_ms=w.rate?(int)(((uint64_t)total*1000)/w.rate):0;s_status.position_ms=0;unlock();uint8_t raw[1024];int16_t out[1024];uint64_t src_index=0,next_out_num=0;uint32_t left=w.data_bytes;bool ok=true;while(left&&!s_stop){size_t want=left<sizeof(raw)?left:sizeof(raw);want-=want%(size_t)fb;if(!want)break;size_t got=fread(raw,1,want,f);if(!got)break;got-=got%(size_t)fb;left-=(uint32_t)got;size_t frames=got/(size_t)fb;size_t on=0;for(size_t i=0;i<frames&&!s_stop;i++,src_index++){const uint8_t*p=raw+i*fb;int32_t v=sample_at(p,w.bits);if(w.channels==2)v=(v+sample_at(p+w.bits/8,w.bits))/2;uint64_t limit=(src_index+1ULL)*AUDIO_RATE;while(next_out_num<limit){out[on++]=(int16_t)v;next_out_num+=w.rate;if(on==sizeof(out)/sizeof(out[0])){if(!write_pcm(out,on)){ok=false;break;}on=0;}}if(!ok)break;}if(ok&&on&&!s_stop)ok=write_pcm(out,on);lock();s_status.position_ms=w.rate?(int)((src_index*1000ULL)/w.rate):0;unlock();if(!ok)break;}fclose(f);if(!ok)set_error("Audio codec write failed");return ok;}
static bool play_tone_internal(void){const int total=AUDIO_RATE/2;int16_t b[256];int done=0;while(done<total&&!s_stop){int n=total-done;if(n>256)n=256;for(int i=0;i<n;i++){float p=2.0f*3.14159265358979323846f*440.0f*(float)(done+i)/(float)AUDIO_RATE;b[i]=(int16_t)(sinf(p)*11000.0f);}if(!write_pcm(b,n)){set_error("Audio codec write failed");return false;}done+=n;lock();s_status.position_ms=done*1000/AUDIO_RATE;unlock();}return true;}
static void playback_task(void*arg){bool tone=(bool)(uintptr_t)arg;char name[64];lock();strlcpy(name,s_status.playing_name,sizeof(name));unlock();bool ok=tone?play_tone_internal():play_wav_file(name);int16_t z[64]={0};if(s_output)write_pcm(z,64);lock();if(ok&&!s_stop)s_status.position_ms=s_status.duration_ms;s_status.playing=false;s_status.tone=false;s_play_task=NULL;unlock();vTaskDelete(NULL);}

static void sensor_tone_task(void*arg){(void)arg;int16_t b[128];float phase=0.0f;const float two_pi=6.28318530717958647692f;const float ratios[5]={1.0f,1.5f,2.0f,3.0f,4.0f};while(!s_sensor_tone_stop){float hz=s_sensor_tone_hz;int voices=s_sensor_tone_voices;if(hz<SENSOR_TONE_MIN_HZ)hz=SENSOR_TONE_MIN_HZ;if(hz>SENSOR_TONE_MAX_HZ)hz=SENSOR_TONE_MAX_HZ;if(voices<1)voices=1;if(voices>5)voices=5;float step=two_pi*hz/(float)AUDIO_RATE;for(size_t i=0;i<sizeof(b)/sizeof(b[0]);i++){float mix=0.0f;for(int v=0;v<voices;v++)mix+=sinf(phase*ratios[v]);b[i]=(int16_t)(mix*(9000.0f/(float)voices));phase+=step;if(phase>=two_pi)phase-=two_pi;}if(!write_pcm(b,sizeof(b)/sizeof(b[0])))break;}int16_t z[64]={0};if(s_output)write_pcm(z,64);s_sensor_tone_active=false;s_sensor_tone_task=NULL;vTaskDelete(NULL);}

esp_err_t prisma_audio_init(i2c_master_bus_handle_t bus){memset(&s_status,0,sizeof(s_status));s_lock=xSemaphoreCreateMutex();if(!s_lock)return ESP_ERR_NO_MEM;s_status.volume=load_volume();s_status.saved_volume=s_status.volume;load_selected(s_status.selected_sample);if(!bus){set_error("Shared I2C bus unavailable");return ESP_ERR_INVALID_STATE;}s_status.es8311_present=i2c_master_probe(bus,ES8311_PROBE_ADDR,100)==ESP_OK;s_status.es7210_present=i2c_master_probe(bus,ES7210_PROBE_ADDR,100)==ESP_OK;if(!s_status.es8311_present){set_error("ES8311 speaker codec not detected");return ESP_ERR_NOT_FOUND;}i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);cc.auto_clear=true;esp_err_t e=i2s_new_channel(&cc,&s_tx,NULL);if(e!=ESP_OK)return e;i2s_std_config_t sc={.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),.slot_cfg=I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO),.gpio_cfg={.mclk=AUDIO_MCLK,.bclk=AUDIO_BCLK,.ws=AUDIO_WS,.dout=AUDIO_DOUT,.din=I2S_GPIO_UNUSED}};sc.clk_cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;if((e=i2s_channel_init_std_mode(s_tx,&sc))!=ESP_OK||(e=i2s_channel_enable(s_tx))!=ESP_OK)return e;audio_codec_i2s_cfg_t dc={.port=I2S_NUM_0,.rx_handle=NULL,.tx_handle=s_tx};const audio_codec_data_if_t*di=audio_codec_new_i2s_data(&dc);audio_codec_i2c_cfg_t ic={.addr=ES8311_CODEC_DEFAULT_ADDR,.bus_handle=bus};const audio_codec_ctrl_if_t*ci=audio_codec_new_i2c_ctrl(&ic);const audio_codec_gpio_if_t*gi=audio_codec_new_gpio();if(!di||!ci||!gi)return ESP_FAIL;es8311_codec_cfg_t c={0};c.ctrl_if=ci;c.gpio_if=gi;c.codec_mode=ESP_CODEC_DEV_WORK_MODE_DAC;c.pa_pin=AUDIO_PA;c.master_mode=false;c.use_mclk=true;c.hw_gain.pa_voltage=5.0;c.hw_gain.codec_dac_voltage=3.3;const audio_codec_if_t*co=es8311_codec_new(&c);if(!co)return ESP_FAIL;esp_codec_dev_cfg_t d={.dev_type=ESP_CODEC_DEV_TYPE_OUT,.codec_if=co,.data_if=di};s_output=esp_codec_dev_new(&d);if(!s_output)return ESP_FAIL;esp_codec_dev_sample_info_t fs={.bits_per_sample=AUDIO_BITS,.channel=1,.sample_rate=AUDIO_RATE};if(esp_codec_dev_open(s_output,&fs)!=ESP_CODEC_DEV_OK||esp_codec_dev_set_out_vol(s_output,s_status.volume)!=ESP_CODEC_DEV_OK)return ESP_FAIL;lock();s_status.initialized=true;s_status.last_error[0]=0;unlock();ESP_LOGI(TAG,"AUDIO READY rate=%d volume=%d",AUDIO_RATE,s_status.volume);return ESP_OK;}
void prisma_audio_get_status(prisma_audio_status_t*out){if(!out)return;lock();load_selected(s_status.selected_sample);*out=s_status;unlock();}
esp_err_t prisma_audio_preview_volume(int v){if(v<0||v>100)return ESP_ERR_INVALID_ARG;if(!s_output)return ESP_ERR_INVALID_STATE;if(esp_codec_dev_set_out_vol(s_output,v)!=ESP_CODEC_DEV_OK)return ESP_FAIL;lock();s_status.volume=v;unlock();return ESP_OK;}
esp_err_t prisma_audio_set_volume(int v){esp_err_t e=prisma_audio_preview_volume(v);if(e!=ESP_OK)return e;if((e=save_volume(v))!=ESP_OK)return e;lock();s_status.saved_volume=v;unlock();return ESP_OK;}
static esp_err_t start_play(bool tone){if(!s_status.initialized||!s_output||s_sensor_tone_active)return ESP_ERR_INVALID_STATE;lock();if(s_status.playing){unlock();return ESP_ERR_INVALID_STATE;}if(!tone){load_selected(s_status.selected_sample);if(!s_status.selected_sample[0]){unlock();return ESP_ERR_NOT_FOUND;}strlcpy(s_status.playing_name,s_status.selected_sample,sizeof(s_status.playing_name));}else strlcpy(s_status.playing_name,"440 HZ TEST",sizeof(s_status.playing_name));s_status.playing=true;s_status.tone=tone;s_status.position_ms=0;s_status.duration_ms=tone?500:0;s_status.source_rate=tone?AUDIO_RATE:0;s_status.source_channels=tone?1:0;s_status.source_bits=tone?16:0;s_status.last_error[0]=0;s_stop=false;unlock();if(xTaskCreate(playback_task,"audio_play",PLAY_STACK,(void*)(uintptr_t)tone,5,&s_play_task)!=pdPASS){lock();s_status.playing=false;s_status.tone=false;s_play_task=NULL;unlock();return ESP_ERR_NO_MEM;}return ESP_OK;}
esp_err_t prisma_audio_play_selected(void){return start_play(false);}esp_err_t prisma_audio_play_tone(void){return start_play(true);}
esp_err_t prisma_audio_sensor_tone_start(float hz){if(!s_status.initialized||!s_output||s_status.playing)return ESP_ERR_INVALID_STATE;if(hz<SENSOR_TONE_MIN_HZ)hz=SENSOR_TONE_MIN_HZ;if(hz>SENSOR_TONE_MAX_HZ)hz=SENSOR_TONE_MAX_HZ;s_sensor_tone_hz=hz;if(s_sensor_tone_active)return ESP_OK;s_sensor_tone_stop=false;s_sensor_tone_active=true;if(xTaskCreate(sensor_tone_task,"sensor_tone",4096,NULL,5,&s_sensor_tone_task)!=pdPASS){s_sensor_tone_active=false;s_sensor_tone_task=NULL;return ESP_ERR_NO_MEM;}return ESP_OK;}
esp_err_t prisma_audio_sensor_tone_set_frequency(float hz){if(hz<SENSOR_TONE_MIN_HZ||hz>SENSOR_TONE_MAX_HZ)return ESP_ERR_INVALID_ARG;s_sensor_tone_hz=hz;return s_sensor_tone_active?ESP_OK:ESP_ERR_INVALID_STATE;}
esp_err_t prisma_audio_sensor_tone_set_voices(int voices){if(voices<1||voices>5)return ESP_ERR_INVALID_ARG;s_sensor_tone_voices=voices;return ESP_OK;}
int prisma_audio_sensor_tone_voices(void){return s_sensor_tone_voices;}
void prisma_audio_sensor_tone_stop(void){s_sensor_tone_stop=true;}
bool prisma_audio_sensor_tone_active(void){return s_sensor_tone_active;}
float prisma_audio_sensor_tone_frequency(void){return s_sensor_tone_hz;}
void prisma_audio_stop(void){s_stop=true;s_sensor_tone_stop=true;}
