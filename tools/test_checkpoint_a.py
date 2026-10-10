#!/usr/bin/env python3
from pathlib import Path
import ast
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
index=(root/'main/web/index.html').read_text(encoding='utf-8')
cmake=(root/'main/CMakeLists.txt').read_text(encoding='utf-8')
base_app=(root/'main/app_checkpoint_a.c').read_text(encoding='utf-8')
app=(root/'main/app_local_processing.c').read_text(encoding='utf-8')
portal=(root/'main/wifi_portal_checkpoint_a.c').read_text(encoding='utf-8')
intensity_portal=(root/'main/wifi_portal_intensity.c').read_text(encoding='utf-8')
local_portal=(root/'main/wifi_portal_local_processing.c').read_text(encoding='utf-8')
legacy_portal=(root/'main/wifi_portal.c').read_text(encoding='utf-8')

# The validated six-page implementation remains present; active build layers page 7 on top.
assert '"app_local_processing.c"' in cmake and '"wifi_portal_local_processing.c"' in cmake
assert '"app_checkpoint_a.c"' not in cmake.split('SRCS',1)[1].split('INCLUDE_DIRS',1)[0]
assert '"wifi_portal_intensity.c"' not in cmake.split('SRCS',1)[1].split('INCLUDE_DIRS',1)[0]
assert '#include "main.c"' in base_app and '#include "main.c"' in app
assert '#include "wifi_portal.c"' in portal
assert '#include "wifi_portal_checkpoint_a.c"' in intensity_portal
assert '#include "wifi_portal_intensity.c"' in local_portal
assert '#define CP_PAGE_COUNT 7' in app
assert 'int xs[7]={68,84,100,116,132,148,164}' in app
assert 'cp_page_fx_lab' in app and 'cp_page_motion' in app and 'cp_page_local_processing' in app
assert 'page==1||page==3||page==5||page==6' in app
assert '#define MOTION_BAR_X 110' in app and '#define MOTION_BAR_W 84' in app
for name in ('PULSE','DRIFT','ROTATE','WAVE','SHAKE','FEEDBACK'):
    assert f'"{name}"' in app

# One authoritative browser poll still carries all state; local state is appended to /api/state.
assert 'motionEffects' in portal and 'motionValues' in portal
assert 'localProcessing' in local_portal
assert 'local_httpd_resp_send_proxy' in local_portal
assert '"/api/motion"' not in portal and '"/api/local' not in local_portal
assert index.count("'/api/state?ts='+Date.now()") == 1
assert 'prisma_admin_register' in portal and '"/source/*"' in legacy_portal

# Base six-page web remains untouched; local wrapper promotes it to seven pages.
assert "Math.min(5,n)" in portal and "pg*(100/6)" in portal
assert "Math.min(6,n)" in local_portal and "pg*(100/7)" in local_portal
assert '.track{width:700%!important}' in local_portal
assert "d.id='d6'" in local_portal
assert "labFxNames=['PIXEL','GLITCH','POSTER','HUE','MIRROR','MIX','PULSE','DRIFT','ROTATE','WAVE','SHAKE','FEEDBACK']" in portal
assert "allowed=[0,1,2,3,4,6,7,8,9,10,11]" in portal
assert "out.push(5)" in portal
assert "appendLabMixBlock(s,fixedRow)" in portal
assert "appendLabMirrorBlock(s,fixedRow)" not in portal
assert "if(from===5||fx===5)return" in portal
assert "var swap=labFxOrder[fromAt]" in portal
assert "oldReverseButton.cloneNode(true)" in portal

# Pipeline category colors: static/MIX purple, Motion orange, Local fixed yellow.
assert 'border-color:#af37ff!important' in portal
assert 'border-color:#ff7819!important' in portal
assert 'border-color:#f5be23!important' in local_portal
assert 'labLocalRow' in local_portal and 'block.draggable=false' in local_portal
assert "names=['MONO','CONTRAST','NEGATIVE']" in local_portal
assert "labPipeline.insertBefore(row,labPipeline.firstChild)" in local_portal

# FX Lab frame and existing browser processors remain untouched.
assert '.labFrame{background:#04070c!important;border-color:#04070c!important;box-shadow:none!important}' in portal
for fn in ('processPixelFx','processGlitchFx','processPosterFx','processHueFx','processMirrorFx'):
    assert fn in index and fn not in portal
for fn in ('processPulseMotion','processDriftMotion','processRotateMotion','processWaveMotion','processShakeMotion','processFeedbackMotion'):
    assert f'function {fn}' in portal
assert "var node=fx<=4?fxEngine.registry[fx]:motionRegistry[fx]" in portal
assert 'requestAnimationFrame(motionTick)' in portal
assert 'ts-motionLastPaint>=33' in portal

# Local Processing Part A is state/UI only: there is deliberately no local image processor yet.
for forbidden in ('processMonoLocal','processContrastLocal','processNegativeLocal','esp_new_jpeg','jpeg_decode','jpeg_encode'):
    assert forbidden not in app
    assert forbidden not in local_portal

# Validate injected browser JavaScript syntax for both layers.
def check_injected(path_text, pattern):
    match=re.search(pattern,path_text,re.S);assert match
    literals=re.findall(r'"(?:\\.|[^"\\])*"',match.group(1));assert literals
    injected=''.join(ast.literal_eval(x) for x in literals)
    script_match=re.search(r'<script>(.*?)</script>',injected,re.S);assert script_match
    with tempfile.NamedTemporaryFile('w',suffix='.js',encoding='utf-8',delete=False) as tmp:
        tmp.write(script_match.group(1)); js_path=tmp.name
    subprocess.run(['node','--check',js_path],check=True)
    Path(js_path).unlink(missing_ok=True)

check_injected(portal,r'static const char checkpoint_web_injection\[\]\s*=\s*(.*?);\n\nstatic esp_err_t checkpoint_page_get')
check_injected(local_portal,r'static const char local_processing_web_injection\[\]\s*=\s*(.*?);\n\nstatic esp_err_t local_httpd_resp_send_proxy')

print('Checkpoint protected: seven pages with Local Processing state/UI layered over validated FX/Motion architecture')
