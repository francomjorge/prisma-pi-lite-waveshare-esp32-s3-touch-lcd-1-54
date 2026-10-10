#!/usr/bin/env python3
from pathlib import Path
import ast
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
app=(root/'main/app_checkpoint_a.c').read_text(encoding='utf-8')
portal=(root/'main/wifi_portal_checkpoint_a.c').read_text(encoding='utf-8')
wrapper=(root/'main/wifi_portal_intensity.c').read_text(encoding='utf-8')
cmake=(root/'main/CMakeLists.txt').read_text(encoding='utf-8')

# Appliance keeps the validated four-button 0 / 1 / 2 / 3 UI and 3-color cycle.
for token in ('INTENSITY_BUTTON_X','INTENSITY_BUTTON_Y','INTENSITY_BUTTON_W','INTENSITY_BUTTON_H','INTENSITY_BUTTON_GAP'):
    assert token in app
assert 'static int intensity_button = 0;' in app
assert 'static int intensity_level = 0;' in app
assert 'if(intensity_button<=0)return 0;' in app
assert 'return ((intensity_button-1)*3)+intensity_level+1;' in app
assert 'intensity_level=(intensity_level+1)%3;' in app
assert 'else if(intensity_level==1)box=yellow;' in app
assert 'else box=red;' in app
assert "char label[2]={(char)('0'+i),0};" in app
assert 'prisma_appliance_get_intensity(void){return cp_intensity_state();}' in app

# State 0 is FREE semantically but must be a true no-op for FX/Motion values.
apply_block=app.split('static void cp_apply_intensity_preset(int state)',1)[1].split('static void cp_select_intensity_button',1)[0]
assert 'if(state<=0||state>9)return;' in apply_block
assert apply_block.index('if(state<=0||state>9)return;') < apply_block.index('effect_enabled[i]=p->fx_enabled[i]')

# Nine curated presets exist and progressively raise glitch / shake / feedback.
assert 'static const cp_intensity_preset_t intensity_presets[9]' in app
for label in ('/* 1G */','/* 1Y */','/* 1R */','/* 2G */','/* 2Y */','/* 2R */','/* 3G */','/* 3Y */','/* 3R */'):
    assert label in app
for expected in (
    '{18, 6,15,12,50,45}',
    '{30,16,28,30,50,52}',
    '{50,43,50,64,50,68}',
    '{66,68,68,85,50,84}',
    '{78,86,80,96,50,95}',
    '{60,48,38,56,30,62}',
    '{74,62,52,74,46,86}',
):
    assert expected in app

# Applying a preset owns both static FX and Motion state; MIX remains static endpoint.
assert 'effect_enabled[i]=p->fx_enabled[i];' in apply_block
assert 'effect_value[i]=p->fx_value[i];' in apply_block
assert 'prisma_control_set_effect(i,effect_enabled[i]);' in apply_block
assert 'effect_enabled[5]=true;' in apply_block
assert 'effect_value[5]=p->fx_value[5];' in apply_block
assert 'motion_enabled[i]=p->motion_enabled[i];' in apply_block
assert 'motion_value[i]=p->motion_value[i];' in apply_block
select_block=app.split('static void cp_select_intensity_button(int button)',1)[1].split('bool prisma_appliance_get_on',1)[0]
assert 'cp_apply_intensity_preset(state);' in select_block

# Manual FX/Motion controls remain present after preset application; presets do not lock them.
assert 'effect_value[effect]=value;' in app
assert 'motion_value[motion]=value;' in app
assert 'effect_enabled[effect]=!effect_enabled[effect];' in app
assert 'motion_enabled[motion]=!motion_enabled[motion];' in app

# Browser reflects the appliance-owned state only; no FREE/status word beside INTENSITY.
assert '"wifi_portal_intensity.c"' in cmake
assert '#include "wifi_portal_checkpoint_a.c"' in wrapper
assert 'intensityPresetState' not in wrapper
assert '>FREE<' not in wrapper
assert "intensityPreset0" in wrapper and "intensityPreset3" in wrapper
assert "classes=['green','yellow','red']" in wrapper
assert "Math.floor(z/3)+1" in wrapper
assert "level:z%3" in wrapper
assert '.intensityPresetBtn.on.green' in wrapper
assert '.intensityPresetBtn.on.yellow' in wrapper
assert '.intensityPresetBtn.on.red' in wrapper

# Presets work by changing normal effect/motion values; the browser pipeline does not special-case intensity.
run_block=portal.split('runFxPipeline=function(s){',1)[1].split('function anyMotionActive',1)[0]
assert '.intensity' not in run_block
assert 'prisma_appliance_get_intensity()' in portal
assert '\\"intensity\\":%d' in portal

# No second state endpoint or poll.
assert "'/api/state?ts='+Date.now()" not in wrapper
assert '"/api/intensity"' not in wrapper
assert '"/api/motion"' not in wrapper

# Validate the extra browser JavaScript exactly as served.
match=re.search(r'static const char intensity_web_injection\[\]\s*=\s*(.*?);\n\nstatic esp_err_t checkpoint_page_get',wrapper,re.S)
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

print('Intensity presets V2 protected: 0 no-op, nine curated presets, manual override preserved, web color reflection')
