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
pages = len(re.findall(r'<section\s+class="page(?:\s|\")', index))
dots = len(re.findall(r'id="d\d+"\s+class="dot', index))
assert pages == EXPECTED_PAGES, f"HTML pages={pages}, expected={EXPECTED_PAGES}"
assert dots == EXPECTED_PAGES, f"HTML dots={dots}, expected={EXPECTED_PAGES}"
assert "width:400%" in style
assert "width:25%" in style
assert "flex:0 0 25%" in style
assert "Math.min(3,n)" in index
assert "pg*25" in index

# Single embedded web source.
assert '"web/index.html"' in cmake
assert '"web/style.css"' in cmake
assert '"web/app.js"' not in cmake
assert '<script src="/app.js"></script>' not in index
assert '"/app.js"' not in portal
assert '"/api/control"' not in portal

# The stable /api/state path is authoritative for Page 2 and Page 4.
for symbol in (
    "prisma_appliance_get_on",
    "prisma_appliance_get_mode",
    "prisma_appliance_get_intensity",
    "prisma_appliance_get_filename",
    "prisma_appliance_get_effect",
    "prisma_appliance_get_effect_value",
):
    assert symbol in header, f"missing declaration {symbol}"
    assert symbol in wrapper, f"missing direct getter {symbol}"
    assert symbol in portal, f"state API not using {symbol}"

assert '"/api/state"' in portal
assert '\\"effects\\"' in portal
assert '\\"values\\"' in portal
assert "char b[640]" in portal

# Browser must use one serialized polling loop regardless of selected page.
assert "'/api/state?ts='+Date.now()" in index
assert "new XMLHttpRequest()" in index
assert "syncBusy" in index
assert "scheduleSync(300)" in index
assert "if(pg!==1)" not in index
assert "pg===3" not in index
assert "setInterval(syncAll" not in index

# Page 2 and Page 4 renderers both consume the same snapshot.
assert "function drawPage2(s)" in index
assert "function drawPage4(s)" in index
assert "function drawAll(s){drawPage2(s);drawPage4(s)}" in index
assert "Array.isArray(s.effects)" in index
assert "Array.isArray(s.values)" in index
assert "fxmirror" in index

# Page 3 source preview is updated from the same global state even while visible.
assert 'id="sourcePreview"' in index
assert 'id="fsBtn"' in index
assert '"/source/*"' in portal
assert "setSource(s.filename)" in index

# Page 5 / Sync Lab must remain removed.
assert "PAGE 5" not in index
assert "SYNC LAB" not in index
assert "api/debug/state" not in index
assert '"/api/debug/state"' not in portal
assert 'id="d4"' not in index
assert "esp_timer" not in cmake

# Conservative HTTP server settings retained for AP stability.
assert "hc.lru_purge_enable=true" in portal
assert "no-store, no-cache, must-revalidate, max-age=0" in portal

print("M4.9 OK: four pages + global serialized sync + Page 2/Page 4 authoritative state")
