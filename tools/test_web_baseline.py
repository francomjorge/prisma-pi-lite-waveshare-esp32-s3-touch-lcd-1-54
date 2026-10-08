#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
index = (root / "main/web/index.html").read_text(encoding="utf-8")
style = (root / "main/web/style.css").read_text(encoding="utf-8")
app = (root / "main/web/app.js").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")
portal = (root / "main/wifi_portal.c").read_text(encoding="utf-8")
header = (root / "main/wifi_portal.h").read_text(encoding="utf-8")
main = (root / "main/main.c").read_text(encoding="utf-8")
wrapper = (root / "main/app36.c").read_text(encoding="utf-8")

EXPECTED_PAGES = 4

pages = len(re.findall(r'<section\s+class="page"', index))
dots = len(re.findall(r'id="d\d+"\s+class="dot', index))
assert pages == EXPECTED_PAGES, f"HTML pages={pages}, expected={EXPECTED_PAGES}"
assert dots == EXPECTED_PAGES, f"HTML dots={dots}, expected={EXPECTED_PAGES}"

assert "width:400%" in style, "CSS track width must be 400% for 4 pages"
assert "width:25%" in style, "CSS page width must be 25% for 4 pages"
assert "flex:0 0 25%" in style, "each page must remain exactly one quarter of the track"
assert "Math.min(3,n)" in index, "web navigation upper bound must be page 4"
assert "pg*25" in index, "web transform step must match 4-page layout"

for name in ("index.html", "style.css", "app.js"):
    assert f'"web/{name}"' in cmake, f"{name} is not embedded by active main component"

for route in ('"/"', '"/style.css"', '"/app.js"', '"/api/state"', '"/api/control"', '"/source/*"'):
    assert route in portal, f"missing HTTP route {route}"

for effect_id in range(6):
    assert f'id="fx{effect_id}"' in index, f"missing static effect row fx{effect_id}"
for label in ("PIXEL", "GLITCH", "POSTER", "HUE", "MIRROR", "MIX"):
    assert label in index, f"missing effect label {label}"

for control_id in ("sourceSelect", "power", "intensityBar", "mode"):
    assert f'id="{control_id}"' in index, f"missing page 2 interactive control {control_id}"

assert "fetch('/api/state'" in app, "state polling missing"
assert "fetch('/api/control'" in app, "page 2 control POST missing"
assert "method:'POST'" in app, "page 2 control API is not POST based"
for payload in ("{on:desired}", "{mode:desired}", "{intensity:draftIntensity}", "{source_next:true}"):
    assert payload in app, f"missing page 2 outbound control payload {payload}"

assert "prisma_page2_command_t" in header, "page 2 command contract missing"
assert "prisma_control_take_page2_command" in header, "appliance command consumer declaration missing"
assert "prisma_control_take_page2_command" in portal, "HTTP command queue missing"
assert "prisma_control_take_page2_command" in main, "appliance is not consuming web page 2 commands"
assert "WEB RX" in portal, "serial WEB RX tracing missing"
assert "WEB TX" in portal, "serial WEB TX tracing missing"
assert "WEB APPLY" in main, "serial web-to-appliance apply tracing missing"
assert "APPLIANCE TX" in main, "serial appliance-to-web tracing missing"

assert "effect_values" in app, "web does not consume effect levels"
assert '"effect_values"' in portal or '\\"effect_values\\"' in portal, "API does not expose effect levels"
assert "prisma_control_set_effect_value" in portal, "effect level state setter missing"
assert "prisma_control_set_effect_value" in wrapper, "appliance effect levels are not mirrored to web state"
assert "appendChild(section)" not in app, "pages must remain static; do not inject page 4 dynamically"

print("WEB4 + PAGE2 BIDI OK: 4 static pages, two-way page 2 API, serial tracing, page 4 read-only")
