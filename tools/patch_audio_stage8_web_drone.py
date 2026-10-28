from pathlib import Path
p=Path(__file__).resolve().parents[1]/"main"/"web"/"sensors.html"
s=p.read_text(encoding="utf-8")
if "function restoreWebDrone()" in s:
 print("stage8 already applied");raise SystemExit(0)
old="function toggleArp(){arpOn=!arpOn;arp.classList.toggle('on',arpOn);"
new="function restoreWebDrone(){if(!webOn||!osc||!gain||!ac||!last)return;gain.gain.cancelScheduledValues(ac.currentTime);osc.frequency.cancelScheduledValues(ac.currentTime);osc.type=wt(+last.waveform);osc.frequency.setTargetAtTime(Math.max(20,Number(last.hz)||440),ac.currentTime,.018);gain.gain.setTargetAtTime(last.master&&!last.face_down?.09:0,ac.currentTime,.018)}function toggleArp(){arpOn=!arpOn;arp.classList.toggle('on',arpOn);"
assert old in s
s=s.replace(old,new,1)
old="else{stopArpClock();syncWeb()}saveAudioUi()"
new="else{stopArpClock();restoreWebDrone()}saveAudioUi()"
assert old in s
s=s.replace(old,new,1)
old="if(!arpOn)osc.frequency.setTargetAtTime(Math.max(20,Number(last.hz)||440),ac.currentTime,.018);"
new="if(!arpOn)osc.frequency.setTargetAtTime(Math.max(20,Number(last.hz)||440),ac.currentTime,.018);"
assert old in s
# Do not let stale gate note-off timeouts mute ARP OFF after switching.
old="if(gain&&ac)gain.gain.setTargetAtTime(0,ac.currentTime,.008)}"
new="if(gain&&ac&&arpOn)gain.gain.setTargetAtTime(0,ac.currentTime,.008)}"
assert old in s
s=s.replace(old,new,1)
p.write_text(s,encoding="utf-8")
print("stage8 web drone restoration applied")
