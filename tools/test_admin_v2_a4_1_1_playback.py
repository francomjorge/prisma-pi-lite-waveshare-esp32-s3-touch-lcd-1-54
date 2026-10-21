#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
audio = (root / "main/audio_service.c").read_text()
files = (root / "main/files_plus_api.c").read_text()
admin = (root / "main/admin_api_factory.c").read_text()

assert '#define FILES_NS "media_cfg"' in audio, "audio playback must read the same NVS namespace used by FILES+"
assert '#define CFG_NS "media_cfg"' in files
assert 'httpd_resp_set_hdr(r,"Accept-Ranges","bytes")' in files or 'httpd_resp_set_hdr(r, "Accept-Ranges", "bytes")' in files
assert '206 Partial Content' in files
assert 'Content-Range' in files
assert 'httpd_resp_send(r,(const char *)data' in files or 'httpd_resp_send(r, (const char *)data' in files
assert 'heap_caps_malloc' in files
assert 'heap_caps_malloc' in admin
assert 'httpd_resp_send(r, (const char *)data' in admin
assert 'Content-Disposition' in admin
print("Admin V2 A4.1.1 Playback OK: FILES+ sample namespace aligned, Safari byte ranges enabled, downloads have Content-Length")
