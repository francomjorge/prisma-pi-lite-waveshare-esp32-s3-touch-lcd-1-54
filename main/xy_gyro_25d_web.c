#include "xy_gyro_25d_web.h"

const char prisma_xy_gyro_25d_web_injection[] =
"<style>#g23{transform-origin:50% 50%;will-change:transform;backface-visibility:hidden;}</style>"
"<script>(function(){"
"const detached=document.documentElement.classList.contains('detached-mode');"
"let rx=0,ry=0,tx=0,ty=0,last=performance.now(),holdRx=0,holdRy=0;"
"function clamp(v,a,b){return v<a?a:(v>b?b:v);}"
"function dead(v,d){return Math.abs(v)<d?0:(v>0?(v-d)/(1-d):(v+d)/(1-d));}"
"function state(){try{if(detached&&window.opener&&!window.opener.closed&&window.opener.PRISMA_G01_VIEW_STATE)return window.opener.PRISMA_G01_VIEW_STATE();if(window.PRISMA_G01_VIEW_STATE)return window.PRISMA_G01_VIEW_STATE();}catch(e){}return null;}"
"function active(){try{return detached?(new URLSearchParams(location.search).get('generator')||window.PRISMA_ACTIVE_GENERATOR||'g01'):(window.PRISMA_ACTIVE_GENERATOR||((document.getElementById('generator')||{}).value)||'g01');}catch(e){return 'g01';}}"
"function frame(now){requestAnimationFrame(frame);let c=document.getElementById('g23');if(!c)return;let g=active();if(g==='g01'){if(c.style.transform)c.style.transform='';return;}let s=state();if(!s)return;let dt=Math.min(.05,Math.max(.001,(now-last)/1000));last=now;let hold=!!s.keyHold;let sx=Number.isFinite(+s.gx)?clamp(+s.gx,-1,1):0,sy=Number.isFinite(+s.gy)?clamp(+s.gy,-1,1):0;sx=dead(sx,.035);sy=dead(sy,.035);if(hold){tx=holdRx;ty=holdRy;}else{tx=clamp(-sy*18,-18,18);ty=clamp(sx*18,-18,18);holdRx=tx;holdRy=ty;}let k=1-Math.exp(-dt*12);rx+=(tx-rx)*k;ry+=(ty-ry)*k;let mag=Math.min(1,(Math.abs(rx)+Math.abs(ry))/36),scale=1.06+mag*.09,persp=820;c.style.transform='perspective('+persp+'px) rotateX('+rx.toFixed(3)+'deg) rotateY('+ry.toFixed(3)+'deg) scale('+scale.toFixed(4)+')';}"
"requestAnimationFrame(frame);"
"})();</script>";
