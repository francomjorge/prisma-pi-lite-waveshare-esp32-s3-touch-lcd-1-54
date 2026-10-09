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
version = (root / "VERSION_5.0.txt").read_text(encoding="utf-8")

EXPECTED_PAGES = 5
pages = len(re.findall(r'<section\s+class="page(?:\s|\")', index))
dots = len(re.findall(r'id="d\d+"\s+class="dot', index))
assert pages == EXPECTED_PAGES, f"HTML pages={pages}, expected={EXPECTED_PAGES}"
assert dots == EXPECTED_PAGES, f"HTML dots={dots}, expected={EXPECTED_PAGES}"
assert "width:500%" in style
assert "width:20%" in style
assert "flex:0 0 20%" in style
assert "Math.min(4,n)" in index
assert "pg*20" in index
assert "for(var i=0;i<5;i++)" in index

assert '"web/index.html"' in cmake
assert '"web/style.css"' in cmake
assert '"web/app.js"' not in cmake
assert '<script src="/app.js"></script>' not in index
assert '"/app.js"' not in portal
assert '"/api/control"' not in portal

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

assert "'/api/state?ts='+Date.now()" in index
assert "new XMLHttpRequest()" in index
assert "syncBusy" in index
assert "scheduleSync(300)" in index
assert "if(pg!==1)" not in index
assert "setInterval(syncAll" not in index

assert "function drawPage2(s)" in index
assert "function drawPage4(s)" in index
assert "function drawPage5(s)" in index
assert "function drawAll(s){drawPage2(s);drawPage4(s);drawPage5(s)}" in index
assert "Array.isArray(s.effects)" in index
assert "Array.isArray(s.values)" in index
assert "fxmirror" in index

assert 'id="sourcePreview"' in index
assert 'id="fsBtn"' in index
assert '"/source/*"' in portal
assert "setSource(s.filename)" in index
assert "photo.classList.toggle('off',!s.on)" in index

assert ".photo.off{background:#000!important}" in style
assert ".photo.off #offscreen{display:none!important}" in style
assert ".photo.off canvas,.photo.off .sourcePreview{visibility:hidden!important;opacity:0!important}" in style

# Existing fullscreen control remains untouched on the original Live View.
assert ".photo .fsBtn{" in style
assert "opacity:0" in style
assert ".photo .fsBtn::after{content:\"⛶\"" in style
assert ".photo:hover .fsBtn" in style
assert ".photo:focus-within .fsBtn" in style

assert "PRISMA-PI LITE 5.0" in version
assert "STABLE" in version

# Page 5 is an isolated browser FX lab. Debug API and timers remain forbidden.
assert "PAGE 5" in index
assert "FX LAB / TEST LIVE VIEW" in index
assert 'id="d4"' in index
assert "api/debug/state" not in index
assert '"/api/debug/state"' not in portal
assert "esp_timer" not in cmake

assert "hc.lru_purge_enable=true" in portal
assert "no-store, no-cache, must-revalidate, max-age=0" in portal

print("Controller OK: original surfaces + page 5 isolated FX lab + global sync")
