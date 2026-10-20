#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
cmake=(root/'main/CMakeLists.txt').read_text()
app=(root/'main/app_factory_v1.c').read_text()
net=(root/'main/network_service.c').read_text()
api=(root/'main/admin_api_factory.c').read_text()
html=(root/'main/web/admin.html').read_text()
css=(root/'main/web/admin.css').read_text()

# Physical appliance UI remains frozen at the known one-page Factory baseline.
assert '#define PAGE_COUNT 1' in app
assert 'SYSTEM CORE' in app
assert 'midi_service.h' not in app

# Network service is modular and AP fallback remains alive while STA is used.
assert '"network_service.c"' in cmake
assert 'WIFI_MODE_APSTA' in net
assert 'PRISMA-PI-%02X%02X' in net
assert 'prisma_network_scan' in net
assert 'prisma_network_connect' in net
assert 'prisma_network_disconnect' in net
assert 'prisma_network_reconnect' in net
assert 'prisma_network_forget' in net
assert 'prisma_network_set_ap' in net
assert 'WIFI_STORAGE_RAM' in net
assert 'KEY_STA_SSID' in net and 'KEY_STA_PASS' in net

# Web API supports full A1 lifecycle without exposing the saved station password.
for route in [
    '/api/network/status', '/api/network/scan', '/api/network/connect',
    '/api/network/disconnect', '/api/network/reconnect', '/api/network/forget',
    '/api/network/ap'
]:
    assert route in api
assert 'PRISMA-PI LITE ADMIN V2 A1' in api
assert 'sta_pass' not in api

# Admin UI now has real CORE / NETWORK / FILES tabs and future subsystem placeholders.
for token in ['data-tab="core"','data-tab="network"','data-tab="files"','SECURITY','AUDIO','BLUETOOTH','SENSORS','GPS','MIDI']:
    assert token in html
for action in ['SCAN NETWORKS','FORGET NETWORK','SAVE FALLBACK AP','/api/network/connect','/api/network/forget']:
    assert action in html
assert '.networkRow' in css and '.statusPill' in css

print('Admin V2 A1 OK: frozen appliance UI + AP/STA network administration')
