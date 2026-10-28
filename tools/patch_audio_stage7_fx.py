from pathlib import Path
p=Path(__file__).resolve().parents[1]/"main"/"web"/"sensors.html"
s=p.read_text(encoding="utf-8")
if "function prismaFxUpdate()" in s:
    print("stage7 FX already applied")
    raise SystemExit(0)
def rep(a,b):
    global s
    assert a in s, "missing stage7 signature: "+a[:90]
    s=s.replace(a,b,1)
rep("ac=new AC();osc=null;gain=null","ac=new AC();osc=null;gain=null;prismaDry=null;prismaDelay=null;prismaDelayFeedback=null;prismaDelayWet=null;prismaReverb=null;prismaReverbWet=null")
rep("gain.connect(ac.destination);osc.start()","prismaFxBuild();gain.connect(prismaDry);gain.connect(prismaDelay);gain.connect(prismaReverb);osc.start()")
rep("function unlockWebAudio(){", """let prismaDry=null,prismaDelay=null,prismaDelayFeedback=null,prismaDelayWet=null,prismaReverb=null,prismaReverbWet=null;
function prismaFxBuild(){if(prismaDry)return;prismaDry=ac.createGain();prismaDry.gain.value=.85;prismaDry.connect(ac.destination);
prismaDelay=ac.createDelay(1);prismaDelay.delayTime.value=.27;prismaDelayFeedback=ac.createGain();prismaDelayFeedback.gain.value=.24;prismaDelayWet=ac.createGain();prismaDelayWet.gain.value=0;prismaDelay.connect(prismaDelayFeedback);prismaDelayFeedback.connect(prismaDelay);prismaDelay.connect(prismaDelayWet);prismaDelayWet.connect(ac.destination);
prismaReverb=ac.createConvolver();let sr=ac.sampleRate,len=Math.floor(sr*1.1),buf=ac.createBuffer(2,len,sr);for(let ch=0;ch<2;ch++){let a=buf.getChannelData(ch);for(let i=0;i<len;i++)a[i]=(Math.random()*2-1)*Math.pow(1-i/len,3)}prismaReverb.buffer=buf;prismaReverbWet=ac.createGain();prismaReverbWet.gain.value=0;prismaReverb.connect(prismaReverbWet);prismaReverbWet.connect(ac.destination)}
function prismaFxUpdate(){if(!ac||!prismaDelayWet||!prismaReverbWet)return;let y=latest&&latest.angle?Number(latest.angle.y):0;if(!Number.isFinite(y))y=0;let amount=Math.min(1,Math.max(0,(Math.abs(y)-8)/55));let mode=y>8?'DELAY':y< -8?'REVERB':'DRY';if(!arpOn||!webOn)mode='DRY';prismaDelayWet.gain.setTargetAtTime(mode==='DELAY'?.35*amount:0,ac.currentTime,.09);prismaReverbWet.gain.setTargetAtTime(mode==='REVERB'?.45*amount:0,ac.currentTime,.09);let label=document.getElementById('prismaFxStatus');if(label)label.textContent=mode+' '+Math.round(amount*100)+'%'}
function unlockWebAudio(){""")
rep("last=d.beep;if(arpOn){","last=d.beep;prismaFxUpdate();if(arpOn){")
rep('<button id="web" onclick="toggleWeb()">WEB AUDIO OFF</button>','<button id="web" onclick="toggleWeb()">WEB AUDIO OFF</button><span id="prismaFxStatus" style="font:11px monospace">FX DRY</span>')
p.write_text(s,encoding="utf-8")
print("stage7 browser-only IMU Y reverb/delay applied; visual untouched")
