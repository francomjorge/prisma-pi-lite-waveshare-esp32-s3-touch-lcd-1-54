#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmake = (root / "main/CMakeLists.txt").read_text()
touch = (root / "main/prisma_touch.c").read_text()
portal = (root / "main/wifi_portal_factory.c").read_text()
mic = (root / "main/mic_service.c").read_text()
api = (root / "main/mic_api.c").read_text()
page = (root / "main/web/admin.html").read_text()
css = (root / "main/web/admin.css").read_text()
app = (root / "main/app_factory_v1.c").read_text()
workflow = (root / ".github/workflows/build.yml").read_text()
mic_c = ''.join(mic.split())

assert '"mic_service.c"' in cmake and '"mic_api.c"' in cmake
assert '"web/admin.html"' in cmake and '"web/admin.css"' in cmake
assert '"web/mic.html"' not in cmake and '"web/mic.css"' not in cmake
diag_audio_only = 'MIC init intentionally skipped in Factory Audio V1.1 diagnostic' in touch
if not diag_audio_only:
    assert 'prisma_mic_init(bus)' in touch
    assert touch.index('prisma_audio_init(bus)') < touch.index('prisma_mic_init(bus)')
else:
    assert 'prisma_audio_init(bus)' in touch
assert 'prisma_mic_api_register(server);' in portal
assert '(mic: /mic)' not in portal

# Hardware-path assertions are whitespace-insensitive because V1.2 deliberately
# compacted mic_service while adding the I2S coexistence state machine.
assert '#defineMIC_MCLKGPIO_NUM_8' in mic_c
assert '#defineMIC_BCLKGPIO_NUM_9' in mic_c
assert '#defineMIC_WSGPIO_NUM_10' in mic_c
assert '#defineMIC_DINGPIO_NUM_11' in mic_c
assert 'ES7210_CODEC_DEFAULT_ADDR' in mic_c
assert 'ES7210_SEL_MIC1|ES7210_SEL_MIC2|ES7210_SEL_MIC3|ES7210_SEL_MIC4' in mic_c
assert 'ES7120_SEL_MIC' not in mic_c
assert 'I2S_TDM_SLOT0|I2S_TDM_SLOT1|I2S_TDM_SLOT2|I2S_TDM_SLOT3' in mic_c
assert '.channel=4' in mic_c
assert 'ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0)' in mic_c
assert 'MIC_GAIN_DB30' in mic_c
assert 'esp_codec_dev_read(s_input' in mic_c
assert 'prisma_mic_pause_for_playback' in mic_c
assert 'prisma_mic_resume_after_playback' in mic_c
assert 'i2s_channel_disable(s_rx)' in mic_c
assert 'i2s_channel_enable(s_rx)' in mic_c

assert '#defineRECORD_NAME"mic_record.wav"' in mic_c
assert '#defineMAX_RECORD_SECONDS10' in mic_c
assert 'memcpy(h,"RIFF",4)' in mic_c
assert 'memcpy(h+8,"WAVE",4)' in mic_c
assert 'put_le16(h+22,1)' in mic_c
assert 'put_le32(h+24,MIC_RATE)' in mic_c
assert 'fwrite(samples,1,chunk,record_file)' in mic_c
assert 'save_selected(RECORD_NAME)' in mic_c
assert 'prisma_storage_file_count()>=prisma_storage_max_files()' in mic_c

assert 'static bool require_auth' in api
assert 'if (!require_auth(r)) return ESP_OK;' in api
assert '"/api/mic/status"' in api
assert '"/api/mic/record"' in api
assert '"/api/mic/stop"' in api
assert 'Location", "/admin#audio"' in api
assert 'FACTORY AUDIO V1' in page
assert 'LIVE MICROPHONE LEVEL' in page.upper()
assert 'RECORD 5 SEC' in page and 'PLAY RECORDING' in page
assert '/api/mic/status' in page and '/api/mic/record' in page
assert '/api/audio/play' in page
assert 'dbFromPercent' in page and 'meterFromDb' in page
assert 'micTrack' in css and 'speakerValue' in css and 'levelTrack' in css
assert '#define PAGE_COUNT 1' in app
assert 'factory-audio-v1' in workflow
assert 'test_admin_v2_a4_2_mic.py' in workflow

print('Factory Audio V1.2 OK: unified admin, ES7210 capture and ES8311 playback arbitration present')
