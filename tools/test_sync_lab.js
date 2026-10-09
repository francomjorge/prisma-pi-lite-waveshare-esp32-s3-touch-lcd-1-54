const fs = require('fs');
const vm = require('vm');

const html = fs.readFileSync('main/web/index.html', 'utf8');
const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
if (!scripts.length) throw new Error('No inline script found');
const script = scripts[scripts.length - 1][1];

class FakeClassList {
  constructor(el){ this.el = el; }
  contains(name){ return (this.el.className || '').split(/\s+/).includes(name); }
  add(name){ if(!this.contains(name)) this.el.className=((this.el.className||'')+' '+name).trim(); }
  remove(name){ this.el.className=(this.el.className||'').split(/\s+/).filter(x=>x&&x!==name).join(' '); }
  toggle(name,on){ if(on===undefined) on=!this.contains(name); on?this.add(name):this.remove(name); }
}
class FakeElement {
  constructor(id){ this.id=id; this.textContent=''; this.className=''; this.style={}; this.listeners={}; this.classList=new FakeClassList(this); }
  addEventListener(name,cb){ this.listeners[name]=cb; }
  closest(){ return null; }
}
const elements = new Map();
function el(id){ if(!elements.has(id)) elements.set(id,new FakeElement(id)); return elements.get(id); }
[
 'v','t','photoFrame','fsBtn','sourcePreview','power','mode','pct','fill','filename','controls',
 'dbgBadge','dbgTry','dbgOk','dbgErr','dbgRtt','dbgReq','dbgSeq','dbgUp','dbgHeap','dbgMatch',
 'dbgOn','dbgSource','dbgIntensity','dbgMode','dbgLegacy','dbgRaw','dbgNow','dbgPause'
].forEach(el);
for(let i=0;i<5;i++) el('d'+i);
for(let i=0;i<6;i++){ el('dbge'+i+'s'); el('dbge'+i+'v'); }
const photo = el('photo');

const document = {
  body: el('body'),
  fullscreenElement: null,
  webkitFullscreenElement: null,
  getElementById: id => el(id),
  querySelector: sel => sel==='.photo' ? photo : null,
  addEventListener: ()=>{},
  exitFullscreen: ()=>{},
  webkitExitFullscreen: ()=>{}
};

const seq = [
  {on:true, mode:0, intensity:61, filename:'F1_MOON.JPG', effects:[false,false,false,false,false,true], values:[50,50,50,50,50,50]},
  {on:true, mode:1, intensity:26, filename:'F1_TANGO.JPG', effects:[true,false,true,false,true,true], values:[12,34,56,78,50,90]},
  {on:false, mode:0, intensity:73, filename:'F1_TANGO.JPG', effects:[true,true,true,true,false,true], values:[99,88,77,66,50,55]}
];
let debugCount=0;
class FakeXHR {
  constructor(){ this.status=0; this.responseText=''; this.timeout=0; }
  open(method,url){ this.url=url; }
  setRequestHeader(){}
  send(){
    setTimeout(()=>{
      if(this.url.includes('/api/debug/state')){
        debugCount++;
        const idx=Math.min(Math.floor((debugCount-1)/2),seq.length-1);
        this.status=200;
        this.responseText=JSON.stringify({
          request_id:debugCount,state_seq:idx+1,uptime_ms:debugCount*500,free_heap:123456,match:false,
          direct:seq[idx],legacy:seq[0]
        });
      } else if(this.url.includes('/api/state')) {
        this.status=200; this.responseText=JSON.stringify(seq[0]);
      } else {
        this.status=404; this.responseText='';
      }
      if(this.onload) this.onload();
    },10);
  }
}

const ctx = {
  document,
  window:{},
  location:{search:'?page=5'},
  XMLHttpRequest:FakeXHR,
  setTimeout,clearTimeout,setInterval,clearInterval,
  Date,Math,Number,String,JSON,isFinite,encodeURIComponent,console
};
ctx.window=ctx;
vm.createContext(ctx);
vm.runInContext(script,ctx,{timeout:1000});

setTimeout(()=>{
  function text(id){ return el(id).textContent; }
  const ok = Number(text('dbgOk'));
  if(ok < 4) throw new Error('Expected repeated XHR polling, got '+ok);
  if(text('dbgSeq') !== '3') throw new Error('state_seq did not advance: '+text('dbgSeq'));
  if(text('dbgOn') !== 'OFF') throw new Error('power did not update: '+text('dbgOn'));
  if(text('dbgSource') !== 'F1_TANGO.JPG') throw new Error('source did not update: '+text('dbgSource'));
  if(text('dbgIntensity') !== '73') throw new Error('intensity did not update: '+text('dbgIntensity'));
  if(text('dbgMode') !== 'RANDOM') throw new Error('mode did not update: '+text('dbgMode'));
  if(el('t').style.transform !== 'translateX(-80%)') throw new Error('page 5 transform wrong: '+el('t').style.transform);
  if(el('d4').className !== 'dot on') throw new Error('page 5 dot not active: '+el('d4').className);
  console.log('SYNC LAB BROWSER LOGIC PASS:', {ok,seq:text('dbgSeq'),source:text('dbgSource'),intensity:text('dbgIntensity'),mode:text('dbgMode')});
  process.exit(0);
},1800);
