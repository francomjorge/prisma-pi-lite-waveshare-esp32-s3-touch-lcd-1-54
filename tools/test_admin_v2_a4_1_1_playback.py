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

# Guard the exact production source set. The prior A4.1.1 test accidentally
# asserted admin_api_factory.c even though CMake compiles admin_api_v2.c.
assert '"admin_api_v2.c"' in cmake
assert '"admin_api_factory.c"' not in cmake
assert '"playback_http.c"' in cmake

# Authoritative transport must be registered before legacy routes so the old
# chunked /files/* and /media/* handlers cannot win registration.
assert 'prisma_playback_http_register(server);' in portal
assert portal.index('prisma_playback_http_register(server);') < portal.index('prisma_admin_register(server);')
assert portal.index('prisma_playback_http_register(server);') < portal.index('prisma_files_plus_register(server);')

# Safari/iOS media needs byte ranges and fixed Content-Length; downloads need
# the same fixed-length transport so iOS never reports "-1 byte".
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

print("Admin V2 A4.1.2 Playback OK: production routes fixed, HEAD/range/fixed-length media transport active, sample namespace aligned")
