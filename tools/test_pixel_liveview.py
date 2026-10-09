from pathlib import Path

root = Path(__file__).resolve().parents[1]
index = (root / "main/web/index.html").read_text(encoding="utf-8")

assert 'id="sourcePreview"' in index
assert 'id="fxCanvas"' in index
assert "var pixelEnabled=false,pixelAmount=50" in index
assert "function pixelBlockSize(v)" in index
assert "function renderLiveView()" in index
assert "function setPixelState(enabled,value)" in index
assert "setPixelState(!!ef[0],vals[0])" in index
assert "fxCtx.imageSmoothingEnabled=false" in index
assert "pixelCtx.drawImage(sourcePreview,0,0,sw,sh)" in index
assert "fxCtx.drawImage(pixelBuffer,0,0,sw,sh,0,0,w,h)" in index
assert "if(!pixelEnabled)" in index
assert "renderLiveView()" in index

# Pixel must reuse the existing global state sync and must not create another poll loop.
assert index.count("'/api/state?ts='+Date.now()") == 1
assert "setInterval" not in index
assert "requestAnimationFrame" not in index

print("PIXEL Live View contract: OK")
