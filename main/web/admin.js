(function(){
'use strict';

function q(id){return document.getElementById(id)}
function text(id,value){var el=q(id);if(el)el.textContent=value}
function bytes(n){
  n=Number(n)||0;
  if(n>=1073741824)return (n/1073741824).toFixed(2)+' GiB';
  if(n>=1048576)return (n/1048576).toFixed(n>=10485760?1:2)+' MiB';
  if(n>=1024)return (n/1024).toFixed(1)+' KiB';
  return n+' B';
}
function getJson(url,ok,fail){
  var x=new XMLHttpRequest();
  x.open('GET',url+(url.indexOf('?')<0?'?':'&')+'ts='+Date.now(),true);
  x.timeout=2500;
  x.onload=function(){
    if(x.status===200){
      try{ok(JSON.parse(x.responseText));return}catch(e){}
    }
    if(fail)fail();
  };
  x.onerror=x.ontimeout=function(){if(fail)fail()};
  x.send(null);
}

function renderStorage(s){
  var total=Number(s.total)||0;
  var free=Number(s.free)||0;
  var used=Number(s.used)||0;
  var pct=total>0?Math.max(0,Math.min(100,(free/total)*100)):0;
  text('storageFree',bytes(free));
  text('storageMeta',bytes(used)+' used · '+bytes(total)+' total · boot snapshot');
  var bar=q('storageBar');if(bar)bar.style.width=pct.toFixed(1)+'%';

  var files=Array.isArray(s.files)?s.files:[];
  text('mediaCount',String(Number(s.media_count)||files.length));
  var list=q('mediaList');
  if(!list)return;
  list.innerHTML='';
  if(!files.length){
    var empty=document.createElement('div');empty.className='file muted';empty.textContent='No media files in storage.';list.appendChild(empty);return;
  }
  files.forEach(function(f){
    var row=document.createElement('div');row.className='file';
    var name=document.createElement('span');name.textContent=f.name||'unnamed';
    var size=document.createElement('b');size.textContent=bytes(f.size);
    row.appendChild(name);row.appendChild(size);list.appendChild(row);
  });
}

function storageError(){
  text('storageFree','ERROR');
  text('storageMeta','Boot snapshot unavailable.');
  text('mediaCount','--');
  var list=q('mediaList');if(list)list.innerHTML='<div class="file error">Storage snapshot unavailable.</div>';
}

function renderWifi(w){
  text('wifiState','READY');
  var state=q('wifiState');if(state){state.classList.remove('error');state.classList.add('ok')}
  text('wifiSsid',w.ssid||'--');
  text('wifiIp',w.ip||'--');
  text('wifiChannel',String(w.channel==null?'--':w.channel));
  text('wifiClients',String(w.client_count==null?'--':w.client_count));
  var list=q('wifiClientList');
  if(!list)return;
  list.className='list';list.innerHTML='';
  var clients=Array.isArray(w.clients)?w.clients:[];
  if(!clients.length){
    var none=document.createElement('div');none.className='client muted';none.textContent='No connected clients.';list.appendChild(none);return;
  }
  clients.forEach(function(c){
    var row=document.createElement('div');row.className='client';
    var mac=document.createElement('span');mac.textContent=c.mac||'--';
    var rssi=document.createElement('b');rssi.textContent=(c.rssi==null?'--':c.rssi+' dBm');
    row.appendChild(mac);row.appendChild(rssi);list.appendChild(row);
  });
}

function wifiError(){
  text('wifiState','ERROR');
  var state=q('wifiState');if(state){state.classList.remove('ok');state.classList.add('error')}
  var list=q('wifiClientList');if(list){list.className='list error';list.textContent='Wi-Fi status unavailable.'}
}

function checkWifi(){
  var button=q('wifiCheck');
  if(button){button.disabled=true;button.textContent='CHECKING…'}
  text('wifiState','CHECKING');
  getJson('/api/admin/network',function(w){renderWifi(w);if(button){button.disabled=false;button.textContent='CHECK WI-FI'}},function(){wifiError();if(button){button.disabled=false;button.textContent='CHECK WI-FI'}});
}

var button=q('wifiCheck');if(button)button.onclick=checkWifi;

/* One request on page load. Endpoint returns RAM cache and never rescans SPIFFS. */
getJson('/api/admin/storage',renderStorage,storageError);

/* Intentionally no recurring polling or background telemetry. */
})();
