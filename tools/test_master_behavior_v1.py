#!/usr/bin/env python3
from pathlib import Path
import ast
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
portal=(root/'main/wifi_portal_checkpoint_a.c').read_text(encoding='utf-8')
app=(root/'main/app_checkpoint_a.c').read_text(encoding='utf-8')
index=(root/'main/web/index.html').read_text(encoding='utf-8')

# Appliance state semantics remain unchanged: mode 0 RANDOM, mode 1 BYPASS.
assert 'prisma_shared_mode?"BYPASS":"RANDOM"' in app
assert 'prisma_shared_mode=prisma_shared_mode?0:1' in app
assert 'prisma_shared_mode=0;prisma_shared_intensity=61' in app

# Power only gates output; it must not reset FX, Motion, MIX, source or values.
power_block=app.split('else if(page==1&&startx>=170',1)[1].split('else if(page==1&&startx>=12',1)[0]
assert 'toggle_power()' in power_block
for forbidden in ('effect_enabled','effect_value','motion_enabled','motion_value','prisma_storage_next','prisma_shared_mode=','prisma_shared_intensity='):
    assert forbidden not in power_block

# OFF is true black in both Live View and FX Lab, not dimmed/greyed.
assert '.photo.off{background:#000!important}' in portal
assert '.photo.off .sourcePreview{visibility:hidden!important;opacity:0!important}' in portal
assert '.labFrame.off{background:#000!important}' in portal
assert '.labFrame.off canvas{visibility:hidden!important;opacity:0!important}' in portal
assert 'if(!s||!s.on){motionFeedbackValid=false;motionCtx(dst);return dst;}' in portal

# BYPASS returns the rotated source buffer before any static or Motion processor.
assert 'if(s.mode){motionFeedbackValid=false;return src;}' in portal
run_block=portal.split('runFxPipeline=function(s){',1)[1].split('function anyMotionActive',1)[0]
assert run_block.index('if(s.mode)') < run_block.index('for(var i=0;i<labFxOrder.length;i++)')

# RANDOM keeps the complete configured pipeline and MIX remains static-only.
assert 'var node=fx<=4?fxEngine.registry[fx]:motionRegistry[fx]' in run_block
assert 'node.process(src,dst,labEffectValue(s,fx)' in run_block
assert 'mixGain=fxEngine.mixGain(labEffectValue(s,5))' in run_block
assert 'if(fx===5||!labEffectEnabled(s,fx))continue' in run_block

# Motion animation must stop while OFF or BYPASS is active.
assert 'function anyMotionActive(s){if(!s||!s.on||s.mode)return false;' in portal
assert 'requestAnimationFrame(motionTick)' in portal

# Fullscreen controls are activity-driven, not permanently visible because of :hover.
assert '.photo.fsControlsVisible .fsBtn,.labFrame.fsControlsVisible .labFsBtn{opacity:1!important;pointer-events:auto!important}' in portal
assert '.photo:hover .fsBtn' in portal and 'opacity:0!important;pointer-events:none!important' in portal
assert 'function bindFsActivity(frame)' in portal
assert "frame.addEventListener('pointermove',show" in portal
assert "timer=setTimeout(hide,1100)" in portal
assert 'bindFsActivity(fsFrame);bindFsActivity(labFrame);' in portal

# No second state endpoint/poll introduced.
assert '"/api/motion"' not in portal
assert index.count("'/api/state?ts='+Date.now()") == 1

# Validate the injected JavaScript syntax exactly as served.
match=re.search(r'static const char checkpoint_web_injection\[\]\s*=\s*(.*?);\n\nstatic esp_err_t checkpoint_page_get',portal,re.S)
assert match
literals=re.findall(r'"(?:\\.|[^"\\])*"',match.group(1))
assert literals
injected=''.join(ast.literal_eval(x) for x in literals)
script_match=re.search(r'<script>(.*?)</script>',injected,re.S)
assert script_match
with tempfile.NamedTemporaryFile('w',suffix='.js',encoding='utf-8',delete=False) as tmp:
    tmp.write(script_match.group(1)); path=tmp.name
subprocess.run(['node','--check',path],check=True)
Path(path).unlink(missing_ok=True)

print('Master behavior V1 protected: OFF black, BYPASS clean source, RANDOM pipeline, activity fullscreen controls')
