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

# Single active web source. No external JS dependency in the diagnostic build.
assert '"web/index.html"' in cmake
assert '"web/style.css"' in cmake
assert '"web/app.js"' not in cmake
assert '<script src="/app.js"></script>' not in index
assert '"/app.js"' not in portal
assert '"/api/control"' not in portal

# Direct authoritative appliance state must expose Page2 + all Page4 variables.
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
    assert symbol in portal, f"debug API not using {symbol}"

# Independent diagnostic endpoint and transport instrumentation.
assert '"/api/debug/state"' in portal
assert "SYNC_LAB GET req=" in portal
assert "request_id" in portal
assert "state_seq" in portal
assert "uptime_ms" in portal
assert "free_heap" in portal
assert 'httpd_resp_set_hdr(r,"Connection","close")' in portal
assert "hc.lru_purge_enable=true" in portal
assert "no-store, no-cache, must-revalidate, max-age=0" in portal

# Page 5 is deliberately implemented from scratch using serialized XHR.
for control_id in (
    "dbgBadge","dbgTry","dbgOk","dbgErr","dbgRtt","dbgReq","dbgSeq",
    "dbgUp","dbgHeap","dbgMatch","dbgOn","dbgSource","dbgIntensity",
    "dbgMode","dbgLegacy","dbgRaw",
):
    assert f'id="{control_id}"' in index, f"missing Sync Lab element {control_id}"
for i in range(6):
    assert f'id="dbge{i}s"' in index
    assert f'id="dbge{i}v"' in index

assert "new XMLHttpRequest()" in index
assert "'/api/debug/state?cb='+Date.now()" in index
assert "debugDone(500)" in index
assert "setInterval(debugPoll" not in index, "debug requests must not overlap"
assert "dbgBusy" in index
assert "x.timeout=1800" in index
assert "if(pg===4)scheduleDebug(0)" in index
assert "page=5" in index

print("M4.8 OK: 5-page shell + independent serialized-XHR Sync Lab + direct all-variable appliance state")
