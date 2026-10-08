let last='',state={on:true,mode:0,intensity:61,generation:1,filename:'F1_MOON.JPG'};
const photo=document.querySelector('.photo');
const sourcePreview=document.getElementById('sourcePreview');
let currentSource='';
function cleanSourceName(filename){return (filename||'F1_MOON.JPG').replace(/[^A-Za-z0-9._-]/g,'');}
function setSource(filename){if(!sourcePreview)return;const f=cleanSourceName(filename);if(!f||f===currentSource)return;currentSource=f;sourcePreview.style.display='block';sourcePreview.style.visibility='visible';sourcePreview.src='/source/'+encodeURIComponent(f)+'?v='+Date.now();}
if(sourcePreview){sourcePreview.onload=()=>{sourcePreview.style.visibility='visible'};sourcePreview.onerror=()=>{sourcePreview.style.visibility='hidden'};}
function draw(s){state=s;const power=document.getElementById('power'),mode=document.getElementById('mode'),pct=document.getElementById('pct'),fill=document.getElementById('fill'),filename=document.getElementById('filename'),controls=document.getElementById('controls');if(power){power.textContent=s.on?'ON':'OFF';power.className=s.on?'power':'power off';}if(mode)mode.textContent=s.mode?'BYPASS':'RANDOM';if(pct)pct.textContent=s.intensity+'%';if(fill)fill.style.width=s.intensity+'%';if(filename)filename.textContent=(s.filename||'F1_MOON.JPG').toUpperCase();if(controls)controls.className=s.on?'':'disabled';if(photo)photo.classList.toggle('off',!s.on);setSource(s.filename);}
async function sync(){try{const r=await fetch('/api/state',{cache:'no-store'});if(!r.ok)return;const s=await r.json();const j=JSON.stringify(s);if(j!==last){last=j;draw(s);}}catch(e){}}
setSource('F1_MOON.JPG');setInterval(sync,300);sync();
