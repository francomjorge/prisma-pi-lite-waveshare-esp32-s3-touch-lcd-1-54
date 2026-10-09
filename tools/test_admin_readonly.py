#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
admin = (root / "main/web/admin.html").read_text(encoding="utf-8")
css = (root / "main/web/admin.css").read_text(encoding="utf-8")
js = (root / "main/web/admin.js").read_text(encoding="utf-8")
api = (root / "main/admin_api.c").read_text(encoding="utf-8")
portal = (root / "main/wifi_portal.c").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")
version = (root / "VERSION_1.1.txt").read_text(encoding="utf-8")

for section in ("overview","live","parameters","storage","network","power","sensors","system","diagnostics","appearance"):
    assert f'id="sec-{section}"' in admin, section
    assert f'data-section="{section}"' in admin, section

assert '/admin.css' in admin
assert '/admin.js' in admin
assert 'data-theme-pick="prisma"' in admin
assert 'data-theme-pick="alt"' in admin
assert 'READ ONLY / PHASE 1' in admin
assert 'UPLOAD DISABLED' in admin
assert 'QMI8658' in admin
assert '512 KiB' in admin and '256 KiB' in admin and '>32<' in admin

# Overview is appliance-health focused: no controller duplication.
assert 'Controller snapshot' not in admin
for field in (
    'ovDevice','ovDeviceSub','ovBattery','ovBatteryV','ovWifi','ovClients',
    'ovStorage','ovStorageSub','ovVersion','ovIdf','ovUptime','ovHeap',
    'ovMinHeap','ovReset','ovBuild','ovHealthState','ovHealthAdmin',
    'ovHealthStorage','ovHealthNetwork'
):
    assert f'id="{field}"' in admin, field
assert 'Services health' in admin
assert 'System health' in admin
assert 'function serviceHealth(id,state)' in js
for health in ('ovHealthState','ovHealthAdmin','ovHealthStorage','ovHealthNetwork'):
    assert health in js
assert "setText('ovDevice','ONLINE')" in js
assert "setText('ovDevice','OFFLINE')" in js
assert "setText('ovWifi','ACTIVE')" in js

# First read-only parameter surface reuses the authoritative controller state.
for field in ('paPower','paMode','paIntensity','paSource','paGeneration'):
    assert f'id="{field}"' in admin
for i in range(6):
    assert f'id="paFx{i}"' in admin
    assert f'id="paVal{i}"' in admin
assert 'function drawParameters(s)' in js
assert 'drawParameters(s)' in js
assert 'Array.isArray(s.effects)' in js
assert 'Array.isArray(s.values)' in js

assert '@media(max-width:720px)' in css
assert 'body.menuOpen .sidebar' in css
assert 'html[data-theme="alt"]' in css
assert '.liveWrap' in css
assert '.wideBtn{' in css

for endpoint in ('/api/state','/api/admin/overview','/api/admin/storage','/api/admin/network'):
    assert endpoint in js or endpoint in api
assert "e.key==='r'||e.key==='R'" in js
assert "e.key==='f'||e.key==='F'" in js
assert 'function toggleAdminFs()' in js
assert 'F FULLSCREEN' in admin
assert 'localStorage' in js

assert '.uri="/admin"' in api
assert '.uri="/admin.css"' in api
assert '.uri="/admin.js"' in api
assert '.uri="/api/admin/overview"' in api
assert '.uri="/api/admin/storage"' in api
assert '.uri="/api/admin/network"' in api
assert 'HTTP_POST' not in api and 'HTTP_PUT' not in api and 'HTTP_DELETE' not in api
assert 'ADMIN_MAX_FILE_BYTES (512U * 1024U)' in api
assert 'ADMIN_STORAGE_RESERVE (256U * 1024U)' in api
assert 'esp_wifi_ap_get_sta_list' in api
assert 'esp_spiffs_info' in api
assert 'prisma_metrics_battery' in api
assert 'httpd_resp_send_chunk(r, NULL, 0)' in api
assert 'finish_chunks(r)' in api

assert 'prisma_admin_register(s);' in portal
assert 'hc.max_uri_handlers=16;' in portal
assert '"web/admin.html"' in cmake and '"web/admin.css"' in cmake and '"web/admin.js"' in cmake
assert '"admin_api.c"' in cmake and '"prisma_metrics.c"' in cmake
assert 'esp_adc' in cmake
assert 'ADMIN READ-ONLY / PHASE 1' in version

print('v1.1 admin OK: appliance-health overview + parameters + F fullscreen + responsive navigation + skins + real telemetry')
