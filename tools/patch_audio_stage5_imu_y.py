from pathlib import Path
p=Path(__file__).resolve().parents[1]/"main"/"web"/"sensors.html"
s=p.read_text(encoding="utf-8")
if "function arpBaseMidi()" in s and "arpAudioY=" in s:
    print("audio stage5 modulation already applied")
    raise SystemExit(0)
old="let arpAudioX=.5;let tx=0"
new="let arpAudioX=.5,arpAudioY=.5;let tx=0"
assert old in s
s=s.replace(old,new,1)
old="arpAudioX=Math.max(0,Math.min(1,+d.x));arpAudioRev="
new="arpAudioX=Math.max(0,Math.min(1,+d.x));if(Number.isFinite(+d.y))arpAudioY=Math.max(0,Math.min(1,+d.y));arpAudioRev="
assert old in s
s=s.replace(old,new,1)
old="let count=iv.length;return {bpm,count}"
new="let count=Math.max(1,Math.min(iv.length,Math.round(1+(1-arpAudioY)*(iv.length-1))));return {bpm,count}"
assert old in s
s=s.replace(old,new,1)
old="function arpMidi(){let r=last&&Number.isFinite(+last.root)?+last.root:root.selectedIndex,sc=last&&Number.isFinite(+last.scale)?+last.scale:+scale.value,iv=ARP_INTERVALS[sc]||ARP_INTERVALS[0];return 60+r+iv[arpStep++%arpXYConfig().count]}"
new="function arpBaseMidi(){return last&&Number.isFinite(+last.midi)?+last.midi:60+root.selectedIndex}function arpMidi(){let sc=last&&Number.isFinite(+last.scale)?+last.scale:+scale.value,iv=ARP_INTERVALS[sc]||ARP_INTERVALS[0];return arpBaseMidi()+iv[arpStep++%arpXYConfig().count]}"
assert old in s
s=s.replace(old,new,1)
# Keep the current arpeggio step, but continuously follow the live IMU-derived note.
old="if(arpOn&&!arpTimer&&playing)startArpClock();if(!playing&&arpTimer)stopArpClock()"
new="if(arpOn&&!arpTimer&&playing)startArpClock();if(arpOn&&arpTimer&&Number.isFinite(+last.midi)){let sc=Number.isFinite(+last.scale)?+last.scale:+scale.value,iv=ARP_INTERVALS[sc]||ARP_INTERVALS[0],idx=(arpStep+iv.length-1)%arpXYConfig().count;osc.frequency.setTargetAtTime(arpHz(arpBaseMidi()+iv[idx]),ac.currentTime,.025)}if(!playing&&arpTimer)stopArpClock()"
assert old in s
s=s.replace(old,new,1)
s=s.replace("ON · X RATE · SOLO WEB","ON · X RATE / Y NOTES · SOLO WEB")
p.write_text(s,encoding="utf-8")
print("audio stage5 IMU + Y mapping applied")
