(function(){
'use strict';

var titles={live:'LIVE VIEW EXTENDED',sensors:'SENSORS',system:'SYSTEM'};
function q(id){return document.getElementById(id)}
function text(id,value){var el=q(id);if(el)el.textContent=value}
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

function openSection(name){
  if(!titles[name])return;
  var sections=document.querySelectorAll('.section');
  var buttons=document.querySelectorAll('.navBtn');
  for(var i=0;i<sections.length;i++)sections[i].classList.toggle('on',sections[i].id==='sec-'+name);
  for(var j=0;j<buttons.length;j++)buttons[j].classList.toggle('on',buttons[j].getAttribute('data-section')===name);
  text('pageTitle',titles[name]);
  document.body.classList.remove('menuOpen');
}

var nav=document.querySelectorAll('.navBtn');
for(var i=0;i<nav.length;i++)nav[i].onclick=function(){openSection(this.getAttribute('data-section'))};

var menu=q('menuBtn');if(menu)menu.onclick=function(){document.body.classList.toggle('menuOpen')};
var overlay=q('overlay');if(overlay)overlay.onclick=function(){document.body.classList.remove('menuOpen')};

function renderWifi(w){
  text('wifiState','READY');
  var state=q('wifiState');if(state){state.classList.remove('error');state.classList.add('ok')}
  text('wifiSsid',w.ssid||'--');
  text('wifiIp',w.ip||'--');
  text('wifiChannel',String(w.channel==null?'--':w.channel));
  text('wifiClients',String(w.client_count==null?'--':w.client_count));

  var list=q('wifiClientList');
  if(!list)return;
  list.className='clientList';
  list.innerHTML='';
  var clients=Array.isArray(w.clients)?w.clients:[];
  if(!clients.length){
    var none=document.createElement('div');none.className='muted';none.textContent='No connected clients.';list.appendChild(none);return;
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
  var list=q('wifiClientList');if(list){list.className='clientList error';list.textContent='Wi-Fi status unavailable.'}
}

function checkWifi(){
  var button=q('wifiCheck');
  if(button){button.disabled=true;button.textContent='CHECKING…'}
  text('wifiState','CHECKING');
  getJson('/api/admin/network',function(w){renderWifi(w);if(button){button.disabled=false;button.textContent='CHECK WI-FI'}},function(){wifiError();if(button){button.disabled=false;button.textContent='CHECK WI-FI'}});
}

var wifi=q('wifiCheck');if(wifi)wifi.onclick=checkWifi;

/* No automatic device reads. Live View Extended and Sensors are layout-only placeholders. */
})();
