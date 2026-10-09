from pathlib import Path

root = Path(__file__).resolve().parents[1]
html = (root / "main/web/admin.html").read_text(encoding="utf-8")
js = (root / "main/web/admin.js").read_text(encoding="utf-8")
api = (root / "main/admin_api.c").read_text(encoding="utf-8")
wifi = (root / "main/wifi_portal.c").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")

required_tabs = ["LIVE VIEW EXTENDED", "SENSORS", "SYSTEM"]
for name in required_tabs:
    assert name in html, f"missing Admin tab: {name}"

assert html.count('class="navBtn') == 3, "Admin sidebar must contain exactly three tabs"
assert html.count('<article class="card') == 2, "System must contain exactly two cards"
assert "Wi-Fi" in html and "Settings" in html
assert "STORAGE FREE" not in html and "MEDIA FILES" not in html

for forbidden in ["setInterval", "requestAnimationFrame"]:
    assert forbidden not in js, f"recurring browser work forbidden in Admin: {forbidden}"

assert "/api/admin/storage" not in js, "Admin UI must not load storage in this layout"
assert "/api/admin/network" in js
assert "wifiCheck" in js and ".onclick=checkWifi" in js
assert "sec-live" in html and "sec-sensors" in html and "sec-system" in html

assert 'uri="/api/admin/network"' in api
assert "prisma_metrics" not in api
assert "prisma_sensors" not in api
assert "prisma_admin_register(s);" in wifi
assert '"admin_api.c"' in cmake
for asset in ["web/admin.html", "web/admin.css", "web/admin.js"]:
    assert asset in cmake, f"missing embedded Admin asset: {asset}"

print("Admin sidebar 3-tab contract: OK")
