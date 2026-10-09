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

# V0 navigation remains keyboard-only; LAB controls do not change page navigation.
assert "addEventListener('keydown'" in index
assert "e.key==='ArrowLeft'" in index
assert "e.key==='ArrowRight'" in index
assert "e.repeat" in index
assert "page(pg-1)" in index
assert "page(pg+1)" in index
assert 'class="navLeft"' not in index
assert 'class="navRight"' not in index

# V0 Live View rotation/fullscreen remains intact.
assert "var previewRotation=0" in index
assert "function rotatePreview()" in index
assert "previewRotation=(previewRotation+90)%360" in index
assert "e.key==='r'||e.key==='R'" in index
assert "pg===2" in index
assert "rotate('+previewRotation+'deg)" in index
assert "fsFrame.clientHeight+'px'" in index
assert "fsFrame.clientWidth+'px'" in index
assert "requestAnimationFrame(applyPreviewRotation)" in index
assert "R ROTATE" in index
assert "function toggleFs()" in index
assert "fsButton.addEventListener('click',toggleFs)" in index

# LAB fullscreen is reversible from F and the overlay button, with pseudo fullscreen fallback.
assert 'id="labFsBtn"' in index
assert 'aria-label="Toggle FX Lab fullscreen"' in index
assert "function toggleLabFs()" in index
assert "labFsButton.addEventListener('click',toggleLabFs)" in index
assert "realFsElement()===labFrame" in index
assert "labFrame.classList.contains('pseudoFull')" in index
assert "pg===4" in index
assert "toggleLabFs();return" in index
assert "labFsButton.textContent=active?'×':'⛶'" in index
assert ".labFrame.pseudoFull{" in index
assert ".labFrame:fullscreen" in index
assert "F FULLSCREEN" in index

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

# Existing V0 pages still render from the same shared snapshot/source.
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

# Page 5 FX Lab remains browser-only and reuses the already-loaded Live View image object.
assert 'class="page labPage"' in index
assert 'id="labCanvas"' in index
assert 'id="labPipeline"' in index
assert 'id="labReverse"' in index
assert 'id="d4"' in index
assert "function renderLabPreview()" in index
assert "function renderLabPipeline(s)" in index
assert "labFxNames=['PIXEL','GLITCH','POSTER','HUE','MIRROR','MIX']" in index
assert "labFxOrder=[0,1,2,3,4,5]" in index
assert "function reverseLabFx()" in index
assert "function moveLabFx(fx,dir)" in index
assert "draggable=true" in index
assert "prismaFxLabOrderV0" in index
assert "BROWSER GRAPHICS PIPELINE" in index
assert "ORDER = PROCESSING ORDER" in index
assert "MIX IS GLOBAL / FIXED LAST" in index
assert "new Image(" not in index
assert "fetch('/source/" not in index
assert "XMLHttpRequest" in index
assert "api/debug/state" not in index
assert '"/api/debug/state"' not in portal

# FX ENGINE V2: native A/B/scratch pipeline with PIXEL + GLITCH active in reorderable sequence.
assert "FX ENGINE V2" in index
assert "fxBufferA=document.createElement('canvas')" in index
assert "fxBufferB=document.createElement('canvas')" in index
assert "fxScratch=document.createElement('canvas')" in index
assert "version:'2'" in index
assert "0:{id:'pixel',process:processPixelFx}" in index
assert "1:{id:'glitch',process:processGlitchFx}" in index
assert "2:{id:'poster',process:null}" in index
assert "3:{id:'hue',process:null}" in index
assert "4:{id:'mirror',process:null}" in index
assert "fxEngine.resize(w,h)" in index
assert "mixGain:function(v){return clamp100(v)/50}" in index

# PIXEL V1 remains unchanged.
assert "function pixelBlockSize(v)" in index
assert "Math.pow(n/100,2)*63" in index
assert "function processPixelFx(src,dst,value,context)" in index
assert "effective=clamp100(value*context.mixGain)" in index
assert "imageSmoothingEnabled=false" in index

# GLITCH V1 is browser-native slice displacement + color ghost, using the same MIX gain.
assert "function glitchHash(n)" in index
assert "function processGlitchFx(src,dst,value,context)" in index
assert "effective=clamp100(value*context.mixGain)" in index
assert "bands=2+Math.round(strength*16)" in index
assert "maxShift=Math.max(1,Math.round(src.width*(.004+.075*strength)))" in index
assert "dctx.rect(0,sy,dst.width,bh)" in index
assert "globalCompositeOperation='screen'" in index
assert "hue-rotate(95deg) saturate(180%)" in index
assert "hue-rotate(265deg) saturate(180%)" in index

# The same A/B pipeline applies processors in LAB order and rerenders immediately when reordered.
assert "function renderRotatedSource(" in index
assert "function runFxPipeline(s)" in index
assert "for(var i=0;i<labFxOrder.length-1;i++)" in index
assert "node.process(src,dst,labEffectValue(s,fx)" in index
assert "var output=runFxPipeline(latestState)" in index
assert "renderLabPipeline(latestState);renderLabPreview()" in index

# MIX remains global metadata, normalized to final slot, never draggable/reorderable.
assert "function normalizeLabOrder(order)" in index
assert "out.push(5)" in index
assert "if(fx===5)return" in index
assert "slice(0,-1).reverse()" in index
assert "function appendLabMixBlock(s)" in index
assert "block.draggable=false" in index
assert "block.dataset.fx='5'" in index
assert "appendLabMixBlock(s)" in index

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

print("V0 protected + FX Engine V2 OK: PIXEL + ordered GLITCH + reversible LAB fullscreen")
