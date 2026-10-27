from pathlib import Path

p = Path(__file__).resolve().parents[1] / "main" / "web" / "sensors.html"
s = p.read_text(encoding="utf-8")

old_css = ".hint{font:11px monospace;color:#6f8798;margin-top:8px}"
new_css = ".hint{font:11px monospace;color:#6f8798;margin-top:8px}.music-stack{display:flex;flex-direction:column;gap:12px}.music-actions{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.music-state{font:10px monospace;color:#6f8798;letter-spacing:1px}.music-state.on{color:#28dddd;background:transparent}.arp-row{display:flex;align-items:center;gap:8px}"
if old_css in s:
    s = s.replace(old_css, new_css, 1)

old = '<section class="card"><div class="row"><div><h2>MUSICAL CONTROL</h2><div id="note" class="now">A4</div><div id="hz" class="axis">440.00 HZ</div></div><button id="play" onclick="togglePlay()">PLAY</button></div><div class="controls"><label>ROOT <select id="root" onchange="setMusic()"><option>C</option><option>C#</option><option>D</option><option>D#</option><option>E</option><option>F</option><option>F#</option><option>G</option><option>G#</option><option>A</option><option>A#</option><option>B</option></select></label><label>SCALE <select id="scale" onchange="setMusic()"><option value="0">CHROMATIC</option><option value="1">MAJOR</option><option value="2">MINOR</option><option value="3">DIMINISHED</option><option value="4">DORIAN</option><option value="5">WHOLE TONE</option></select></label><label>WAVE <select id="wave" onchange="setMusic()"><option value="0">SINE</option><option value="1">SQUARE</option><option value="2">TRIANGLE</option><option value="3">SAW</option><option value="4">PULSE</option></select></label><button id="speaker" onclick="toggleSpeaker()">APPLIANCE SPEAKER</button><button id="web" onclick="toggleWeb()">WEB AUDIO OFF</button></div><div class="controls" style="margin-top:14px"><label>VOL <b id="vn">100</b></label><input id="vol" type="range" min="0" max="100" value="100" onchange="setVol(this.value)"><span class="hint">WEB AUDIO is local only; it never changes appliance speaker state.</span></div></section>'
new = '<section class="card"><div class="music-stack"><div><h2>MUSICAL CONTROL</h2><div id="note" class="now">A4</div><div id="hz" class="axis">440.00 HZ</div></div><div class="controls"><label>ROOT <select id="root" onchange="setMusic()"><option>C</option><option>C#</option><option>D</option><option>D#</option><option>E</option><option>F</option><option>F#</option><option>G</option><option>G#</option><option>A</option><option>A#</option><option>B</option></select></label><label>SCALE <select id="scale" onchange="setMusic()"><option value="0">CHROMATIC</option><option value="1">MAJOR</option><option value="2">MINOR</option><option value="3">DIMINISHED</option><option value="4">DORIAN</option><option value="5">WHOLE TONE</option></select></label><label>WAVE <select id="wave" onchange="setMusic()"><option value="0">SINE</option><option value="1">SQUARE</option><option value="2">TRIANGLE</option><option value="3">SAW</option><option value="4">PULSE</option></select></label></div><div class="arp-row"><button id="arp" onclick="toggleArp()">ARP</button><span id="arpState" class="music-state">OFF · ENGINE RESERVED</span><span class="music-state" title="El arpegiador se reproduce únicamente en el navegador">SOLO WEB</span></div><div class="music-actions"><button id="play" onclick="togglePlay()">PLAY</button><button id="speaker" onclick="toggleSpeaker()">APPLIANCE SPEAKER</button><button id="web" onclick="toggleWeb()">WEB AUDIO</button><span id="webState" class="music-state">OFF</span></div><div class="controls"><label>VOL <b id="vn">100</b></label><input id="vol" type="range" min="0" max="100" value="100" onchange="setVol(this.value)"></div><span class="hint">WEB AUDIO is local only; it never changes appliance speaker state.</span></div></section>'
if old not in s:
    if 'id="arp"' not in s:
        raise SystemExit("audio panel signature not found")
else:
    s = s.replace(old, new, 1)

old_vars = "let playing=false,speakerOn=true,webOn=false,ac=null,osc=null,gain=null,last=null,latest=null,fail=0,busy=false,keyHold=false,fsTimer=0;"
new_vars = "let playing=false,speakerOn=true,webOn=false,arpOn=false,ac=null,osc=null,gain=null,last=null,latest=null,fail=0,busy=false,keyHold=false,fsTimer=0;const AUDIO_UI_MEM='prisma-audio-ui-v1';function saveAudioUi(){try{localStorage.setItem(AUDIO_UI_MEM,JSON.stringify({arp:arpOn,web:webOn}))}catch(e){}}function restoreAudioUi(){try{let m=JSON.parse(localStorage.getItem(AUDIO_UI_MEM)||'{}');arpOn=!!m.arp;if(typeof arp!=='undefined'){arp.classList.toggle('on',arpOn);arpState.textContent=arpOn?'ON · ENGINE RESERVED':'OFF · ENGINE RESERVED';arpState.classList.toggle('on',arpOn)}if(m.web&&typeof webState!=='undefined')webState.textContent='OFF · TAP TO RESTORE'}catch(e){}}function toggleArp(){arpOn=!arpOn;arp.classList.toggle('on',arpOn);arpState.textContent=arpOn?'ON · ENGINE RESERVED':'OFF · ENGINE RESERVED';arpState.classList.toggle('on',arpOn);saveAudioUi()}"
if old_vars in s:
    s = s.replace(old_vars, new_vars, 1)

old_web = "async function toggleWeb(){try{if(!webOn){buildWebAudio();await unlockWebAudio();webOn=true;web.textContent='WEB AUDIO ON';web.classList.add('on');syncWeb()}else{webOn=false;if(gain&&ac)gain.gain.setTargetAtTime(0,ac.currentTime,.015);web.textContent='WEB AUDIO OFF';web.classList.remove('on')}}catch(e){webOn=false;web.textContent='WEB AUDIO ERROR';web.classList.remove('on')}}"
new_web = "async function toggleWeb(){try{if(!webOn){buildWebAudio();await unlockWebAudio();webOn=true;web.textContent='WEB AUDIO';web.classList.add('on');webState.textContent='ON';webState.classList.add('on');syncWeb()}else{webOn=false;if(gain&&ac)gain.gain.setTargetAtTime(0,ac.currentTime,.015);web.textContent='WEB AUDIO';web.classList.remove('on');webState.textContent='OFF';webState.classList.remove('on')}saveAudioUi()}catch(e){webOn=false;web.textContent='WEB AUDIO';web.classList.remove('on');webState.textContent='ERROR';webState.classList.remove('on');saveAudioUi()}}"
if old_web in s:
    s = s.replace(old_web, new_web, 1)

old_init = "initVisual();tick();setInterval(tick,250);"
new_init = "restoreAudioUi();initVisual();tick();setInterval(tick,250);"
if old_init in s:
    s = s.replace(old_init, new_init, 1)

p.write_text(s, encoding="utf-8")
print("audio stage1 UI patch applied")
