#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
audio = (root / "main/audio_service.c").read_text()
files = (root / "main/files_plus_api.c").read_text()
transport = (root / "main/playback_http.c").read_text()
portal = (root / "main/wifi_portal_factory.c").read_text()
cmake = (root / "main/CMakeLists.txt").read_text()

assert '#define FILES_NS "media_cfg"' in audio, "audio playback must read the FILES+ sample namespace"
assert '#define CFG_NS "media_cfg"' in files
assert '"admin_api_v2.c"' in cmake
assert '"admin_api_factory.c"' not in cmake
assert '"playback_http.c"' in cmake
assert 'prisma_playback_http_register(server);' in portal
assert portal.index('prisma_playback_http_register(server);') < portal.index('prisma_admin_register(server);')
assert portal.index('prisma_playback_http_register(server);') < portal.index('prisma_files_plus_register(server);')
assert 'HTTP_HEAD' in transport
assert '206 Partial Content' in transport
assert 'Content-Range: bytes %u-%u/%u' in transport
assert 'Content-Length: %u' in transport
assert 'Accept-Ranges: bytes' in transport
assert 'Content-Transfer-Encoding: binary' in transport
assert 'httpd_send(r, buf, len)' in transport
assert 'httpd_resp_send_chunk' not in transport
assert '"/media/*"' in transport
assert '"/files/*"' in transport
assert 'attachment' in transport and 'inline' in transport
assert '#define PLAY_STACK 8192' in audio
assert '#define WAV_RAW_BYTES 512' in audio
assert '#define WAV_OUT_SAMPLES 512' in audio
assert 'uint8_t raw[WAV_RAW_BYTES];' in audio
assert 'int16_t out[WAV_OUT_SAMPLES];' in audio
assert 'uint8_t raw[1024];' not in audio
assert 'int16_t out[2048];' not in audio
# Validate actual error handling, not a legacy UI string.
assert 'if(!f){set_error(' in audio
assert 'if(!parse_wav(f,&w))' in audio
assert 'if(!write_pcm(out,on))' in audio
assert 'set_error("Audio codec write failed")' in audio

print("Admin V2 A4.1.3 Playback OK: browser transport preserved and on-device WAV task has safe stack headroom")
