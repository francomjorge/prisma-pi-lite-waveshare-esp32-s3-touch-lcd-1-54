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

# V0 desktop navigation remains keyboard-only with the same behavior.
assert "addEventListener('keydown'" in index
assert "e.key==='ArrowLeft'" in index
assert "e.key==='ArrowRight'" in index
assert "e.repeat" in index
assert "page(pg-1)" in index
assert "page(pg+1)" in index
assert "isRealFs()||fsFrame.classList.contains('pseudoFull')" in index
assert 'class="navLeft"' not in index
assert 'class="navRight"' not in index

# V0 Live View rotation/fullscreen stays intact.
assert "var previewRotation=0" in index
assert "function rotatePreview()" in index
assert "previewRotation=(previewRotation+90)%360" in index
assert "e.key==='r'||e.key==='R'" in index
assert "pg===2" in index
assert "rotate('+previewRotation+'deg)" in index
assert "fsFrame.clientHeight+'px'" in index
assert "fsFrame.clientWidth+'px'" in index
assert "requestAnimationFrame(applyPreviewRotation)" in index
assert "window.addEventListener('resize',applyPreviewRotation)" in index
assert "R ROTATE" in index
assert "e.key==='f'||e.key==='F'" in index
assert "toggleFs();return" in index
assert "F FULLSCREEN" in index
assert "pg===2||isRealFs()||fsFrame.classList.contains('pseudoFull')" in index

# Active firmware/backend contract is unchanged.
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
assert index.count("'/api/state?ts='+Date.now()") == 1
assert "new XMLHttpRequest()" in index
assert "syncBusy" in index
assert "scheduleSync(300)" in index
assert "setInterval(syncAll" not in index

# Existing V0 pages still render from the same shared snapshot.
assert "function drawPage2(s)" in index
assert "function drawPage4(s)" in index
assert "function drawAll(s){drawPage2(s);drawPage4(s);drawLab(s)}" in index
assert "Array.isArray(s.effects)" in index
assert "Array.isArray(s.values)" in index
assert "fxmirror" in index
assert 'id="sourcePreview"' in index
assert 'id="fsBtn"' in index
assert '"/source/*"' in portal
assert "setSource(s.filename)" in index
assert "photo.classList.toggle('off',!s.on)" in index

# Page 5 FX Lab is browser-only. It reuses the already-loaded Live View image object.
assert 'class="page labPage"' in index
assert 'id="labCanvas"' in index
assert 'id="labPipeline"' in index
assert 'id="labReverse"' in index
assert 'id="d4"' in index
assert "function renderLabPreview()" in index
assert "ctx.drawImage(sourcePreview,0,0,w,h)" in index
assert "function renderLabPipeline(s)" in index
assert "labFxNames=['PIXEL','GLITCH','POSTER','HUE','MIRROR','MIX']" in index
assert "labFxOrder=[0,1,2,3,4,5]" in index
assert "function reverseLabFx()" in index
assert "function moveLabFx(fx,dir)" in index
assert "draggable=true" in index
assert "prismaFxLabOrderV0" in index
assert "BROWSER GRAPHICS PIPELINE" in index
assert "ORDER = PROCESSING ORDER" in index
assert "LAB ORDER IS BROWSER ONLY" in index
assert "new Image(" not in index
assert "fetch('/source/" not in index
assert "XMLHttpRequest" in index
assert "api/debug/state" not in index
assert '"/api/debug/state"' not in portal

assert ".photo.off{background:#000!important}" in style
assert ".photo.off #offscreen{display:none!important}" in style
assert ".photo.off canvas,.photo.off .sourcePreview{visibility:hidden!important;opacity:0!important}" in style
assert ".photo .fsBtn{" in style
assert "opacity:0" in style
assert ".photo .fsBtn::after{content:\"⛶\"" in style
assert ".photo:hover .fsBtn" in style
assert ".photo:focus-within .fsBtn" in style
assert ".labFrame{" in style
assert ".labPipeline{" in style
assert ".labBlock{" in style

assert "PRISMA-PI LITE 5.0" in version
assert "NO_WF" in version
assert "NO_ADMIN" in version
assert "STABLE" in version

# v1.1 Admin remains compiled exactly as before; FX Lab adds no Admin/background work.
assert "esp_timer" in cmake
assert "hc.lru_purge_enable=true" in portal
assert "no-store, no-cache, must-revalidate, max-age=0" in portal

print("V0 controller protected + page 5 FX Lab OK: shared image object + active reorderable browser-only pipeline")
