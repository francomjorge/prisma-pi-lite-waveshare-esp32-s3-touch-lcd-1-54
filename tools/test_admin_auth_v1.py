#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
auth=(root/'main/admin_api_auth.c').read_text(encoding='utf-8')
cmake=(root/'main/CMakeLists.txt').read_text(encoding='utf-8')

# Fixed HTTP Basic credentials requested for the local ADMIN surface.
assert 'ADMIN_AUTH_HEADER "Basic cHJpc21hLXBpOnByaXNtYS1waQ=="' in auth
assert 'Basic realm=\\"PRISMA-PI ADMIN\\"' in auth
assert 'httpd_req_get_hdr_value_len(r, "Authorization")' in auth
assert 'httpd_req_get_hdr_value_str(r, "Authorization", value, sizeof(value))' in auth
assert 'strcmp(value, ADMIN_AUTH_HEADER) == 0' in auth
assert '401 Unauthorized' in auth
assert 'WWW-Authenticate' in auth

# Every ADMIN page/static/API handler is wrapped before the legacy read-only handler is called.
for name in ('page','css','js','overview','status','sensors','storage','network'):
    assert f'static esp_err_t admin_{name}_get(httpd_req_t *r)' in auth
    block=auth.split(f'static esp_err_t admin_{name}_get(httpd_req_t *r)',1)[1].split('\n}',1)[0]
    assert 'if (!admin_auth_ok(r)) return admin_unauthorized(r);' in block
    assert f'return admin_{name}_get_unauthed(r);' in block

for uri in ('/admin','/admin.css','/admin.js','/api/admin/overview','/api/admin/status','/api/admin/sensors','/api/admin/storage','/api/admin/network'):
    assert f'.uri="{uri}"' in auth

# The wrapper includes the existing implementation rather than duplicating its read-only logic.
assert '#include "admin_api.c"' in auth
assert '"admin_api_auth.c"' in cmake
assert '"admin_api.c"' not in cmake

print('ADMIN auth V1 protected: fixed prisma-pi credentials guard page, assets and all ADMIN APIs')
