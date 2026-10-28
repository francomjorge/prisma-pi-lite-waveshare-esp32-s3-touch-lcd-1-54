from pathlib import Path
p=Path(__file__).resolve().parents[1]/"main"/"web"/"sensors.html"
s=p.read_text(encoding="utf-8")
if "function arpStaticRoot()" in s and "function arpImuGate()" in s:
    print("stage6 already applied")
    raise SystemExit(0)
def replace(a,b):
    global s
    if a not in s: raise SystemExit("stage6 missing signature: "+a[:100])
    s=s.replace(a,b,1)
replace("let count=Math.max(1,Math.min(iv.length,Math.round(1+(1-arpAudioY)*(iv.length-1))))","let count=Math.max(1,Math.min(5,Math.round(1+(1-arpAudioY)*4)))")
replace("function arpBaseMidi(){return last&&Number.isFinite(+last.midi)?+last.midi:60+root.selectedIndex}","function arpStaticRoot(){return 60+(last&&Number.isFinite(+last.root)?+last.root:root.selectedIndex)}function arpBaseMidi(){return arpStaticRoot()}function arpImuGate(){let a=latest&&latest.angle&&Number(latest.angle.x);return Number.isFinite(a)?Math.max(5,Math.min(100,Math.round((Math.max(-90,Math.min(90,a))+90)/180*95+5))):100}")
replace("let h=arpHz(arpMidi()),period=60000/arpXYConfig().bpm;","let h=arpHz(arpMidi()),period=60000/arpXYConfig().bpm;arpGatePct=arpImuGate();let gv=document.getElementById('arpGateValue');if(gv)gv.textContent=arpGatePct+'% IMU X';")
replace("if(arpOn&&arpTimer&&Number.isFinite(+last.midi)){let sc=Number.isFinite(+last.scale)?+last.scale:+scale.value,iv=ARP_INTERVALS[sc]||ARP_INTERVALS[0],idx=(arpStep+iv.length-1)%arpXYConfig().count;osc.frequency.setTargetAtTime(arpHz(arpBaseMidi()+iv[idx]),ac.currentTime,.025)}","")
# Ensure IMU X is reflected on screen while ARP is running; never modify original appliance music.
replace("last=d.beep;playing=", "last=d.beep;if(arpOn){arpGatePct=arpImuGate();let gv=document.getElementById('arpGateValue');if(gv)gv.textContent=arpGatePct+'% IMU X';let gs=document.getElementById('arpGate');if(gs)gs.value=arpGatePct}playing=")
p.write_text(s,encoding="utf-8")
print("stage6 web-only ARP: stable root, IMU X gate, 1-5 Y steps")
