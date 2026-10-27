from pathlib import Path
p=Path(__file__).resolve().parents[1]/"main"/"web"/"sensors.html"
s=p.read_text(encoding="utf-8")
if "function arpXYConfig()" in s:
    print("audio XY ARP already applied")
    raise SystemExit(0)
old="let arpTimer=0,arpStep=0;function arpMidi()"
new="let arpTimer=0,arpStep=0,arpNext=0;function arpXYConfig(){let x=Math.max(-1,Math.min(1,tx)),y=Math.max(-1,Math.min(1,ty));let bpm=Math.round(60+(x+1)*90);let sc=last&&Number.isFinite(+last.scale)?+last.scale:+scale.value;let iv=ARP_INTERVALS[sc]||ARP_INTERVALS[0];let count=Math.max(1,Math.min(iv.length,Math.round(1+(y+1)*.5*(iv.length-1))));return {bpm,count}}function arpMidi()"
assert old in s,"ARP variable signature missing"
s=s.replace(old,new,1)
old="iv[arpStep++%iv.length]"
new="iv[arpStep++%arpXYConfig().count]"
assert old in s,"ARP notes signature missing"
s=s.replace(old,new,1)
old="function stopArpClock(){if(arpTimer){clearInterval(arpTimer);arpTimer=0}arpStep=0}"
new="function stopArpClock(){if(arpTimer){clearInterval(arpTimer);arpTimer=0}arpStep=0;arpNext=0}"
assert old in s,"ARP stop signature missing"
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
p.write_text(s,encoding="utf-8")
print("audio XY ARP applied")
