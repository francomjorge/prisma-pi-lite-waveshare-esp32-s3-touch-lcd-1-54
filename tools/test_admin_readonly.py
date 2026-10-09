#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
admin = (root / "main/web/admin.html").read_text(encoding="utf-8")
css = (root / "main/web/admin.css").read_text(encoding="utf-8")
api = (root / "main/admin_api.c").read_text(encoding="utf-8")
portal = (root / "main/wifi_portal.c").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")
metrics = (root / "main/prisma_metrics.c").read_text(encoding="utf-8")
sensors = (root / "main/prisma_sensors.c").read_text(encoding="utf-8")
touch = (root / "main/prisma_touch.c").read_text(encoding="utf-8")
version = (root / "VERSION_1.1.txt").read_text(encoding="utf-8")

for section in ("overview","live","parameters","storage","network","power","sensors","system","diagnostics","appearance"):
    assert f'id="sec-{section}"' in admin, section
    assert f'data-section="{section}"' in admin, section

assert '/admin.css' in admin
assert '<script src="/admin.js"' not in admin
assert 'data-theme-pick="prisma"' in admin
assert 'data-theme-pick="alt"' in admin
assert 'READ ONLY / ON DEMAND' in admin
assert 'NO BACKGROUND POLLING' in admin

# Overview is intentionally clean: large Live View Extended + future upload placeholder.
for removed in ('ovWifi','ovClients','wifiRefresh','ovStorage','ovStorageSub','ovStorageBar','ovMediaCount','ovMediaList'):
    assert f'id="{removed}"' not in admin, removed
assert 'APPLIANCE STATUS' not in admin
assert 'Overview liviano' not in admin
assert 'statusDot' not in admin
assert '<div class="card w8 overviewLiveCard">' in admin
assert '<div class="cardTitle">Live View Extended</div>' in admin
assert 'id="overviewLiveFrame"' in admin
assert 'id="overviewPreview"' in admin
assert 'id="overviewLiveBadge"' in admin
assert 'id="overviewRefresh"' in admin
assert '<div class="card w4 placeholder uploadFuture">' in admin
assert '<div class="cardTitle">Media Upload</div>' in admin
assert 'UPLOAD FILES' in admin
assert '<button class="wideBtn" disabled>COMING SOON</button>' in admin

# No background polling loops in Admin. Reads happen only once initially or on explicit navigation/button action.
for forbidden in ('function loopStatus()', 'function loopState()', 'function loopStorage()', 'function loopNetwork()', 'function loopSensors()', 'setInterval('):
    assert forbidden not in admin, forbidden
assert 'setTimeout(loop' not in admin
assert 'function loadStorageOnce()' in admin
assert 'function loadWifiOnDemand()' in admin
assert "api('/api/admin/network'" in admin
assert "api('/api/admin/storage'" in admin
assert "api('/api/admin/sensors'" in admin
assert "api('/api/admin/overview'" in admin
assert "api('/api/state'" in admin
assert "if(name==='storage')loadStorageOnce();" in admin
assert admin.rstrip().endswith('</body></html>')
assert 'loadStateOnce();\n})();' in admin
assert 'loadStorageOnce();\n})();' not in admin

# Parameter surface still mirrors authoritative /api/state on demand.
for field in ('paPower','paMode','paIntensity','paSource','paGeneration'):
    assert f'id="{field}"' in admin
for i in range(6):
    assert f'id="paFx{i}"' in admin
    assert f'id="paVal{i}"' in admin
assert 'Array.isArray(s.effects)' in admin
assert 'Array.isArray(s.values)' in admin

# Storage is calculated/scanned exactly once per boot and then served from RAM cache.
assert 'static bool storage_snapshot_done;' in api
assert 'static void admin_storage_snapshot_init(void)' in api
assert 'if (storage_snapshot_done) return;' in api
assert 'admin_storage_snapshot_init();' in api
assert api.count('esp_spiffs_info("storage"') == 1
assert api.count('opendir("/storage")') == 1
assert 'storage_files_cached' in api
assert r'\"snapshot\":\"boot\"' in api
assert 'file_count' in api

# Wi-Fi remains explicit/on-demand only.
assert 'Wi-Fi is intentionally calculated only when this endpoint is requested.' in api
assert 'esp_wifi_get_config(WIFI_IF_AP' in api
assert 'esp_wifi_ap_get_sta_list' in api
assert 'return admin_overview_get(r);' in api

# Official Waveshare-style system and power telemetry remains available on demand.
assert 'BAT_CHARGING_PIN GPIO_NUM_3' in metrics
assert 'BAT_ADC_CHANNEL ADC_CHANNEL_0' in metrics
assert 'BAT_DIVIDER_X1000 3000' in metrics
assert 'temperature_sensor_get_celsius' in metrics
assert 'esp_flash_get_size' in metrics
assert 'heap_caps_get_total_size(MALLOC_CAP_SPIRAM)' in metrics

# QMI8658 remains on the shared touchscreen I2C bus.
assert 'QMI8658_ADDR       0x6B' in sensors
assert 'QMI8658_WHO_AM_I' in sensors
assert 'id != 0x05' in sensors
assert 'QMI8658_RESET, 0xB0' in sensors
assert 'QMI8658_CTRL2, 0x95' in sensors
assert 'QMI8658_CTRL3, 0xD5' in sensors
assert 'prisma_sensors_init(bus);' in touch
assert 'i2c_new_master_bus' in touch
assert 'i2c_master_bus_add_device(bus' in sensors
for field in ('imuAx','imuAy','imuAz','imuGx','imuGy','imuGz','imuAngleX','imuAngleY','imuAngleZ'):
    assert f'id="{field}"' in admin

assert '@media(max-width:720px)' in css
assert 'body.menuOpen .sidebar' in css
assert 'html[data-theme="alt"]' in css
assert '.liveWrap' in css
assert '.wideBtn{' in css
assert '.card.w8' in css and '.card.w4' in css
assert "e.key==='r'||e.key==='R'" in admin
assert "e.key==='f'||e.key==='F'" in admin
assert 'function toggleFs()' in admin
assert 'F FULLSCREEN' in admin
assert 'localStorage' in admin

assert '.uri="/admin"' in api
assert '.uri="/api/admin/overview"' in api
assert '.uri="/api/admin/status"' in api
assert '.uri="/api/admin/sensors"' in api
assert '.uri="/api/admin/storage"' in api
assert '.uri="/api/admin/network"' in api
assert 'HTTP_POST' not in api and 'HTTP_PUT' not in api and 'HTTP_DELETE' not in api
assert 'ADMIN_MAX_FILE_BYTES (512U * 1024U)' in api
assert 'ADMIN_STORAGE_RESERVE (256U * 1024U)' in api
assert 'prisma_admin_register(s);' in portal
assert 'hc.max_uri_handlers=16;' in portal
assert '"web/admin.html"' in cmake and '"web/admin.css"' in cmake
assert '"prisma_sensors.c"' in cmake and '"prisma_metrics.c"' in cmake and '"admin_api.c"' in cmake
assert 'esp_adc' in cmake and 'esp_driver_tsens' in cmake and 'heap' in cmake and 'spi_flash' in cmake
assert 'ADMIN READ-ONLY / PHASE 1' in version

print('v1.1 admin OK: clean 2:1 overview live/upload layout + zero background polling + cached storage + on-demand telemetry')
