from pathlib import Path

p = Path(__file__).resolve().parents[1] / "main" / "web" / "sensors.html"
s = p.read_text(encoding="utf-8")

old = "function toggleArp(){arpOn=!arpOn;arp.classList.toggle('on',arpOn);arpState.textContent=arpOn?'ON · ENGINE RESERVED':'OFF · ENGINE RESERVED';arpState.classList.toggle('on',arpOn);saveAudioUi()}"
new = "const ARP_INTERVALS=[[0,1,2,3,4,5,6,7,8,9,10,11],[0,2,4,5,7,9,11],[0,2,3,5,7,8,10],[0,3,6,9],[0,2,3,5,7,9,10],[0,2,4,6,8,10]];let arpTimer=0,arpStep=0;function arpMidi(){let r=last&&Number.isFinite(+last.root)?+last.root:root.selectedIndex,sc=last&&Number.isFinite(+last.scale)?+last.scale:+scale.value,iv=ARP_INTERVALS[sc]||ARP_INTERVALS[0];return 60+r+iv[arpStep++%iv.length]}function arpHz(m){return 440*Math.pow(2,(m-69)/12)}function stopArpClock(){if(arpTimer){clearInterval(arpTimer);arpTimer=0}arpStep=0}function arpTick(){if(!arpOn||!webOn||!playing||!osc||!gain||!ac)return;let h=arpHz(arpMidi());osc.frequency.setTargetAtTime(h,ac.currentTime,.012);gain.gain.setTargetAtTime(.09,ac.currentTime,.012)}function startArpClock(){stopArpClock();if(arpOn){arpTick();arpTimer=setInterval(arpTick,250)}}function toggleArp(){arpOn=!arpOn;arp.classList.toggle('on',arpOn);arpState.textContent=arpOn?'ON · 120 BPM':'OFF';arpState.classList.toggle('on',arpOn);if(arpOn)startArpClock();else{stopArpClock();syncWeb()}saveAudioUi()}"
if old not in s:
    raise SystemExit("stage1 ARP signature not found")
s = s.replace(old, new, 1)

old_restore = "arpState.textContent=arpOn?'ON · ENGINE RESERVED':'OFF · ENGINE RESERVED';"
new_restore = "arpState.textContent=arpOn?'ON · 120 BPM':'OFF';"
if old_restore not in s:
    raise SystemExit("stage1 restore signature not found")
s = s.replace(old_restore, new_restore, 1)

old_sync = "function syncWeb(){if(!webOn||!osc||!gain||!ac||!last)return;if(ac.state==='suspended'||ac.state==='interrupted'){ac.resume().then(syncWeb).catch(()=>{});return}osc.type=wt(+last.waveform);osc.frequency.setTargetAtTime(Math.max(20,Number(last.hz)||440),ac.currentTime,.018);gain.gain.setTargetAtTime(last.master&&!last.face_down?.09:0,ac.currentTime,.025)}"
new_sync = "function syncWeb(){if(!webOn||!osc||!gain||!ac||!last)return;if(ac.state==='suspended'||ac.state==='interrupted'){ac.resume().then(syncWeb).catch(()=>{});return}osc.type=wt(+last.waveform);if(!arpOn)osc.frequency.setTargetAtTime(Math.max(20,Number(last.hz)||440),ac.currentTime,.018);gain.gain.setTargetAtTime(last.master&&!last.face_down?.09:0,ac.currentTime,.025);if(arpOn&&!arpTimer)startArpClock()}"
if old_sync not in s:
    raise SystemExit("syncWeb signature not found")
s = s.replace(old_sync, new_sync, 1)

old_music = "async function setMusic(){try{await fetch('/api/sensors/music?root='+root.selectedIndex+'&scale='+scale.value+'&waveform='+wave.value,{method:'POST'})}catch(e){}}"
new_music = "async function setMusic(){try{await fetch('/api/sensors/music?root='+root.selectedIndex+'&scale='+scale.value+'&waveform='+wave.value,{method:'POST'});arpStep=0;if(arpOn)arpTick()}catch(e){}}"
if old_music not in s:
    raise SystemExit("setMusic signature not found")
s = s.replace(old_music, new_music, 1)

p.write_text(s, encoding="utf-8")
print("audio stage2 ARP patch applied")
