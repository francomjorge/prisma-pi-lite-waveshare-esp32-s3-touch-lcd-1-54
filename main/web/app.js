let last='',state={on:true,mode:0,intensity:61,generation:1,filename:'F1_MOON.JPG',effects:[false,false,false,false,false,true],effect_values:[50,50,50,50,50,50]};
const photo=document.querySelector('.photo');
const sourcePreview=document.getElementById('sourcePreview');
const power=document.getElementById('power');
const mode=document.getElementById('mode');
const pct=document.getElementById('pct');
const fill=document.getElementById('fill');
const filename=document.getElementById('filename');
const sourceSelect=document.getElementById('sourceSelect');
const intensityBar=document.getElementById('intensityBar');
const controls=document.getElementById('controls');
let currentSource='';
let intensityDragging=false;
let draftIntensity=61;

function cleanSourceName(filename){return (filename||'F1_MOON.JPG').replace(/[^A-Za-z0-9._-]/g,'');}
function setSource(filename){if(!sourcePreview)return;const f=cleanSourceName(filename);if(!f||f===currentSource)return;currentSource=f;sourcePreview.style.display='block';sourcePreview.style.visibility='visible';sourcePreview.src='/source/'+encodeURIComponent(f)+'?v='+Date.now();}
if(sourcePreview){sourcePreview.onload=()=>{sourcePreview.style.visibility='visible'};sourcePreview.onerror=()=>{sourcePreview.style.visibility='hidden'};}
function clamp100(v){v=Number(v);if(!Number.isFinite(v))return 0;return Math.max(0,Math.min(100,Math.round(v)));}
function renderIntensity(v){v=clamp100(v);if(pct)pct.textContent=v+'%';if(fill)fill.style.width=v+'%';}
function drawEffects(s){const enabled=Array.isArray(s.effects)?s.effects:[false,false,false,false,false,true];const values=Array.isArray(s.effect_values)?s.effect_values:[50,50,50,50,50,50];const effects=document.getElementById('effects');if(effects)effects.className=s.on?'effects':'effects disabled';for(let i=0;i<6;i++){const row=document.getElementById('fx'+i);const isOn=i===5?true:!!enabled[i];if(row)row.classList.toggle('off',!isOn);if(i===4){const sw=document.getElementById('fxmirror');if(sw){sw.textContent=isOn?'ON':'OFF';sw.className=isOn?'fxswitch':'fxswitch off';}}else{const value=clamp100(values[i]);const val=document.getElementById('fxv'+i),fxfill=document.getElementById('fxfill'+i);if(val)val.textContent=String(value);if(fxfill)fxfill.style.width=value+'%';}}}
function draw(s){state=s;if(power){power.textContent=s.on?'ON':'OFF';power.className=s.on?'power':'power off';}if(mode)mode.textContent=s.mode?'BYPASS':'RANDOM';if(!intensityDragging){draftIntensity=clamp100(s.intensity);renderIntensity(draftIntensity);}if(filename)filename.textContent=(s.filename||'F1_MOON.JPG').toUpperCase();if(controls)controls.className=s.on?'':'disabled';if(photo)photo.classList.toggle('off',!s.on);setSource(s.filename);drawEffects(s);}
async function sync(){try{const r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)return;const s=await r.json();const j=JSON.stringify(s);if(j!==last){last=j;draw(s);}}catch(e){}}
async function sendControl(payload){try{const r=await fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},cache:'no-store',body:JSON.stringify(payload)});if(!r.ok)throw new Error('HTTP '+r.status);await r.json();setTimeout(sync,40);setTimeout(sync,160);return true;}catch(e){last='';setTimeout(sync,20);return false;}}

if(power){power.addEventListener('click',()=>{const desired=!state.on;state={...state,on:desired};last='';draw(state);sendControl({on:desired});});}
if(mode){mode.addEventListener('click',()=>{if(!state.on)return;const desired=state.mode?0:1;state={...state,mode:desired};last='';draw(state);sendControl({mode:desired});});}
if(sourceSelect){sourceSelect.addEventListener('click',()=>{sendControl({source_next:true});});}

function intensityFromPointer(e){if(!intensityBar)return draftIntensity;const r=intensityBar.getBoundingClientRect();if(r.width<=0)return draftIntensity;return clamp100(((e.clientX-r.left)/r.width)*100);}
if(intensityBar){
 intensityBar.addEventListener('pointerdown',e=>{if(!state.on)return;intensityDragging=true;draftIntensity=intensityFromPointer(e);renderIntensity(draftIntensity);try{intensityBar.setPointerCapture(e.pointerId);}catch(_){}});
 intensityBar.addEventListener('pointermove',e=>{if(!intensityDragging)return;draftIntensity=intensityFromPointer(e);renderIntensity(draftIntensity);});
 intensityBar.addEventListener('pointerup',e=>{if(!intensityDragging)return;draftIntensity=intensityFromPointer(e);intensityDragging=false;state={...state,intensity:draftIntensity};last='';renderIntensity(draftIntensity);sendControl({intensity:draftIntensity});});
 intensityBar.addEventListener('pointercancel',()=>{intensityDragging=false;last='';sync();});
}

setSource('F1_MOON.JPG');drawEffects(state);setInterval(sync,300);sync();
