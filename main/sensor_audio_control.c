#include "sensor_audio_control.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_service.h"
#include "prisma_sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static SemaphoreHandle_t s_lock;
static prisma_sensor_audio_state_t s={.enabled=false,.active=false,.face_down=false,.speaker_enabled=true,.volume=100,.voices=1,.pitch_hz=440.0f,.root=9,.scale=PRISMA_SCALE_CHROMATIC,.waveform=PRISMA_WAVE_SINE,.midi_note=69,.note_name="A4"};
static TaskHandle_t s_task;
static const char *NOTE[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char *SCALE_NAMES[PRISMA_SCALE_COUNT]={"CHROMATIC","MAJOR","MINOR","DIMINISHED","DORIAN","WHOLE TONE"};
static const char *WAVE_NAMES[PRISMA_WAVE_COUNT]={"SINE","SQUARE","TRIANGLE","SAW","PULSE"};
static const int MAJOR[]={0,2,4,5,7,9,11};
static const int MINOR[]={0,2,3,5,7,8,10};
static const int DIMINISHED[]={0,2,3,5,6,8,9,11};
static const int DORIAN[]={0,2,3,5,7,9,10};
static const int WHOLE[]={0,2,4,6,8,10};
const char *prisma_sensor_audio_root_name(int root){return NOTE[((root%12)+12)%12];}
const char *prisma_sensor_audio_scale_name(prisma_scale_t scale){return(scale>=0&&scale<PRISMA_SCALE_COUNT)?SCALE_NAMES[scale]:"CHROMATIC";}
const char *prisma_sensor_audio_waveform_name(prisma_waveform_t wave){return(wave>=0&&wave<PRISMA_WAVE_COUNT)?WAVE_NAMES[wave]:"SINE";}
static bool in_set(int rel,const int*a,int n){for(int i=0;i<n;i++)if(a[i]==rel)return true;return false;}
static bool allowed_pc(int pc,int root,prisma_scale_t scale){int rel=(pc-root+12)%12;switch(scale){case PRISMA_SCALE_CHROMATIC:return true;case PRISMA_SCALE_MAJOR:return in_set(rel,MAJOR,7);case PRISMA_SCALE_MINOR:return in_set(rel,MINOR,7);case PRISMA_SCALE_DIMINISHED:return in_set(rel,DIMINISHED,8);case PRISMA_SCALE_DORIAN:return in_set(rel,DORIAN,7);case PRISMA_SCALE_WHOLE_TONE:return in_set(rel,WHOLE,6);default:return true;}}
static float midi_hz(int n){return 440.0f*powf(2.0f,((float)n-69.0f)/12.0f);}
/* Zero X is always the selected ROOT in octave 4. The +/-90 degree travel spans
   one octave below/above that reference and is quantized to the selected scale. */
static int quantized_note_from_x(float x,int root,prisma_scale_t scale){if(x<-90)x=-90;if(x>90)x=90;int center=60+((root%12)+12)%12;float pos=(x+90.0f)/180.0f;int target=(center-12)+(int)lroundf(pos*24.0f),best=center,bestd=999;for(int n=center-12;n<=center+12;n++){if(!allowed_pc(n%12,root,scale))continue;int d=abs(n-target);if(d<bestd||(d==bestd&&abs(n-center)<abs(best-center))){best=n;bestd=d;}}return best;}
static void note_label(int midi,char out[8]){int pc=((midi%12)+12)%12,oct=midi/12-1;snprintf(out,8,"%s%d",NOTE[pc],oct);}
static int voices_from_y(float y,int current){static const float up[4]={-61.5f,-16.5f,28.5f,67.5f};static const float down[4]={-67.5f,-22.5f,22.5f,61.5f};if(current<1||current>5)current=1;while(current<5&&y>up[current-1])current++;while(current>1&&y<down[current-2])current--;return current;}
static void ctl_task(void *arg){(void)arg;while(1){prisma_imu_data_t d;if(prisma_sensors_imu_ready()&&prisma_sensors_read_imu(&d)){bool enabled,face;int voices,root;prisma_scale_t scale;prisma_waveform_t wave;xSemaphoreTake(s_lock,portMAX_DELAY);root=s.root;scale=s.scale;wave=s.waveform;xSemaphoreGive(s_lock);int midi=quantized_note_from_x(d.angle_x,root,scale);float hz=midi_hz(midi);char name[8];note_label(midi,name);xSemaphoreTake(s_lock,portMAX_DELAY);if(!s.face_down&&d.angle_z<20)s.face_down=true;else if(s.face_down&&d.angle_z>35)s.face_down=false;s.voices=voices_from_y(d.angle_y,s.voices);s.angle_x=d.angle_x;s.angle_y=d.angle_y;s.angle_z=d.angle_z;s.pitch_hz=hz;s.midi_note=midi;strlcpy(s.note_name,name,sizeof(s.note_name));enabled=s.enabled;face=s.face_down;voices=s.voices;xSemaphoreGive(s_lock);prisma_audio_sensor_tone_set_voices(voices);prisma_audio_sensor_tone_set_waveform((int)wave);if(enabled&&!face){if(!prisma_audio_sensor_tone_active())prisma_audio_sensor_tone_start(hz);else prisma_audio_sensor_tone_set_frequency(hz);}else if(prisma_audio_sensor_tone_active())prisma_audio_sensor_tone_stop();xSemaphoreTake(s_lock,portMAX_DELAY);s.active=prisma_audio_sensor_tone_active()&&enabled&&!face;xSemaphoreGive(s_lock);}vTaskDelay(pdMS_TO_TICKS(50));}}
esp_err_t prisma_sensor_audio_start(void){if(s_task)return ESP_OK;if(!s_lock)s_lock=xSemaphoreCreateMutex();if(!s_lock)return ESP_ERR_NO_MEM;prisma_audio_set_volume(100);prisma_audio_sensor_tone_set_voices(1);prisma_audio_sensor_tone_set_waveform(0);prisma_audio_set_speaker_enabled(true);s.volume=100;s.voices=1;s.speaker_enabled=true;if(xTaskCreate(ctl_task,"sensor_audio",4096,NULL,5,&s_task)!=pdPASS){s_task=NULL;return ESP_ERR_NO_MEM;}return ESP_OK;}
esp_err_t prisma_sensor_audio_set_enabled(bool enabled){if(!s_lock)return ESP_ERR_INVALID_STATE;xSemaphoreTake(s_lock,portMAX_DELAY);s.enabled=enabled;xSemaphoreGive(s_lock);if(!enabled)prisma_audio_sensor_tone_stop();return ESP_OK;}
esp_err_t prisma_sensor_audio_set_volume(int volume){if(volume<0||volume>100)return ESP_ERR_INVALID_ARG;esp_err_t e=prisma_audio_set_volume(volume);if(e==ESP_OK){xSemaphoreTake(s_lock,portMAX_DELAY);s.volume=volume;xSemaphoreGive(s_lock);}return e;}
esp_err_t prisma_sensor_audio_set_root(int root){if(root<0||root>11)return ESP_ERR_INVALID_ARG;if(!s_lock)return ESP_ERR_INVALID_STATE;xSemaphoreTake(s_lock,portMAX_DELAY);s.root=root;xSemaphoreGive(s_lock);return ESP_OK;}
esp_err_t prisma_sensor_audio_set_scale(prisma_scale_t scale){if(scale<0||scale>=PRISMA_SCALE_COUNT)return ESP_ERR_INVALID_ARG;if(!s_lock)return ESP_ERR_INVALID_STATE;xSemaphoreTake(s_lock,portMAX_DELAY);s.scale=scale;xSemaphoreGive(s_lock);return ESP_OK;}
esp_err_t prisma_sensor_audio_set_waveform(prisma_waveform_t wave){if(wave<0||wave>=PRISMA_WAVE_COUNT)return ESP_ERR_INVALID_ARG;if(!s_lock)return ESP_ERR_INVALID_STATE;prisma_audio_sensor_tone_set_waveform((int)wave);xSemaphoreTake(s_lock,portMAX_DELAY);s.waveform=wave;xSemaphoreGive(s_lock);return ESP_OK;}
esp_err_t prisma_sensor_audio_set_speaker(bool enabled){if(!s_lock)return ESP_ERR_INVALID_STATE;esp_err_t e=prisma_audio_set_speaker_enabled(enabled);if(e==ESP_OK){xSemaphoreTake(s_lock,portMAX_DELAY);s.speaker_enabled=enabled;xSemaphoreGive(s_lock);}return e;}
void prisma_sensor_audio_get_state(prisma_sensor_audio_state_t *out){if(!out)return;if(!s_lock){*out=s;return;}xSemaphoreTake(s_lock,portMAX_DELAY);*out=s;xSemaphoreGive(s_lock);}
