#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
index=(root/'main/web/index.html').read_text(encoding='utf-8')
style=(root/'main/web/style.css').read_text(encoding='utf-8')
cmake=(root/'main/CMakeLists.txt').read_text(encoding='utf-8')
app=(root/'main/app_checkpoint_a.c').read_text(encoding='utf-8')
portal=(root/'main/wifi_portal_checkpoint_a.c').read_text(encoding='utf-8')
legacy_portal=(root/'main/wifi_portal.c').read_text(encoding='utf-8')

# Build only the checkpoint wrappers; legacy sources remain untouched for rollback/reference.
assert '"app_checkpoint_a.c"' in cmake
assert '"wifi_portal_checkpoint_a.c"' in cmake
assert '"app36.c"' not in cmake
assert '"wifi_portal.c"' not in cmake.split('SRCS',1)[1].split('INCLUDE_DIRS',1)[0]
assert '#include "main.c"' in app
assert '#include "wifi_portal.c"' in portal

# Physical appliance grows from 4 to 6 pages without modifying legacy main.c.
assert '#define CP_PAGE_COUNT 6' in app
assert 'int xs[6]={76,92,108,124,140,156}' in app
assert 'cp_page_fx_lab' in app
assert '"FX LAB"' in app and '"OPEN IN BROWSER"' in app
assert 'cp_page_motion' in app
assert 'page==4' in app and 'page==5' in app
assert 'page==1||page==3||page==5' in app

# Six independent motion controls: no MIX and no browser animation processing yet.
for name in ('PULSE','DRIFT','ROTATE','WAVE','SHAKE','FEEDBACK'):
    assert f'"{name}"' in app
assert 'MOTION_COUNT 6' in app
assert 'motion_enabled[MOTION_COUNT]' in app
assert 'motion_value[MOTION_COUNT]' in app
assert 'prisma_appliance_get_motion_effect' in app
assert 'prisma_appliance_get_motion_value' in app
assert '#define MOTION_BAR_X 110' in app
assert '#define MOTION_BAR_W 84' in app
assert 'rect(MOTION_BAR_X' in app
assert 'startx<MOTION_BAR_X' in app
assert 'requestAnimationFrame' not in portal
assert 'processMotion' not in portal
assert 'MOTION FX / READ ONLY' in portal
assert 'NO MIX' in portal

# The same /api/state snapshot carries static + motion state; no second poll/endpoint.
assert 'motionEffects' in portal
assert 'motionValues' in portal
assert 'char b[1024]' in portal
assert '.handler=checkpoint_state_get' in portal
assert '"/api/state"' in portal
assert '"/api/motion"' not in portal
assert "'/api/state?ts='+Date.now()" in index
assert index.count("'/api/state?ts='+Date.now()") == 1

# Web stays based on physically validated 5-page ALLFX UI and receives page 6 at serve time.
assert index.count('<section class="page') == 5
assert "var sec=document.createElement('section')" in portal
assert "sec.className='page motionPage'" in portal
assert "Math.min(5,n)" in portal
assert "pg*(100/6)" in portal
assert "width:600%!important" in portal
assert "16.6666667%" in portal
assert "document.getElementById('d5')" in portal
assert "drawMotion=function" in portal

# Requested FX Lab polish is injected without changing effect algorithms.
assert '.labFrame .labFsBtn{opacity:0!important' in portal
assert '.labFrame:hover .labFsBtn' in portal
assert "name.textContent='MIRROR'" in portal
assert "name.textContent='MIX'" in portal
assert "textContent='FIXED'" not in portal
assert "textContent='END'" not in portal
assert 'processPixelFx' in index and 'processGlitchFx' in index
assert 'processPosterFx' in index and 'processHueFx' in index and 'processMirrorFx' in index

# Existing backend/source/admin behavior remains inherited from legacy portal.
assert '"/source/*"' in legacy_portal
assert 'prisma_admin_register' in portal
assert 'hc.lru_purge_enable=true' in portal
assert 'no_cache(r)' in portal

print('Checkpoint A protected: 6 appliance pages + web page 6 motion state, no motion processing')
