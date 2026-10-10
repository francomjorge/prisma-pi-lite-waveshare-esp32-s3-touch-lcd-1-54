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

# Appliance: four visible buttons, 0 = FREE, buttons 1..3 each have three color states.
for token in ('INTENSITY_BUTTON_X','INTENSITY_BUTTON_Y','INTENSITY_BUTTON_W','INTENSITY_BUTTON_H','INTENSITY_BUTTON_GAP'):
    assert token in app
assert 'static int intensity_button = 0;' in app
assert 'static int intensity_level = 0;' in app
assert 'if(intensity_button<=0)return 0;' in app
assert 'return ((intensity_button-1)*3)+intensity_level+1;' in app
assert 'intensity_level=(intensity_level+1)%3;' in app
assert 'intensity_button=button;' in app and 'intensity_level=0;' in app
assert 'for(int i=0;i<4;i++)' in app
assert 'else if(intensity_level==1)box=yellow;' in app
assert 'else box=red;' in app
assert "char label[2]={(char)('0'+i),0};" in app
assert 'prisma_appliance_get_intensity(void){return cp_intensity_state();}' in app

# Intensity selection is independent from power and replaces the old continuous bar touch mapping.
intensity_touch=app.split('else if(page==1&&starty>=INTENSITY_BUTTON_Y-4',1)[1].split('else if(page==1&&prisma_shared_on&&startx>=82',1)[0]
assert 'cp_select_intensity_button(button)' in intensity_touch
assert 'prisma_shared_on&&' not in intensity_touch
assert 'prisma_shared_intensity=((int)startx-12)*100/216' not in app

# Existing processing does not consume the new intensity state yet.
run_block=portal.split('runFxPipeline=function(s){',1)[1].split('function anyMotionActive',1)[0]
assert '.intensity' not in run_block
assert 'prisma_appliance_get_intensity()' in portal
assert '\\"intensity\\":%d' in portal

# Web layer is read-only and decodes encoded states 0..9 into button + color.
assert '"wifi_portal_intensity.c"' in cmake
assert '#include "wifi_portal_checkpoint_a.c"' in wrapper
assert "intensityPreset0" in wrapper and "intensityPreset3" in wrapper
assert "FREE" in wrapper
assert "names=['GREEN','YELLOW','RED']" in wrapper
assert "classes=['green','yellow','red']" in wrapper
assert "Math.floor(z/3)+1" in wrapper
assert "level:z%3" in wrapper
assert '.intensityPresetBtn.on.green' in wrapper
assert '.intensityPresetBtn.on.yellow' in wrapper
assert '.intensityPresetBtn.on.red' in wrapper
assert 'addEventListener' not in wrapper.split('function decodeIntensityPreset',1)[0].split("<script>(function(){",1)[1]

# Wrapper must add no second poll or state endpoint.
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

print('Intensity presets V1 protected: FREE + 3x3 states, appliance-owned, web-reflected, no processing impact')
