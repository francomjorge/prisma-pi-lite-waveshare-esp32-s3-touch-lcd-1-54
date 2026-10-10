#!/usr/bin/env python3
from pathlib import Path
import ast
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
app=(root/'main/app_local_processing.c').read_text(encoding='utf-8')
portal=(root/'main/wifi_portal_local_processing.c').read_text(encoding='utf-8')
cmake=(root/'main/CMakeLists.txt').read_text(encoding='utf-8')
index=(root/'main/web/index.html').read_text(encoding='utf-8')

# Active build uses the Page-7 wrappers.
assert '"app_local_processing.c"' in cmake
assert '"wifi_portal_local_processing.c"' in cmake
assert '#define CP_PAGE_COUNT 7' in app
assert '#define LOCAL_COUNT 3' in app
assert 'static bool local_processing_active = false;' in app
assert 'static int local_processing_value[LOCAL_COUNT] = {0,0,0};' in app
assert 'static const char *local_processing_names[LOCAL_COUNT] = {"MONO","CONTRAST","NEGATIVE"};' in app

# Appliance page 7 is independent from synth power/mode/intensity and preserves values while inactive.
assert 'static void cp_page_local_processing(void)' in app
assert '"LOCAL PROCESSING"' in app
assert '"ACTIVE"' in app and '"INACTIVE"' in app
assert 'else cp_page_local_processing();' in app
assert 'page==6' in app
assert 'local_processing_active=startx<120;' in app
assert 'local_processing_value[row]=value;' in app
assert 'value>=0&&value<4' in app
assert 'prisma_appliance_get_local_processing_active' in app
assert 'prisma_appliance_get_local_processing_value' in app
reset=app.split('static void cp_reset_performance(void)',1)[1].split('bool prisma_appliance_get_on',1)[0]
assert 'local_processing_active' not in reset
assert 'local_processing_value' not in reset

# Same existing /api/state response carries local state; no new state endpoint/poll.
assert '#define httpd_resp_send local_httpd_resp_send_proxy' in portal
assert '\\"localProcessing\\":{\\"active\\":%s,\\"values\\":[%d,%d,%d]}' in portal
assert 'prisma_appliance_get_local_processing_active()' in portal
assert 'prisma_appliance_get_local_processing_value(0)' in portal
assert '"/api/local' not in portal
assert index.count("'/api/state?ts='+Date.now()") == 1

# Browser page 7 is read only and mirrors ACTIVE/INACTIVE plus 0/1/2/3 for all three rows.
assert "sec.id='localProcessingPage'" in portal
assert 'APPLIANCE PREPROCESSING / READ ONLY' in portal
assert "var names=['MONO','CONTRAST','NEGATIVE']" in portal
assert "for(var v=0;v<4;v++)" in portal
assert "Math.min(6,n)" in portal
assert "pg*(100/7)" in portal
assert "d.id='d6'" in portal
assert 'local_processing_web_injection' in portal
assert 'data-control' not in re.search(r"sec\.innerHTML=(.*?);\\n", portal).group(1)

# FX Lab has immutable yellow Local Processing nodes before the browser pipeline.
assert '.labLocalRow .labBlock{border-color:#f5be23!important' in portal
assert "block.draggable=false" in portal
assert "handle.textContent='●'" in portal
assert "labPipeline.insertBefore(row,labPipeline.firstChild)" in portal
assert "localHint.textContent='YELLOW = LOCAL FIXED · PURPLE = MIXED FX · ORANGE = MOTION · MIX FIXED'" in portal

# Part A must not process pixels locally yet.
for forbidden in ('esp_new_jpeg','jpeg_decode','jpeg_encode','processMonoLocal','processContrastLocal','processNegativeLocal','getImageData'):
    assert forbidden not in portal
    assert forbidden not in app

# Validate injected JS exactly as compiled.
match=re.search(r'static const char local_processing_web_injection\[\]\s*=\s*(.*?);\n\nstatic esp_err_t local_httpd_resp_send_proxy',portal,re.S)
assert match
literals=re.findall(r'"(?:\\.|[^"\\])*"',match.group(1));assert literals
injected=''.join(ast.literal_eval(x) for x in literals)
script_match=re.search(r'<script>(.*?)</script>',injected,re.S);assert script_match
with tempfile.NamedTemporaryFile('w',suffix='.js',encoding='utf-8',delete=False) as tmp:
    tmp.write(script_match.group(1)); path=tmp.name
subprocess.run(['node','--check',path],check=True)
Path(path).unlink(missing_ok=True)

print('Local Processing Part A OK: appliance page 7 + read-only web page 7 + yellow fixed LAB nodes, no processing yet')
