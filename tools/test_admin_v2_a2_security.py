#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]
cmake=(root/'main/CMakeLists.txt').read_text()
sec=(root/'main/security_service.c').read_text()
api=(root/'main/admin_api_v2.c').read_text()
html=(root/'main/web/admin.html').read_text()
css=(root/'main/web/admin.css').read_text()
power=(root/'main/prisma_power.c').read_text()

assert 'security_service.c' in cmake and 'admin_api_v2.c' in cmake and 'mbedtls' in cmake
assert '#define HASH_ROUNDS 12000' in sec
assert '#define RECOVERY_GPIO GPIO_NUM_0' in sec
assert '#define RECOVERY_HOLD_MS 8000' in sec
assert 'nvs_set_blob' in sec and 'KEY_SALT' in sec and 'KEY_HASH' in sec
assert 'prisma_security_create_session' in sec and 'SESSION_IDLE_US' in sec
assert 'HttpOnly; SameSite=Strict' in api
assert '/api/security/status' in api
assert '/api/security/login' in api
assert '/api/security/password' in api
assert '/api/security/remove' in api
assert '/api/security/logout-all' in api
assert 'require_auth' in api and 'auth_required' in api
assert 'SECURITY' in html and 'Admin login' in html
assert 'Hold the physical <b>BOOT</b> button for 8 seconds' in html
assert 'A2 SECURITY' in html
assert 'loginOverlay' in css
assert '#define POWER_OFF_HOLD_MS 5000' in power
print('Admin V2 A2 Security OK')
