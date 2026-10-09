from pathlib import Path

root = Path(__file__).resolve().parents[1]
index = (root / "main/web/index.html").read_text(encoding="utf-8")
portal = (root / "main/wifi_portal.c").read_text(encoding="utf-8")
admin = (root / "main/admin_api.c").read_text(encoding="utf-8")

assert 'id="labCanvas"' in index
assert 'FX LAB / TEST LIVE VIEW' in index
assert 'var labState={on:true,pixelEnabled:false,pixelValue:50}' in index
assert 'function pixelBlockSize(value)' in index
assert 'function renderFxLab()' in index
assert 'labState.pixelEnabled=!!ef[0]' in index
assert 'labState.pixelValue=clamp100(vals[0])' in index
assert 'labCtx.imageSmoothingEnabled=false' in index
assert 'labBufferCtx.drawImage(sourcePreview' in index
assert 'labCtx.drawImage(labBuffer' in index

# The lab must reuse the existing source image and global state sync.
assert 'new Image()' not in index
assert index.count("'/api/state?ts='+Date.now()") == 1
assert index.count("/source/") == 1

# Existing controller and Admin routes remain present.
assert 'prisma_admin_register(s);' in portal
assert 'uri="/admin"' in admin
assert 'uri="/api/admin/network"' in admin

print("Page 5 FX lab contract: OK")
