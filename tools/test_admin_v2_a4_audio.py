#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
cmake=(root/'main/CMakeLists.txt').read_text()
audio=(root/'main/audio_service.c').read_text()
api=(root/'main/audio_api.c').read_text()
html=(root/'main/web/admin.html').read_text()
touch=(root/'main/prisma_touch.c').read_text()
manifest=(root/'main/idf_component.yml').read_text()

for s in ['audio_service.c','audio_api.c','esp_driver_i2s','esp_codec_dev']:
    assert s in cmake
assert 'esp_codec_dev' in manifest and '~1.5' in manifest

for s in ['GPIO_NUM_7','GPIO_NUM_8','GPIO_NUM_9','GPIO_NUM_10','GPIO_NUM_12','ES8311_CODEC_DEFAULT_ADDR','ES7210_CODEC_DEFAULT_ADDR']:
    assert s in audio
assert 'I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG' in audio
assert 'AUDIO_RATE 16000' in audio
assert 'esp_codec_dev_write' in audio
assert 'esp_codec_dev_set_out_vol' in audio
assert '440.0f' in audio
assert 'WAV must be PCM' in audio
assert 'prisma_audio_init(bus)' in touch

# esp_codec_dev constants are shifted/8-bit addresses (ES8311=0x30,
# ES7210=0x80), while ESP-IDF i2c_master_probe() requires 7-bit addresses.
# The hardware probe must therefore use >>1, but the codec control adapter
# must keep ES8311_CODEC_DEFAULT_ADDR unchanged.
assert '#define ES8311_PROBE_ADDR ((uint16_t)(ES8311_CODEC_DEFAULT_ADDR >> 1))' in audio
assert '#define ES7210_PROBE_ADDR ((uint16_t)(ES7210_CODEC_DEFAULT_ADDR >> 1))' in audio
assert 'i2c_master_probe(i2c_bus, ES8311_PROBE_ADDR' in audio
assert 'i2c_master_probe(i2c_bus, ES7210_PROBE_ADDR' in audio
assert '.addr = ES8311_CODEC_DEFAULT_ADDR' in audio

for uri in ['/api/audio/status','/api/audio/volume','/api/audio/tone','/api/audio/play','/api/audio/stop']:
    assert uri in api
assert 'require_auth' in api

assert 'ADMIN V2 A4' in html
assert "data-tab=\"audio\"" in html
assert 'PLAY 440 HZ' in html
assert 'PLAY SELECTED WAV' in html
assert 'ES8311' in html and 'ES7210' in html
assert 'A4 AUDIO' in html
assert 'A5 BLE' in html
assert 'SYSTEM CORE' in html

print('Admin V2 A4 AUDIO OK: official pin map, 7-bit codec probes, volume, tone, selected WAV playback, physical UI frozen')
