from pathlib import Path
p=Path(__file__).resolve().parents[1]/"main"/"web"/"sensors.html"
s=p.read_text(encoding="utf-8")
if "function arpXYConfig()" in s and "function arpGateSet(" in s and "arpPadX=px;arpPadY=py" in s:
    print("audio XY ARP already applied")
    raise SystemExit(0)
# The Stage1 panel exists by now; append the GATE control next to ARP.
old_row='<div class="arp-row"><button id="arp" onclick="toggleArp()">ARP</button>'
new_row='<div class="arp-row"><button id="arp" onclick="toggleArp()">ARP</button><label style="display:flex;align-items:center;gap:6px;font:10px monospace;white-space:nowrap">GATE <input id="arpGate" type="range" min="0" max="100" value="100" step="1" style="width:110px" oninput="arpGateSet(this.value)"><span id="arpGateValue">100%</span></label>'
assert old_row in s,"ARP panel signature missing"
s=s.replace(old_row,new_row,1)
old="let arpTimer=0,arpStep=0;function arpMidi()"
new="let arpTimer=0,arpStep=0,arpNext=0,arpGatePct=100,arpGateOff=0;function arpGateSet(v){arpGatePct=Math.max(0,Math.min(100,Number(v)||0));arpGateValue.textContent=arpGatePct+'%';if(arpGatePct===0&&gain&&ac)gain.gain.setTargetAtTime(0,ac.currentTime,.005)}function arpXYConfig(){let x=Math.max(-1,Math.min(1,arpPadX)),y=Math.max(-1,Math.min(1,arpPadY));let bpm=Math.round(60+(x+1)*90);let sc=last&&Number.isFinite(+last.scale)?+last.scale:+scale.value;let iv=ARP_INTERVALS[sc]||ARP_INTERVALS[0];let count=Math.max(1,Math.min(iv.length,Math.round(1+(y+1)*.5*(iv.length-1))));return {bpm,count}}function arpMidi()"
assert old in s,"ARP variable signature missing"
s=s.replace(old,new,1)
old="iv[arpStep++%iv.length]"
new="iv[arpStep++%arpXYConfig().count]"
assert old in s,"ARP notes signature missing"
s=s.replace(old,new,1)
old="function stopArpClock(){if(arpTimer){clearInterval(arpTimer);arpTimer=0}arpStep=0}"
new="function stopArpClock(){if(arpTimer){clearInterval(arpTimer);arpTimer=0}arpStep=0;arpNext=0;if(arpGateOff){clearTimeout(arpGateOff);arpGateOff=0}if(gain&&ac)gain.gain.setTargetAtTime(0,ac.currentTime,.008)}"
assert old in s,"ARP stop signature missing"
s=s.replace(old,new,1)
old="function arpTick(){if(!arpOn||!webOn||!playing||!osc||!gain||!ac)return;let h=arpHz(arpMidi());osc.frequency.setTargetAtTime(h,ac.currentTime,.012);gain.gain.setTargetAtTime(.09,ac.currentTime,.012)}"
new="function arpTick(){if(!arpOn||!webOn||!playing||!osc||!gain||!ac)return;if(arpGateOff){clearTimeout(arpGateOff);arpGateOff=0}let h=arpHz(arpMidi()),period=60000/arpXYConfig().bpm;osc.frequency.setTargetAtTime(h,ac.currentTime,.012);gain.gain.setTargetAtTime(arpGatePct>0?.09:0,ac.currentTime,.008);if(arpGatePct<100)arpGateOff=setTimeout(()=>{arpGateOff=0;if(gain&&ac)gain.gain.setTargetAtTime(0,ac.currentTime,.008)},period*arpGatePct/100)}"
assert old in s,"ARP tick signature missing"
s=s.replace(old,new,1)
old="function startArpClock(){stopArpClock();if(arpOn){arpTick();arpTimer=setInterval(arpTick,250)}}"
new="function startArpClock(){stopArpClock();if(!arpOn||!webOn||!playing||!osc||!gain||!ac)return;arpTick();arpNext=performance.now()+60000/arpXYConfig().bpm;arpTimer=setInterval(()=>{if(!arpOn||!webOn||!playing||!osc||!gain||!ac){stopArpClock();return}let now=performance.now();if(now>=arpNext){arpTick();arpNext=now+60000/arpXYConfig().bpm}},20)}"
assert old in s,"ARP timer signature missing"
s=s.replace(old,new,1)
old="if(arpOn&&!arpTimer)startArpClock()"
new="if(arpOn&&!arpTimer&&playing)startArpClock();if(!playing&&arpTimer)stopArpClock()"
assert old in s,"ARP sync signature missing"
s=s.replace(old,new,1)
old="arpState.textContent=arpOn?'ON · 120 BPM':'OFF';"
new="arpState.textContent=arpOn?'ON · X TEMPO / Y NOTES · SOLO WEB':'OFF · SOLO WEB';"
assert s.count(old)>=2,"ARP status signatures missing"
s=s.replace(old,new)

# Read the exact final XY values sent to the WebGL visual pad, after its existing smoothing.
old="let tx=0,ty=0,tz=0,gx=0"
new="let arpPadX=0,arpPadY=0;let tx=0,ty=0,tz=0,gx=0"
assert old in s,"pad state signature missing"
s=s.replace(old,new,1)
old="if(keyHold){px=holdPx;py=holdPy}gl.uniform2f(U.R,w,h);"
new="if(keyHold){px=holdPx;py=holdPy}arpPadX=px;arpPadY=py;gl.uniform2f(U.R,w,h);"
assert old in s,"rendered XY pad signature missing"
s=s.replace(old,new,1)
# Do not let the 250ms status poll defeat the GATE note-off envelope.
old="gain.gain.setTargetAtTime(last.master&&!last.face_down?.09:0,ac.currentTime,.025);"
new="if(!arpOn)gain.gain.setTargetAtTime(last.master&&!last.face_down?.09:0,ac.currentTime,.025);"
assert old in s,"syncWeb GATE envelope signature missing"
s=s.replace(old,new,1)
p.write_text(s,encoding="utf-8")
print("audio XY ARP applied")
