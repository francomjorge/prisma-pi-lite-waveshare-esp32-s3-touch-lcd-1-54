#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
audio = (root / "main/audio_service.c").read_text()
header = (root / "main/audio_service.h").read_text()
api = (root / "main/audio_api.c").read_text()
html = (root / "main/web/admin.html").read_text()
css = (root / "main/web/admin.css").read_text()
workflow = (root / ".github/workflows/build.yml").read_text()

assert "esp_err_t prisma_audio_preview_volume(int volume);" in header
assert "int saved_volume;" in header
assert "esp_codec_dev_set_out_vol(s_output, volume)" in audio
preview = audio.split("esp_err_t prisma_audio_preview_volume(int volume)", 1)[1].split("esp_err_t prisma_audio_set_volume(int volume)", 1)[0]
assert "save_volume(" not in preview, "live preview must not write NVS"
assert "s_status.saved_volume = volume;" in audio
assert "static esp_err_t save_volume(int volume)" in audio
assert '"saved_volume":%d' in api
assert "prisma_audio_preview_volume(volume)" in api
assert "strcmp(save,\\"0\\")!=0" in api
assert "save must be 0 or 1" in api
assert "oninput=\\"onVolumeInput()\\" onchange=\\"saveVolume()\\" " in html
assert "savedVolumeValue" in html
assert "setVolumePreset(15)" in html and "setVolumePreset(60)" in html
assert "SAVE VOLUME" in html
assert "volumeScale" in css and "volumePresets" in css
assert "admin-v2-a4-1-volume" in workflow
assert "test_admin_v2_a4_1_volume.py" in workflow
print("Admin V2 A4.1 Volume OK: live preview, no NVS writes while dragging, persisted release/save, presets and saved-state display")
