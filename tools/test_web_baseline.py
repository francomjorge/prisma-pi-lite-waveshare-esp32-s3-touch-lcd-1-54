#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
index = (root / "main/web/index.html").read_text(encoding="utf-8")
style = (root / "main/web/style.css").read_text(encoding="utf-8")
app = (root / "main/web/app.js").read_text(encoding="utf-8")
cmake = (root / "main/CMakeLists.txt").read_text(encoding="utf-8")
portal = (root / "main/wifi_portal.c").read_text(encoding="utf-8")

EXPECTED_PAGES = 3

pages = len(re.findall(r'<section\s+class="page"', index))
dots = len(re.findall(r'id="d\d+"\s+class="dot', index))
assert pages == EXPECTED_PAGES, f"HTML pages={pages}, expected={EXPECTED_PAGES}"
assert dots == EXPECTED_PAGES, f"HTML dots={dots}, expected={EXPECTED_PAGES}"

assert "width:300%" in style, "CSS track width must be 300% for 3 pages"
assert "width:33.3333%" in style, "CSS page width must be 33.3333% for 3 pages"
assert "Math.min(2,n)" in index, "web navigation upper bound must be page 3"
assert "pg*33.3333" in index, "web transform step must match 3-page layout"

for name in ("index.html", "style.css", "app.js"):
    assert f'"web/{name}"' in cmake, f"{name} is not embedded by active main component"

for route in ('"/"', '"/style.css"', '"/app.js"', '"/api/state"', '"/source/*"'):
    assert route in portal, f"missing HTTP route {route}"

assert "fetch('/api/state'" in app, "read-only state polling missing"
assert "appendChild(section)" not in app, "pages must not be injected dynamically"
assert "effectsPage" not in app, "page 4 is not part of the stable 3-page baseline"

print("WEB BASELINE OK: 3 pages, matching layout/navigation, explicit embedded assets")
