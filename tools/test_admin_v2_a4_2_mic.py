#!/usr/bin/env python3
from pathlib import Path
root=Path(__file__).resolve().parents[1]
cmake=(root/'main/CMakeLists.txt').read_text();touch=(root/'main/prisma_touch.c').read_text();mic=(root/'main/mic_service.c').read_text();audio=(root/'main/audio_service.c').read_text();api=(root/'main/mic_api.c').read_text();page=(root/'main/web/admin.html').read_text();workflow=(root/'.github/workflows/build.yml').read_text()
mc=''.join(mic.split());ac=''.join(audio.split())
assert '"mic_service.c"' in cmake and '"mic_api.c"' in cmake
assert touch.index('prisma_mic_init(bus)') < touch.index('prisma_audio_init(bus)')
assert '#defineMIC_MCLKGPIO_NUM_8' in mc and '#defineMIC_BCLKGPIO_NUM_9' in mc and '#defineMIC_WSGPIO_NUM_10' in mc and '#defineMIC_DINGPIO_NUM_11' in mc
assert 'ES7210_CODEC_DEFAULT_ADDR' in mc and 'I2S_TDM_SLOT0|I2S_TDM_SLOT1|I2S_TDM_SLOT2|I2S_TDM_SLOT3' in mc
assert 's_capture_on=false' in mc
assert 'prisma_audio_suspend_for_mic()' in mc and 'prisma_audio_resume_after_mic()' in mc
assert 'i2s_channel_enable(s_rx)' in mc and 'i2s_channel_disable(s_rx)' in mc
assert 'i2s_channel_disable(s_tx)' in ac and 'i2s_channel_enable(s_tx)' in ac
assert 'i2s_channel_reconfig_std_clock' in ac and 'i2s_channel_reconfig_std_slot' in ac and 'i2s_channel_reconfig_std_gpio' in ac
assert 'prisma_audio_suspend_for_mic' in audio and 'prisma_audio_resume_after_mic' in audio
assert '"/api/mic/status"' in api and '"/api/mic/record"' in api and '"/api/mic/stop"' in api
assert 'FACTORY AUDIO V1' in page and 'RECORD 5 SEC' in page and 'PLAY RECORDING' in page
assert '/api/audio/play' in page and '/api/mic/record' in page
assert 'factory-audio-v1' in workflow and 'test_admin_v2_a4_2_mic.py' in workflow
print('Factory Audio V1.4 OK: MIC idle-off and explicit I2S0 speaker/microphone ownership handoff present')
