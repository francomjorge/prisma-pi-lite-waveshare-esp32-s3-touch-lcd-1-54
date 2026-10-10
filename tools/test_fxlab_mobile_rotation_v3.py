#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
v3 = (root / 'main' / 'wifi_portal_layout_v3.c').read_text(encoding='utf-8')
cmake = (root / 'main' / 'CMakeLists.txt').read_text(encoding='utf-8')

assert '"wifi_portal_layout_v3.c"' in cmake
assert '#include "wifi_portal_layout_v2.c"' in v3

# V3 must patch inside the V2 IIFE so it can reuse the selected-layer state.
assert 'v2RenderList();v2Paint();\\n})();' in v3
assert 'fxlab_v3_inner_patch' in v3
assert 'buf == fxlab_layout_v2_injection' in v3

# R rotates only the selected FX Lab layer in 90-degree steps.
assert "if(pg!==4||!v2Selected" in v3
assert "ev.key!=='r'&&ev.key!=='R'" in v3
assert "layer.rotation=(((Number(layer.rotation)||0)+90)%360+360)%360" in v3
assert 'v3EffectiveLayerRotation' in v3
assert "ctx.rotate(g.rot*Math.PI/180)" in v3

# Desktop keeps the previous fullscreen implementation; iOS uses pseudo fullscreen.
assert 'var v3DesktopToggleLabFs=toggleLabFs' in v3
assert 'if(!v3UseMobilePseudoFull())return v3DesktopToggleLabFs()' in v3
assert '/iPhone|iPad|iPod/i.test(ua)' in v3
assert "labFrame.classList.add('pseudoFull')" in v3
assert '100dvw' in v3 and '100dvh' in v3
assert "window.addEventListener('orientationchange'" in v3
assert 'window.visualViewport' in v3

print('FX Lab V3 protected: desktop fullscreen preserved, iOS pseudo fullscreen, selected-layer R rotation')
