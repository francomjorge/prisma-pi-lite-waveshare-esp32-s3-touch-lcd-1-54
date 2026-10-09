from pathlib import Path

root = Path(__file__).resolve().parents[1]
html = (root / "main/web/admin.html").read_text(encoding="utf-8")
js = (root / "main/web/admin.js").read_text(encoding="utf-8")
api = (root / "main/admin_api.c").read_text(encoding="utf-8")
wifi = (root / "main/wifi_portal.c").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")

required_cards = ["WI-FI", "STORAGE FREE", "MEDIA FILES"]
for name in required_cards:
    assert name in html, f"missing Admin card: {name}"

assert html.count('<article class="card') == 3, "Admin must contain exactly three cards"

for forbidden in ["LIVE VIEW", "PARAMETERS", "POWER / BATTERY", "SENSORS", "DIAGNOSTICS", "APPEARANCE"]:
    assert forbidden not in html, f"forbidden Admin section present: {forbidden}"

for forbidden in ["setInterval", "requestAnimationFrame"]:
    assert forbidden not in js, f"recurring browser work forbidden in Admin: {forbidden}"

assert "/api/admin/storage" in js
assert "/api/admin/network" in js
assert "wifiCheck" in js and ".onclick=checkWifi" in js

assert "admin_storage_snapshot_init();" in api
assert api.count("opendir(\"/storage\")") == 1, "storage enumeration must exist in one snapshot path only"
assert 'uri="/api/admin/storage"' in api
assert 'uri="/api/admin/network"' in api
assert "prisma_metrics" not in api
assert "prisma_sensors" not in api

assert "prisma_admin_register(s);" in wifi
assert '"admin_api.c"' in cmake
for asset in ["web/admin.html", "web/admin.css", "web/admin.js"]:
    assert asset in cmake, f"missing embedded Admin asset: {asset}"

print("Minimal Admin static contract: OK")
