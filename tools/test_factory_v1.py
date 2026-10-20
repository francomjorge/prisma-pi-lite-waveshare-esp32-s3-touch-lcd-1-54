#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
cmake=(root/'main/CMakeLists.txt').read_text()
app=(root/'main/app_factory_v1.c').read_text()
power=(root/'main/prisma_power.c').read_text()
storage=(root/'main/storage.c').read_text()
admin=(root/'main/admin_api.c').read_text()
html=(root/'main/web/admin.html').read_text()
parts=(root/'partitions.csv').read_text()

assert '"app_factory_v1.c"' in cmake
assert '"wifi_portal_factory.c"' in cmake
assert 'app_checkpoint_a.c' not in cmake
assert 'wifi_portal_checkpoint_a.c' not in cmake
assert 'web/index.html' not in cmake
assert 'web/admin.html' in cmake and 'web/admin.css' in cmake

assert '#define PAGE_COUNT 1' in app
assert 'PRISMA-PI LITE FACTORY V1' in app
assert 'glitch(startx,starty)' in app
assert '(int)startx-(int)x > 55' in app
assert '(int)x-(int)startx > 55' in app
assert 'SYSTEM CORE' in app

assert '#define POWER_OFF_HOLD_MS 5000' in power
assert '#define POWER_ON_HOLD_MS 3000' in power
assert '#define RESTART_HOLD_MS 2000' in power

assert '#define PRISMA_MAX_FILES 4' in storage
assert 'format_if_mount_failed = false' in storage
assert 'prisma_storage_file_count' in storage

assert '.uri="/api/files"' in admin
assert 'HTTP_POST' in admin
assert 'HTTP_DELETE' in admin
assert '.uri="/files/*"' in admin
assert 'FACTORY_MAX_UPLOAD_BYTES' in admin
assert 'audio/wav' in admin

assert 'Ready for nanoPAD' in html
assert 'UPLOAD' in html and 'DOWNLOAD' in html and 'DELETE' in html
assert 'Sin pedal ni efectos' in html
assert '/api/files?name=' in html

assert 'factory,  app,  factory' in parts
assert 'ota_0' not in parts and 'ota_1' not in parts and 'otadata' not in parts
assert '0x9F0000' in parts

assert not (root/'storage/F1_MOON.JPG').exists()
assert not (root/'storage/F1_TANGO.JPG').exists()

print('Factory V1 OK: home+swipe+power+WiFi+generic 4-file storage, no product UI')
