#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
index = (root / "main/web/index.html").read_text(encoding="utf-8")
style = (root / "main/web/style.css").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")
portal = (root / "main/wifi_portal.c").read_text(encoding="utf-8")
header = (root / "main/wifi_portal.h").read_text(encoding="utf-8")
wrapper = (root / "main/app36.c").read_text(encoding="utf-8")

EXPECTED_PAGES = 4
pages = len(re.findall(r'<section\s+class="page"', index))
dots = len(re.findall(r'id="d\d+"\s+class="dot', index))
assert pages == EXPECTED_PAGES, f"HTML pages={pages}, expected={EXPECTED_PAGES}"
assert dots == EXPECTED_PAGES, f"HTML dots={dots}, expected={EXPECTED_PAGES}"
assert "width:400%" in style
assert "width:25%" in style
assert "flex:0 0 25%" in style
assert "Math.min(3,n)" in index
assert "pg*25" in index

# Single active web source: Page2 sync is inline, not external app.js.
assert '"web/index.html"' in cmake
assert '"web/style.css"' in cmake
assert '"web/app.js"' not in cmake
assert '<script src="/app.js"></script>' not in index
assert '"/app.js"' not in portal
assert '"/api/control"' not in portal

# Direct authoritative appliance state contract.
for symbol in (
    "prisma_appliance_get_on",
    "prisma_appliance_get_mode",
    "prisma_appliance_get_intensity",
    "prisma_appliance_get_filename",
):
    assert symbol in header, f"missing declaration {symbol}"
    assert symbol in wrapper, f"missing direct getter {symbol}"
    assert symbol in portal, f"/api/state not using {symbol}"

assert "mirror_task" not in wrapper, "Page2 must not depend on mirrored duplicate state"
assert "PAGE2 GET TX" in portal, "state-change serial trace missing"
assert '"/api/state"' in portal
assert '"/source/*"' in portal
assert "no-store, no-cache, must-revalidate, max-age=0" in portal

# Inline browser polling and DOM updates.
assert "async function syncPage2()" in index
assert "fetch('/api/state?t='+Date.now()" in index
assert "setInterval(syncPage2,300)" in index
for control_id in ("filename", "power", "pct", "fill", "mode"):
    assert f'id="{control_id}"' in index

print("M4.7 OK: 4-page layout + direct authoritative read-only Page2 + inline 300ms polling")
