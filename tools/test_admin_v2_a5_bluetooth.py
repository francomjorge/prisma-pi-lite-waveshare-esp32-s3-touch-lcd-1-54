#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmake = (root / "main/CMakeLists.txt").read_text()
portal = (root / "main/wifi_portal_factory.c").read_text()
shell = (root / "main/admin_shell.c").read_text()
service = (root / "main/bluetooth_service.c").read_text()
header = (root / "main/bluetooth_service.h").read_text()
api = (root / "main/bluetooth_api.c").read_text()
js = (root / "main/web/bluetooth.js").read_text()
css = (root / "main/web/bluetooth.css").read_text()
sdk = (root / "sdkconfig.defaults").read_text()
app = (root / "main/app_factory_v1.c").read_text()
admin_html = (root / "main/web/admin.html").read_text()

# FACTORY AUDIO V1 remains the product baseline; physical UI stays frozen.
assert '#define PAGE_COUNT 1' in app
assert 'FACTORY AUDIO V1' in admin_html
# The base admin asset stays untouched; Bluetooth is an optional extension.
assert '/bluetooth.js' not in admin_html

# Build includes BLE and the modular web extension.
assert '"bluetooth_service.c"' in cmake
assert '"bluetooth_api.c"' in cmake
assert '"admin_shell.c"' in cmake
assert ' bt' in cmake
assert '"web/bluetooth.js"' in cmake
assert '"web/bluetooth.css"' in cmake

# ESP32-S3 uses NimBLE / BLE roles; no Bluedroid Classic stack.
assert 'CONFIG_BT_ENABLED=y' in sdk
assert 'CONFIG_BT_NIMBLE_ENABLED=y' in sdk
assert '# CONFIG_BT_BLUEDROID_ENABLED is not set' in sdk
assert 'CONFIG_BT_NIMBLE_ROLE_CENTRAL=y' in sdk
assert 'CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y' in sdk
assert 'CONFIG_BT_NIMBLE_ROLE_OBSERVER=y' in sdk
assert 'CONFIG_BT_NIMBLE_GATT_CLIENT=y' in sdk
assert 'CONFIG_BT_NIMBLE_GATT_SERVER=y' in sdk
assert 'CONFIG_BT_NIMBLE_NVS_PERSIST=y' in sdk

# Regression guard: HTTP/admin must be alive BEFORE NimBLE is initialized.
assert 'httpd_start(&server, &hc)' in portal
assert 'xTaskCreate(ble_start_task' in portal
assert portal.index('httpd_start(&server, &hc)') < portal.index('xTaskCreate(ble_start_task')
assert 'vTaskDelay(pdMS_TO_TICKS(1200))' in portal
assert 'prisma_bluetooth_init()' in portal
assert 'admin remains available' in portal

# /admin shell is fixed-length and fail-safe. Never make the working base page
# depend on a large chunked response or successful extension allocation.
assert 'src=\\"/bluetooth.js\\"' in shell
assert 'heap_caps_malloc' in shell
assert 'MALLOC_CAP_SPIRAM' in shell
assert 'httpd_resp_send_chunk' not in shell
assert 'httpd_resp_send(r, page' in shell
assert 'serving base admin unchanged' in shell
assert 'httpd_resp_send(r, start' in shell

# Runtime is exposed in the single /admin surface.
assert 'prisma_bluetooth_api_register(server);' in portal
assert 'prisma_admin_shell_register(server);' in portal
assert portal.index('prisma_admin_shell_register(server);') < portal.index('prisma_admin_register(server);')

# BLE runtime capabilities: scan, advertise, connect, disconnect and bond.
assert 'PRISMA_BT_MAX_DEVICES 20' in header
assert 'nimble_port_init()' in service
assert 'ble_gap_disc(' in service
assert 'ble_hs_adv_parse_fields' in service
assert 'ble_gap_adv_start(' in service
assert 'BLE_HS_ADV_F_BREDR_UNSUP' in service
assert 'ble_gap_connect(' in service
assert 'ble_gap_terminate(' in service
assert 'ble_gap_security_initiate(' in service
assert 'ble_gap_conn_rssi(' in service
assert 'ble_store_config_init()' in service
assert 'sm_bonding = 1' in service

# Protected HTTP API uses the fixed boolean authorization pattern.
assert 'static bool require_auth' in api
assert 'if (!require_auth(r)) return ESP_OK;' in api
for endpoint in [
    '/api/bluetooth/status', '/api/bluetooth/scan', '/api/bluetooth/scan-stop',
    '/api/bluetooth/connect', '/api/bluetooth/disconnect', '/api/bluetooth/pair',
    '/api/bluetooth/advertise'
]:
    assert endpoint in api
assert r'\"classic\":false' in api
assert r'\"a2dp\":false' in api
assert r'\"ble_midi_profile\":false' in api

# The previously grey tab becomes a real admin tab with complete diagnostics.
assert "textContent.trim() === 'BLUETOOTH'" in js
assert "tab.dataset.tab = 'bluetooth'" in js
assert "id = 'tab-bluetooth'" in js
assert '/api/bluetooth/status' in js
assert '/api/bluetooth/scan' in js
assert '/api/bluetooth/connect' in js
assert '/api/bluetooth/disconnect' in js
assert '/api/bluetooth/pair' in js
assert '/api/bluetooth/advertise' in js
assert 'Classic Bluetooth' in js and 'NOT SUPPORTED' in js
assert 'BLE MIDI profile' in js
assert 'btDeviceList' in css and 'btCapabilities' in css

print('Admin V2 A5 Bluetooth SAFE OK: Factory Audio admin first, fixed-length fail-safe shell, deferred NimBLE, BLE scan/connect/advertise/bonding preserved')
