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

# Single active web source.
assert '"web/index.html"' in cmake
assert '"web/style.css"' in cmake
assert '"web/app.js"' not in cmake
assert '<script src="/app.js"></script>' not in index
assert '"/app.js"' not in portal
assert '"/api/control"' not in portal

# Page 2 must keep using the direct authoritative appliance state.
for symbol in (
    "prisma_appliance_get_on",
    "prisma_appliance_get_mode",
    "prisma_appliance_get_intensity",
    "prisma_appliance_get_filename",
):
    assert symbol in header, f"missing declaration {symbol}"
    assert symbol in wrapper, f"missing direct getter {symbol}"
    assert symbol in portal, f"Page 2 API not using {symbol}"

assert '"/api/state"' in portal
assert "'/api/state?ts='+Date.now()" in index
assert "new XMLHttpRequest()" in index
assert "if(pg!==1)" in index
assert "x.timeout=1500" in index
assert "p2Timer=setTimeout(syncPage2,300)" in index

# Page 3 source preview remains available and Page 5 is gone.
assert 'id="sourcePreview"' in index
assert 'id="fsBtn"' in index
assert '"/source/*"' in portal
assert "PAGE 5" not in index
assert "SYNC LAB" not in index
assert "api/debug/state" not in index
assert 'id="d4"' not in index

# Keep the HTTP server conservative for AP stability.
assert "hc.lru_purge_enable=true" in portal
assert "no-store, no-cache, must-revalidate, max-age=0" in portal

print("M4.8 stable OK: 4-page shell + Page 2 direct live sync + Page 3 source preview")
