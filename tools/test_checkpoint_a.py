#!/usr/bin/env python3
from pathlib import Path
import ast
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
index=(root/'main/web/index.html').read_text(encoding='utf-8')
cmake=(root/'main/CMakeLists.txt').read_text(encoding='utf-8')
app=(root/'main/app_checkpoint_a.c').read_text(encoding='utf-8')
portal=(root/'main/wifi_portal_checkpoint_a.c').read_text(encoding='utf-8')
legacy_portal=(root/'main/wifi_portal.c').read_text(encoding='utf-8')

# Physical appliance remains the physically validated six-page checkpoint.
assert '"app_checkpoint_a.c"' in cmake and '"wifi_portal_checkpoint_a.c"' in cmake
assert '#include "main.c"' in app and '#include "wifi_portal.c"' in portal
assert '#define CP_PAGE_COUNT 6' in app
assert 'int xs[6]={76,92,108,124,140,156}' in app
assert 'cp_page_fx_lab' in app and 'cp_page_motion' in app
assert 'page==1||page==3||page==5' in app
assert '#define MOTION_BAR_X 110' in app and '#define MOTION_BAR_W 84' in app
for name in ('PULSE','DRIFT','ROTATE','WAVE','SHAKE','FEEDBACK'):
    assert f'"{name}"' in app

# One state endpoint / one existing browser poll carries static and motion state.
assert 'motionEffects' in portal and 'motionValues' in portal
assert '"/api/motion"' not in portal
assert "'/api/state?ts='+Date.now()" in index
assert index.count("'/api/state?ts='+Date.now()") == 1
assert 'prisma_admin_register' in portal and '"/source/*"' in legacy_portal

# Six-page web and unified movable pipeline, MIX alone fixed.
assert "Math.min(5,n)" in portal and "pg*(100/6)" in portal
assert "labFxNames=['PIXEL','GLITCH','POSTER','HUE','MIRROR','MIX','PULSE','DRIFT','ROTATE','WAVE','SHAKE','FEEDBACK']" in portal
assert "allowed=[0,1,2,3,4,6,7,8,9,10,11]" in portal
assert "out.push(5)" in portal
assert "appendLabMixBlock(s,fixedRow)" in portal
assert "appendLabMirrorBlock(s,fixedRow)" not in portal
assert "if(from===5||fx===5)return" in portal
assert "var swap=labFxOrder[fromAt]" in portal
assert "oldReverseButton.cloneNode(true)" in portal

# Pipeline category colors: MIX-controlled static FX are purple, motion FX are orange.
assert '.labOrderRow .labBlock[data-fx=\\"0\\"]' in portal
assert '.labOrderRow .labBlock[data-fx=\\"4\\"]' in portal
assert '.labFixedRow .labBlock[data-fx=\\"5\\"]' in portal
assert 'border-color:#af37ff!important' in portal
assert '.labOrderRow .labBlock[data-fx=\\"6\\"]' in portal
assert '.labOrderRow .labBlock[data-fx=\\"11\\"]' in portal
assert 'border-color:#ff7819!important' in portal
assert 'PURPLE = MIXED FX · ORANGE = MOTION' in portal

# FX Lab frame still exists but visually merges into the page background.
assert '.labFrame{background:#04070c!important;border-color:#04070c!important;box-shadow:none!important}' in portal
assert '.labFrame .labFsBtn{opacity:0!important' in portal

# Existing static algorithms stay untouched in index.html.
for fn in ('processPixelFx','processGlitchFx','processPosterFx','processHueFx','processMirrorFx'):
    assert fn in index
    assert fn not in portal

# Motion V1: six real browser processors, ordered together with static nodes.
for fn in ('processPulseMotion','processDriftMotion','processRotateMotion','processWaveMotion','processShakeMotion','processFeedbackMotion'):
    assert f'function {fn}' in portal
assert "6:{id:'pulse',process:processPulseMotion}" in portal
assert "11:{id:'feedback',process:processFeedbackMotion}" in portal
assert "var node=fx<=4?fxEngine.registry[fx]:motionRegistry[fx]" in portal
assert "node.process(src,dst,labEffectValue(s,fx)" in portal

# MIX is supplied to the existing static engine context; motion processors do not read mixGain.
for block in re.findall(r'function process(?:Pulse|Drift|Rotate|Wave|Shake|Feedback)Motion\(.*?\n\}', portal, re.S):
    assert 'mixGain' not in block
assert 'mixGain=fxEngine.mixGain(labEffectValue(s,5))' in portal

# Animation loop is browser-only, throttled to about 30 fps and active only while LAB is visible.
assert 'requestAnimationFrame(motionTick)' in portal
assert 'ts-motionLastPaint>=33' in portal
assert 'anyMotionActive(latestState)&&labIsVisible()' in portal
assert 'MOTION FX / READ ONLY · NO MIX · BROWSER PROCESSING' in portal

# Feedback keeps its own browser canvas and resets on size/source/long gaps.
assert "motionFeedback=document.createElement('canvas')" in portal
assert 'motionFeedback.width!==dst.width' in portal
assert 'currentSource!==motionFeedbackSource' in portal
assert 'context.time-motionFeedbackLast>250' in portal

# Reconstruct injected JS and validate syntax.
match=re.search(r'static const char checkpoint_web_injection\[\]\s*=\s*(.*?);\n\nstatic esp_err_t checkpoint_page_get',portal,re.S)
assert match
literals=re.findall(r'"(?:\\.|[^"\\])*"',match.group(1))
injected=''.join(ast.literal_eval(x) for x in literals)
script_match=re.search(r'<script>(.*?)</script>',injected,re.S)
assert script_match
with tempfile.NamedTemporaryFile('w',suffix='.js',encoding='utf-8',delete=False) as tmp:
    tmp.write(script_match.group(1)); js_path=tmp.name
subprocess.run(['node','--check',js_path],check=True)
Path(js_path).unlink(missing_ok=True)

print('Motion V1 protected: purple static/MIX, orange motion, six browser processors, unified order')
