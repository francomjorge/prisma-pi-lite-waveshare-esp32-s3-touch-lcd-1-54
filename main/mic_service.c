#include "mic_service.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "audio_service.h"
#include "driver/i2s_tdm.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "storage.h"

#define MIC_RATE 16000
#define MIC_BITS 16
#define MIC_MCLK GPIO_NUM_8
#define MIC_BCLK GPIO_NUM_9
#define MIC_WS GPIO_NUM_10
#define MIC_DIN GPIO_NUM_11
#define MIC_GAIN_DB 30
#define MIC_TASK_STACK 6144
#define MIC_CHUNK_SAMPLES 256
#define RECORD_NAME "mic_record.wav"
#define FILES_NS "media_cfg"
#define FILES_SAMPLE "sample"
#define MAX_RECORD_SECONDS 10

static const char *TAG="PRISMA_MIC";
static SemaphoreHandle_t s_lock;static i2s_chan_handle_t s_rx;static esp_codec_dev_handle_t s_input;static prisma_mic_status_t s_status;static TaskHandle_t s_task;static FILE*s_record_file;static uint32_t s_record_bytes,s_record_target_bytes;static volatile bool s_record_stop;static volatile bool s_capture_on;
static void lock(void){if(s_lock)xSemaphoreTake(s_lock,portMAX_DELAY);}static void unlock(void){if(s_lock)xSemaphoreGive(s_lock);}static void set_mic_error(const char*m){lock();strlcpy(s_status.last_error,m?m:"",sizeof(s_status.last_error));unlock();}
static void put16(uint8_t*p,uint16_t v){p[0]=v&255;p[1]=v>>8;}static void put32(uint8_t*p,uint32_t v){p[0]=v&255;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void wav_header(uint8_t h[44],uint32_t n){memset(h,0,44);memcpy(h,"RIFF",4);put32(h+4,36+n);memcpy(h+8,"WAVEfmt ",8);put32(h+16,16);put16(h+20,1);put16(h+22,1);put32(h+24,MIC_RATE);put32(h+28,MIC_RATE*2);put16(h+32,2);put16(h+34,MIC_BITS);memcpy(h+36,"data",4);put32(h+40,n);}
static esp_err_t save_selected(const char*n){nvs_handle_t h;esp_err_t e=nvs_open(FILES_NS,NVS_READWRITE,&h);if(e!=ESP_OK)return e;e=nvs_set_str(h,FILES_SAMPLE,n);if(e==ESP_OK)e=nvs_commit(h);nvs_close(h);return e;}

static esp_err_t capture_off(void){
 if(!s_capture_on)return ESP_OK;
 esp_err_t e=i2s_channel_disable(s_rx);if(e==ESP_ERR_INVALID_STATE)e=ESP_OK;
 s_capture_on=false;lock();s_status.level=0;s_status.peak=0;unlock();
 esp_err_t a=prisma_audio_resume_after_mic();if(e==ESP_OK)e=a;
 ESP_LOGI(TAG,"MIC OFF; ES8311 restored");return e;
}
static esp_err_t capture_on(void){
 if(s_capture_on)return ESP_OK;
 esp_err_t e=prisma_audio_suspend_for_mic();if(e!=ESP_OK)return e;
 e=i2s_channel_enable(s_rx);if(e!=ESP_OK){prisma_audio_resume_after_mic();return e;}
 s_capture_on=true;ESP_LOGI(TAG,"MIC ON; ES7210 capture window");return ESP_OK;
}

/* Kept for API compatibility; idle policy is now explicit ownership. */
esp_err_t prisma_mic_pause_for_playback(void){if(s_status.recording)return ESP_ERR_INVALID_STATE;return capture_off();}
esp_err_t prisma_mic_resume_after_playback(void){return ESP_ERR_INVALID_STATE;}

static void finish_recording(bool keep,const char*error){
 FILE*f;uint32_t bytes;lock();f=s_record_file;bytes=s_record_bytes;s_record_file=NULL;s_status.recording=false;s_status.record_ms=(int)(((uint64_t)bytes*1000ULL)/(MIC_RATE*2ULL));if(error)strlcpy(s_status.last_error,error,sizeof(s_status.last_error));unlock();
 if(f){bool ok=keep&&bytes>0;if(ok){uint8_t h[44];wav_header(h,bytes);if(fflush(f)!=0||fseek(f,0,SEEK_SET)!=0||fwrite(h,1,44,f)!=44)ok=false;}fclose(f);if(!ok){remove("/storage/" RECORD_NAME);if(!error)set_mic_error("Recording finalize failed");}else if(save_selected(RECORD_NAME)!=ESP_OK)set_mic_error("Recorded WAV saved but sample selection failed");else{lock();strlcpy(s_status.record_name,RECORD_NAME,sizeof(s_status.record_name));if(!error)s_status.last_error[0]=0;unlock();ESP_LOGI(TAG,"Recording saved: %s bytes=%u",RECORD_NAME,(unsigned)bytes);}}
 esp_err_t e=capture_off();if(e!=ESP_OK)set_mic_error("Speaker restore after recording failed");
}

static void mic_task(void*arg){(void)arg;int16_t samples[MIC_CHUNK_SAMPLES];for(;;){if(!s_capture_on||!s_status.recording){vTaskDelay(pdMS_TO_TICKS(20));continue;}int rc=esp_codec_dev_read(s_input,samples,sizeof(samples));if(rc!=ESP_CODEC_DEV_OK){set_mic_error("Microphone read failed");vTaskDelay(pdMS_TO_TICKS(40));continue;}uint32_t peak=0;for(size_t i=0;i<MIC_CHUNK_SAMPLES;i++){int32_t v=samples[i];uint32_t a=(uint32_t)(v<0?-v:v);if(a>peak)peak=a;}if(peak>32767)peak=32767;int raw=(int)(peak*100U/32767U);FILE*f;uint32_t bytes,target;bool rec,stop;lock();int prev=s_status.level;s_status.peak=raw;s_status.level=raw>=prev?raw:(prev*3+raw)/4;rec=s_status.recording;f=s_record_file;bytes=s_record_bytes;target=s_record_target_bytes;stop=s_record_stop;unlock();if(!rec||!f)continue;size_t chunk=sizeof(samples);if(bytes+chunk>target)chunk=target-bytes;if(chunk&&fwrite(samples,1,chunk,f)!=chunk){finish_recording(false,"Storage write failed");continue;}bytes+=(uint32_t)chunk;lock();s_record_bytes=bytes;s_status.record_ms=(int)(((uint64_t)bytes*1000ULL)/(MIC_RATE*2ULL));stop=s_record_stop;unlock();if(stop||bytes>=target)finish_recording(true,NULL);}}

esp_err_t prisma_mic_init(i2c_master_bus_handle_t bus){
 memset(&s_status,0,sizeof(s_status));s_status.gain_db=MIC_GAIN_DB;strlcpy(s_status.record_name,RECORD_NAME,sizeof(s_status.record_name));s_lock=xSemaphoreCreateMutex();if(!s_lock)return ESP_ERR_NO_MEM;if(!bus){set_mic_error("Shared I2C bus unavailable");return ESP_ERR_INVALID_STATE;}if(i2c_master_probe(bus,(uint16_t)(ES7210_CODEC_DEFAULT_ADDR>>1),100)!=ESP_OK){set_mic_error("ES7210 microphone ADC not detected");return ESP_ERR_NOT_FOUND;}
 i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);cc.auto_clear=true;esp_err_t e=i2s_new_channel(&cc,NULL,&s_rx);if(e!=ESP_OK){set_mic_error("I2S RX channel unavailable");return e;}i2s_tdm_config_t tc={.clk_cfg={.sample_rate_hz=MIC_RATE,.clk_src=I2S_CLK_SRC_DEFAULT,.ext_clk_freq_hz=0,.mclk_multiple=I2S_MCLK_MULTIPLE_256,.bclk_div=8},.slot_cfg={.data_bit_width=I2S_DATA_BIT_WIDTH_16BIT,.slot_bit_width=I2S_SLOT_BIT_WIDTH_AUTO,.slot_mode=I2S_SLOT_MODE_STEREO,.slot_mask=I2S_TDM_SLOT0|I2S_TDM_SLOT1|I2S_TDM_SLOT2|I2S_TDM_SLOT3,.ws_width=I2S_TDM_AUTO_WS_WIDTH,.ws_pol=false,.bit_shift=true,.left_align=false,.big_endian=false,.bit_order_lsb=false,.skip_mask=false,.total_slot=I2S_TDM_AUTO_SLOT_NUM},.gpio_cfg={.mclk=MIC_MCLK,.bclk=MIC_BCLK,.ws=MIC_WS,.dout=I2S_GPIO_UNUSED,.din=MIC_DIN,.invert_flags={.mclk_inv=false,.bclk_inv=false,.ws_inv=false}}};e=i2s_channel_init_tdm_mode(s_rx,&tc);if(e!=ESP_OK){set_mic_error("I2S microphone TDM init failed");return e;}
 audio_codec_i2s_cfg_t dc={.port=I2S_NUM_0,.rx_handle=s_rx,.tx_handle=NULL};const audio_codec_data_if_t*data=audio_codec_new_i2s_data(&dc);audio_codec_i2c_cfg_t ic={.addr=ES7210_CODEC_DEFAULT_ADDR,.bus_handle=bus};const audio_codec_ctrl_if_t*ctrl=audio_codec_new_i2c_ctrl(&ic);if(!data||!ctrl){set_mic_error("Microphone interface failed");return ESP_FAIL;}es7210_codec_cfg_t cfg={0};cfg.ctrl_if=ctrl;cfg.mic_selected=ES7210_SEL_MIC1|ES7210_SEL_MIC2|ES7210_SEL_MIC3|ES7210_SEL_MIC4;const audio_codec_if_t*codec=es7210_codec_new(&cfg);if(!codec){set_mic_error("ES7210 codec creation failed");return ESP_FAIL;}esp_codec_dev_cfg_t dev={.dev_type=ESP_CODEC_DEV_TYPE_IN,.codec_if=codec,.data_if=data};s_input=esp_codec_dev_new(&dev);if(!s_input){set_mic_error("Microphone device creation failed");return ESP_FAIL;}esp_codec_dev_sample_info_t fs={.bits_per_sample=MIC_BITS,.channel=4,.channel_mask=ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),.sample_rate=MIC_RATE,.mclk_multiple=0};if(esp_codec_dev_open(s_input,&fs)!=ESP_CODEC_DEV_OK){set_mic_error("ES7210 open failed");return ESP_FAIL;}if(esp_codec_dev_set_in_channel_gain(s_input,ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),MIC_GAIN_DB)!=ESP_CODEC_DEV_OK){set_mic_error("Microphone gain init failed");return ESP_FAIL;}
 /* RX was configured but never enabled: microphone clocks are OFF at idle. */
 s_capture_on=false;lock();s_status.initialized=true;s_status.last_error[0]=0;unlock();if(xTaskCreate(mic_task,"mic_record",MIC_TASK_STACK,NULL,4,&s_task)!=pdPASS){set_mic_error("Microphone task failed");return ESP_ERR_NO_MEM;}ESP_LOGI(TAG,"MIC READY/OFF ES7210 rate=%d gain=%d",MIC_RATE,MIC_GAIN_DB);return ESP_OK;
}
void prisma_mic_get_status(prisma_mic_status_t*out){if(!out)return;lock();*out=s_status;unlock();}
esp_err_t prisma_mic_record_start(int seconds){
 if(!s_status.initialized||!s_input)return ESP_ERR_INVALID_STATE;if(seconds<1||seconds>MAX_RECORD_SECONDS)return ESP_ERR_INVALID_ARG;prisma_audio_status_t a;prisma_audio_get_status(&a);if(a.playing)return ESP_ERR_INVALID_STATE;lock();bool busy=s_status.recording;unlock();if(busy)return ESP_ERR_INVALID_STATE;if(!prisma_storage_ready())return ESP_ERR_INVALID_STATE;struct stat st;bool replacing=stat("/storage/" RECORD_NAME,&st)==0&&S_ISREG(st.st_mode);if(!replacing&&prisma_storage_file_count()>=prisma_storage_max_files()){set_mic_error("FILES+ is full (4 files)");return ESP_ERR_NO_MEM;}size_t total=0,used=0;uint32_t target=(uint32_t)seconds*MIC_RATE*2U;if(!prisma_storage_info(&total,&used)||total<=used||(total-used)<(size_t)target+8192U){set_mic_error("Not enough storage for recording");return ESP_ERR_NO_MEM;}FILE*f=fopen("/storage/" RECORD_NAME,"wb");if(!f){set_mic_error("Cannot create recording file");return ESP_FAIL;}uint8_t h[44];wav_header(h,0);if(fwrite(h,1,44,f)!=44){fclose(f);remove("/storage/" RECORD_NAME);return ESP_FAIL;}
 esp_err_t e=capture_on();if(e!=ESP_OK){fclose(f);remove("/storage/" RECORD_NAME);set_mic_error("Cannot acquire I2S0 for microphone");return e;}lock();s_record_file=f;s_record_bytes=0;s_record_target_bytes=target;s_record_stop=false;s_status.recording=true;s_status.record_ms=0;s_status.record_target_ms=seconds*1000;s_status.last_error[0]=0;unlock();ESP_LOGI(TAG,"Recording started: %d sec",seconds);return ESP_OK;
}
void prisma_mic_record_stop(void){s_record_stop=true;}
