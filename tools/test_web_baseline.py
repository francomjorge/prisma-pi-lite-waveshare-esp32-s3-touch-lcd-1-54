#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
index = (root / "main/web/index.html").read_text(encoding="utf-8")
style = (root / "main/web/style.css").read_text(encoding="utf-8")
app = (root / "main/web/app.js").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")
portal = (root / "main/wifi_portal.c").read_text(encoding="utf-8")
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

for name in ("index.html", "style.css", "app.js"):
    assert f'"web/{name}"' in cmake

for route in ('"/"', '"/style.css"', '"/app.js"', '"/api/state"', '"/source/*"'):
    assert route in portal, f"missing route {route}"

# Diagnostic build deliberately restores the Build 58 one-way contract.
assert '"/api/control"' not in portal, "M4.6 must remain read-only"
assert "fetch('/api/control'" not in app, "M4.6 web must not POST controls"
assert "fetch('/api/state'" in app, "Build58 state polling missing"
assert "setInterval(sync,300)" in app, "Build58 300 ms polling missing"

for control_id in ("power", "pct", "fill", "mode", "filename"):
    assert f'id="{control_id}"' in index, f"missing page2 display {control_id}"

# Critical Build 58 mechanism: main.c is included through aliases so the mirror
# reads the exact appliance variables, then republishes them every 100 ms.
for alias in (
    "#define synth_on prisma_shared_on",
    "#define mode_index prisma_shared_mode",
    "#define intensity prisma_shared_intensity",
    "#define app_main prisma_legacy_main",
):
    assert alias in wrapper, f"missing Build58 alias {alias}"

for call in (
    "prisma_control_set_on(prisma_shared_on)",
    "prisma_control_set_mode(prisma_shared_mode ? 1 : 0)",
    "prisma_control_set_intensity(prisma_shared_intensity)",
    "pdMS_TO_TICKS(100)",
):
    assert call in wrapper, f"missing Build58 mirror operation {call}"

assert "BUILD58 READ-ONLY PAGE2 SYNC ACTIVE" in wrapper
print("M4.6 OK: 4-page shell + exact Build58-style read-only Page2 mirror")
