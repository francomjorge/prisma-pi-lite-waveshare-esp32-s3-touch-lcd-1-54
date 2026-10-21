#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmake = (root / "main/CMakeLists.txt").read_text()
touch = (root / "main/prisma_touch.c").read_text()
portal = (root / "main/wifi_portal_factory.c").read_text()
mic = (root / "main/mic_service.c").read_text()
api = (root / "main/mic_api.c").read_text()
page = (root / "main/web/mic.html").read_text()
app = (root / "main/app_factory_v1.c").read_text()
workflow = (root / ".github/workflows/build.yml").read_text()

assert '"mic_service.c"' in cmake and '"mic_api.c"' in cmake
assert '"web/mic.html"' in cmake and '"web/mic.css"' in cmake
assert 'prisma_mic_init(bus)' in touch
assert touch.index('prisma_audio_init(bus)') < touch.index('prisma_mic_init(bus)')
assert 'prisma_mic_api_register(server);' in portal

# Exact Waveshare A4.2 capture hardware path.
assert '#define MIC_MCLK GPIO_NUM_8' in mic
assert '#define MIC_BCLK GPIO_NUM_9' in mic
assert '#define MIC_WS GPIO_NUM_10' in mic
assert '#define MIC_DIN GPIO_NUM_11' in mic
assert 'ES7210_CODEC_DEFAULT_ADDR' in mic
assert 'ES7120_SEL_MIC1 | ES7120_SEL_MIC2 | ES7120_SEL_MIC3 | ES7120_SEL_MIC4' in mic
assert 'I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3' in mic
assert '.channel = 4' in mic
assert 'ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0)' in mic
assert 'MIC_GAIN_DB 30' in mic
assert 'esp_codec_dev_read(s_input' in mic

# Recording is bounded, stream-written and standard PCM WAV.
assert '#define RECORD_NAME "mic_record.wav"' in mic
assert '#define MAX_RECORD_SECONDS 10' in mic
assert 'memcpy(h + 0, "RIFF", 4)' in mic
assert 'memcpy(h + 8, "WAVE", 4)' in mic
assert 'put_le16(h + 22, 1)' in mic
assert 'put_le32(h + 24, MIC_RATE)' in mic
assert 'fwrite(samples, 1, chunk, record_file)' in mic
assert 'save_selected(RECORD_NAME)' in mic
assert 'prisma_storage_file_count() >= prisma_storage_max_files()' in mic

# Web diagnostics and protected API.
assert 'static bool require_auth' in api
assert 'if (!require_auth(r)) return ESP_OK;' in api
assert '"/api/mic/status"' in api
assert '"/api/mic/record"' in api
assert '"/api/mic/stop"' in api
assert 'LIVE LEVEL' in page and 'RECORD 5 SEC' in page and 'PLAY RECORDING' in page
assert '/api/audio/play' in page

# Appliance UI remains frozen; A4.2 is web-only.
assert '#define PAGE_COUNT 1' in app
assert 'admin-v2-a4-2-mic' in workflow
assert 'test_admin_v2_a4_2_mic.py' in workflow

print('Admin V2 A4.2 MIC OK: ES7210 TDM capture, live meter, bounded WAV recording, auto-select and playback loop')
