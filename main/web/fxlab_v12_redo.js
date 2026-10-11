/* FX LAB V12 REDO: rebuilt from V11. Continuous generators, black stage and precision drag. */
(function(){
'use strict';
var v12rBaseDrawGenerator=null,v12rBaseDecorateGeneratorMenu=null;
var v12rLayerDrag=null,v12rAutoScrollRaf=0,v12rAutoScrollVelocity=0,v12rSuppressClickUntil=0;
var v12rCanvasDrag=null,v12rCanvasPaintRaf=0;

function v12rClamp(v,a,b){return Math.max(a,Math.min(b,v));}
function v12rFrac(v){return v-Math.floor(v);}
function v12rHash(n){return v12rFrac(Math.sin(n*12.9898+78.233)*43758.5453123);}
function v12rSmooth(v){return v*v*(3-2*v);}
function v12rNoise1(x,seed){var i=Math.floor(x),f=x-i,a=v12rHash(i+seed*113.7),b=v12rHash(i+1+seed*113.7);return a+(b-a)*v12rSmooth(f);}
function v12rNoise2(x,y,seed){var ix=Math.floor(x),iy=Math.floor(y),fx=v12rSmooth(x-ix),fy=v12rSmooth(y-iy),s=seed*191.3,a=v12rHash(ix*127.1+iy*311.7+s),b=v12rHash((ix+1)*127.1+iy*311.7+s),c=v12rHash(ix*127.1+(iy+1)*311.7+s),d=v12rHash((ix+1)*127.1+(iy+1)*311.7+s),ab=a+(b-a)*fx,cd=c+(d-c)*fx;return ab+(cd-ab)*fy;}
function v12rVal(layer,key,fallback){var n=Number(layer&&layer[key]);return isFinite(n)?n:fallback;}
function v12rColor(layer){return typeof v10GeneratorColor==='function'?v10GeneratorColor(layer):'#f0f5fa';}
function v12rRgba(hex,a){var rgb=typeof v10HexRgb==='function'?v10HexRgb(hex):[240,245,250];return 'rgba('+rgb[0]+','+rgb[1]+','+rgb[2]+','+v12rClamp(a,0,1)+')';}
function v12rSeed(layer){var s=String(layer&&layer.id||'generator'),n=0;for(var i=0;i<s.length;i++)n=(n*33+s.charCodeAt(i))%100000;return n+1;}
function v12rEnsureState(layer){if(!layer)return;if(!isFinite(Number(layer.genSides)))layer.genSides=5;if(!isFinite(Number(layer.genCount)))layer.genCount=24;if(!isFinite(Number(layer.genSize)))layer.genSize=52;if(!isFinite(Number(layer.genDepth)))layer.genDepth=58;if(!isFinite(Number(layer.genTrail)))layer.genTrail=52;if(!isFinite(Number(layer.genLength)))layer.genLength=70;if(!isFinite(Number(layer.genTurbulence)))layer.genTurbulence=58;if(!isFinite(Number(layer.genSpread)))layer.genSpread=66;if(!isFinite(Number(layer.genThickness)))layer.genThickness=28;if(!isFinite(Number(layer.genSpeed)))layer.genSpeed=35;if(!isFinite(Number(layer.genAmount)))layer.genAmount=80;if(!isFinite(Number(layer.asciiScale)))layer.asciiScale=100;}

function v12rDrawFlowerAscii(ctx,layer,g,speed){
  var cols=40,rows=24,t=performance.now()/1000*(0.12+speed*0.55),scale=v12rClamp(v12rVal(layer,'asciiScale',100)/100,.5,2),cw=(g.w/cols)*scale,ch=(g.h/rows)*scale,ox=-cw*cols/2,oy=-ch*rows/2;
  ctx.save();ctx.fillStyle=v12rColor(layer);ctx.textAlign='left';ctx.textBaseline='top';ctx.font=Math.max(2,ch*.92)+'px monospace';
  for(var r=0;r<rows;r++)for(var c=0;c<cols;c++){var x=(c-cols*.5)/(cols*.5),y=(r-rows*.44)/(rows*.5),rr=Math.sqrt(x*x+y*y),ang=Math.atan2(y,x)+t*.36,petals=.35+.13*Math.cos(6*ang+t*.72),ring=Math.abs(rr-petals),cc=' ';if(rr<.095)cc='@';else if(ring<.052)cc=(Math.sin(ang*6+t*1.4)>0?'*':'+');else if(Math.abs(x)<.03&&y>.31&&y<.91)cc='|';else if(y>.58&&y<.79&&Math.abs(x+(y-.68)*.62)<.05)cc='/';else if(y>.58&&y<.79&&Math.abs(x-(y-.68)*.62)<.05)cc='\\';if(cc!==' ')ctx.fillText(cc,ox+c*cw,oy+r*ch);}
  ctx.restore();
}
function v12rDrawAscii(ctx,layer,g,amount,speed){
  var name=(layer.asciiClip==='eye'||layer.asciiClip==='flower')?layer.asciiClip:'face';if(name==='flower'){v12rDrawFlowerAscii(ctx,layer,g,speed);return;}
  var clip=v5AsciiClips&&v5AsciiClips[name]?v5AsciiClips[name]:v5AsciiClips.face,frames=clip.frames||[];if(!frames.length)return;
  var cycle=.32+speed*1.8,phase=performance.now()/1000*cycle,fi=Math.floor(phase)%frames.length,rows=v5UnpackAscii(frames[fi]).split('\n'),scale=v12rClamp(v12rVal(layer,'asciiScale',100)/100,.5,2),cw=(g.w/clip.w)*scale,ch=(g.h/clip.h)*scale,ox=-cw*clip.w/2,oy=-ch*clip.h/2;
  ctx.save();ctx.fillStyle=v12rColor(layer);ctx.textAlign='left';ctx.textBaseline='top';ctx.font=Math.max(2,ch*.92)+'px monospace';for(var r=0;r<clip.h;r++){var row=rows[r]||'';for(var c=0;c<clip.w;c++){var cc=row.charAt(c)||' ';if(cc===' ')continue;if(((r*41+c*17+fi*7)%100)>=amount)continue;ctx.fillText(cc,ox+c*cw,oy+r*ch);}}ctx.restore();
}

function v12rDrawFlowLines(ctx,layer,g,t){
  var count=Math.round(v12rClamp(v12rVal(layer,'genCount',10),2,30)),th=.5+v12rClamp(v12rVal(layer,'genThickness',28),1,100)*.045,turb=v12rClamp(v12rVal(layer,'genTurbulence',58),0,100)/100,step=Math.max(4,g.w/84),seed=v12rSeed(layer);ctx.lineWidth=th;ctx.strokeStyle=v12rColor(layer);ctx.lineCap='round';
  for(var i=0;i<count;i++){var base=-g.h/2+(i+.5)*g.h/count,s=seed+i*2.17;ctx.beginPath();for(var x=-g.w/2;x<=g.w/2+step;x+=step){var nx=(x/Math.max(1,g.w))*2.8+t*.22,organic=(v12rNoise2(nx,i*.19+t*.11,s)-.5)*g.h*.30*turb,wave=Math.sin(nx*4.1+i*.63+t*.48)*g.h*.022,y=base+organic+wave;if(x===-g.w/2)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();}
}
function v12rDrawPolygon(ctx,layer,g,t){
  var sides=Math.round(v12rClamp(v12rVal(layer,'genSides',5),3,12)),count=Math.round(v12rClamp(v12rVal(layer,'genCount',8),1,24)),size=v12rClamp(v12rVal(layer,'genSize',52),10,100)/100,spread=v12rClamp(v12rVal(layer,'genSpread',66),0,100)/100,md=Math.min(g.w,g.h),seed=v12rSeed(layer);ctx.strokeStyle=v12rColor(layer);ctx.lineWidth=.6+v12rClamp(v12rVal(layer,'genThickness',28),1,100)*.04;ctx.lineJoin='round';
  for(var i=0;i<count;i++){var s=seed+i*13.1,orb=(.06+.35*spread*v12rHash(s*3.7))*md,a=t*(.22+.32*v12rHash(s*7.1))+i*2.399,cx=Math.cos(a)*orb,cy=Math.sin(a*1.13)*orb*.72,r=md*(.032+.13*size*(.35+.65*v12rHash(s*5.3))),rot=t*(.38+.7*v12rHash(s*9.9))+s*.01;ctx.beginPath();for(var k=0;k<sides;k++){var q=rot+k*Math.PI*2/sides,px=cx+Math.cos(q)*r,py=cy+Math.sin(q)*r;if(k===0)ctx.moveTo(px,py);else ctx.lineTo(px,py);}ctx.closePath();ctx.stroke();}
}
function v12rDrawFire(ctx,layer,g,t){
  var count=Math.round(v12rClamp(v12rVal(layer,'genCount',78),18,160)),turb=v12rClamp(v12rVal(layer,'genTurbulence',64),0,100)/100,size=v12rClamp(v12rVal(layer,'genSize',58),10,100)/100,col=v12rColor(layer),md=Math.min(g.w,g.h),seed=v12rSeed(layer);ctx.globalCompositeOperation='lighter';
  for(var i=0;i<count;i++){var s=seed+i*17.31,rate=.10+.22*v12rHash(s),life=v12rFrac(v12rHash(s*2.7)+t*rate),y=g.h*.48-life*g.h*1.04,x=(v12rHash(s*4.1)-.5)*g.w*.62+(v12rNoise1(t*.74+i*.17,s)-.5)*g.w*.26*turb*(.28+life),r=md*(.008+.034*size)*(1-life*.73)*(.5+.9*v12rHash(s*8.2)),a=Math.sin(Math.PI*life)*(.14+.62*(1-life));ctx.fillStyle=v12rRgba(col,a);ctx.beginPath();ctx.ellipse(x,y,r*(.72+.35*(1-life)),r*1.7,0,0,Math.PI*2);ctx.fill();}
  ctx.globalCompositeOperation='source-over';ctx.globalAlpha=1;
}
function v12rDrawOrbit3D(ctx,layer,g,t){
  var count=Math.round(v12rClamp(v12rVal(layer,'genCount',44),8,100)),depth=v12rClamp(v12rVal(layer,'genDepth',62),0,100)/100,size=v12rClamp(v12rVal(layer,'genSize',55),10,100)/100,col=v12rColor(layer),md=Math.min(g.w,g.h),rad=md*(.23+.20*depth),focal=2.35,pts=[],seed=v12rSeed(layer);
  for(var i=0;i<count;i++){var u=(i+.5)/count,v=i*2.3999632297+seed*.001,phi=Math.acos(1-2*u),x=Math.sin(phi)*Math.cos(v),y=Math.cos(phi),z=Math.sin(phi)*Math.sin(v),a=t*.54,b=t*.31,ca=Math.cos(a),sa=Math.sin(a),cb=Math.cos(b),sb=Math.sin(b),x1=x*ca-z*sa,z1=x*sa+z*ca,y1=y*cb-z1*sb,z2=y*sb+z1*cb,p=focal/(focal-z2*.75*depth);pts.push({x:x1*rad*p,y:y1*rad*p,z:z2,p:p});}
  pts.sort(function(a,b){return a.z-b.z;});ctx.strokeStyle=v12rRgba(col,.14);ctx.lineWidth=Math.max(.5,md/500);ctx.beginPath();ctx.ellipse(0,0,rad,rad*.34,Math.sin(t*.2)*.25,0,Math.PI*2);ctx.stroke();
  for(var j=0;j<pts.length;j++){var p=pts[j],front=(p.z+1)/2,rr=md*(.004+.016*size)*p.p*(.5+.75*front);ctx.fillStyle=v12rRgba(col,.20+.76*front);ctx.beginPath();ctx.arc(p.x,p.y,rr,0,Math.PI*2);ctx.fill();ctx.fillStyle='rgba(255,255,255,'+(.08+.28*front)+')';ctx.beginPath();ctx.arc(p.x-rr*.28,p.y-rr*.28,Math.max(.5,rr*.22),0,Math.PI*2);ctx.fill();}
}
function v12rDrawParticleFlow(ctx,layer,g,t){
  var count=Math.round(v12rClamp(v12rVal(layer,'genCount',72),12,150)),trail=v12rClamp(v12rVal(layer,'genTrail',52),0,100)/100,spread=v12rClamp(v12rVal(layer,'genSpread',72),10,100)/100,col=v12rColor(layer),seed=v12rSeed(layer);ctx.lineWidth=.6+v12rClamp(v12rVal(layer,'genThickness',20),1,100)*.024;ctx.lineCap='round';
  for(var i=0;i<count;i++){var s=seed+i*19.17,phase=t*(.12+.22*v12rHash(s))+v12rHash(s*3.1)*12,x=(v12rNoise1(phase,s)-.5)*g.w*.96*spread,y=(v12rNoise1(phase+9.3,s*1.7)-.5)*g.h*.96*spread,prev=phase-(.05+.32*trail),px=(v12rNoise1(prev,s)-.5)*g.w*.96*spread,py=(v12rNoise1(prev+9.3,s*1.7)-.5)*g.h*.96*spread;ctx.strokeStyle=v12rRgba(col,.18+.68*v12rHash(s*7.4));ctx.beginPath();ctx.moveTo(px,py);ctx.lineTo(x,y);ctx.stroke();}
}
function v12rDrawRibbon(ctx,layer,g,t){
  var count=Math.round(v12rClamp(v12rVal(layer,'genCount',5),1,12)),th=.8+v12rClamp(v12rVal(layer,'genThickness',34),1,100)*.055,turb=v12rClamp(v12rVal(layer,'genTurbulence',56),0,100)/100,len=v12rClamp(v12rVal(layer,'genLength',72),20,100)/100,col=v12rColor(layer),step=Math.max(4,g.w/96),half=g.w*.5*len;ctx.lineWidth=th;ctx.lineCap='round';
  for(var i=0;i<count;i++){ctx.strokeStyle=v12rRgba(col,.30+.62*(i+1)/count);ctx.beginPath();for(var x=-half;x<=half+step;x+=step){var q=(x/Math.max(1,g.w))*4+t*.38+i*.79,y=Math.sin(q*2.12+i)*g.h*(.055+.14*turb)+Math.sin(q*.71+t*.52+i*2.1)*g.h*.082*turb+(i-(count-1)/2)*g.h*.036;if(x===-half)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();}
}
function v12rDrawKaleido(ctx,layer,g,t){
  var arms=Math.round(v12rClamp(v12rVal(layer,'genSides',6),3,12)),count=Math.round(v12rClamp(v12rVal(layer,'genCount',18),4,40)),spread=v12rClamp(v12rVal(layer,'genSpread',70),10,100)/100,col=v12rColor(layer),md=Math.min(g.w,g.h),seed=v12rSeed(layer);ctx.lineWidth=.6+v12rClamp(v12rVal(layer,'genThickness',24),1,100)*.03;ctx.strokeStyle=v12rRgba(col,.72);ctx.lineCap='round';
  for(var a=0;a<arms;a++){ctx.save();ctx.rotate(a*Math.PI*2/arms+t*.08);ctx.beginPath();for(var i=0;i<count;i++){var p=i/Math.max(1,count-1),r=p*md*.48*spread,ang=(v12rNoise1(p*3+t*.12,seed+a)-.5)*1.6,x=Math.cos(ang)*r,y=Math.sin(ang)*r;if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);}ctx.stroke();ctx.restore();}
}
function v12rDrawGenerator(ctx,layer,g){
  v12rEnsureState(layer);var type=layer.generatorType||'';if(!type)return;var speed=v12rClamp(v12rVal(layer,'genSpeed',35),0,100)/100,t=performance.now()/1000*(.18+speed*2.0),amount=v12rClamp(v12rVal(layer,'genAmount',80),1,100);ctx.save();ctx.beginPath();ctx.rect(-g.w/2,-g.h/2,g.w,g.h);ctx.clip();
  if(type==='ascii')v12rDrawAscii(ctx,layer,g,amount,speed);else if(type==='lines')v12rDrawFlowLines(ctx,layer,g,t);else if(type==='polygon')v12rDrawPolygon(ctx,layer,g,t);else if(type==='fire')v12rDrawFire(ctx,layer,g,t);else if(type==='orbit3d')v12rDrawOrbit3D(ctx,layer,g,t);else if(type==='flow'||type==='particles')v12rDrawParticleFlow(ctx,layer,g,t);else if(type==='ribbon'||type==='wave')v12rDrawRibbon(ctx,layer,g,t);else if(type==='kaleido')v12rDrawKaleido(ctx,layer,g,t);else if(v12rBaseDrawGenerator)v12rBaseDrawGenerator(ctx,layer,g);ctx.restore();
}

function v12rRange(parent,label,min,max,value,fn){return v5RangeRow(parent,label,min,max,value,fn);}
function v12rDecorateGeneratorMenu(layer,menu){
  if(!menu)return;var old10=menu.querySelector('.labGeneratorControlsV10'),old5=menu.querySelector('.labGeneratorControlsV5');if(old10)old10.remove();else if(old5)old5.remove();v12rEnsureState(layer);
  var box=document.createElement('div');box.className='labGeneratorControlsV5 labGeneratorControlsV10 labGeneratorControlsV12R';var opts=[['','CHOOSE...'],['lines','FLOW LINES'],['polygon','POLYGON'],['fire','FIRE / PLASMA'],['orbit3d','ORBIT SPHERES'],['flow','PARTICLE FLOW'],['ribbon','RIBBON'],['kaleido','KALEIDO'],['ascii','ASCII']];
  var extras=document.createElement('div');extras.className='labGenExtrasV5';
  v5SelectRow(box,'TYPE',opts,layer.generatorType||'',function(v){layer.generatorType=v;renderExtras();v2EnsureAnimation();v2Paint();});
  v10ColorRow(box,'COLOR',v12rColor(layer),function(v){layer.generatorColor=v;});box.appendChild(extras);
  function speed(){v12rRange(extras,'SPEED',0,100,v12rVal(layer,'genSpeed',35),function(v){layer.genSpeed=v;v2EnsureAnimation();});}
  function renderExtras(){extras.innerHTML='';var ty=layer.generatorType||'';if(!ty)return;
    if(ty==='ascii'){v5SelectRow(extras,'ANIM',[['face','FACE'],['eye','EYE'],['flower','FLOWER']],(layer.asciiClip==='eye'||layer.asciiClip==='flower')?layer.asciiClip:'face',function(v){layer.asciiClip=v;});speed();v12rRange(extras,'DENSITY',20,100,v12rVal(layer,'genAmount',80),function(v){layer.genAmount=v;});v12rRange(extras,'SCALE',50,200,v12rVal(layer,'asciiScale',100),function(v){layer.asciiScale=v;});}
    else if(ty==='polygon'){v12rRange(extras,'SIDES',3,12,v12rVal(layer,'genSides',5),function(v){layer.genSides=v;});v12rRange(extras,'COUNT',1,24,v12rVal(layer,'genCount',8),function(v){layer.genCount=v;});speed();v12rRange(extras,'SIZE',10,100,v12rVal(layer,'genSize',52),function(v){layer.genSize=v;});v12rRange(extras,'SPREAD',0,100,v12rVal(layer,'genSpread',66),function(v){layer.genSpread=v;});v12rRange(extras,'THICK',1,100,v12rVal(layer,'genThickness',28),function(v){layer.genThickness=v;});}
    else if(ty==='fire'){v12rRange(extras,'COUNT',18,160,v12rVal(layer,'genCount',78),function(v){layer.genCount=v;});speed();v12rRange(extras,'SIZE',10,100,v12rVal(layer,'genSize',58),function(v){layer.genSize=v;});v12rRange(extras,'TURB',0,100,v12rVal(layer,'genTurbulence',64),function(v){layer.genTurbulence=v;});}
    else if(ty==='orbit3d'){v12rRange(extras,'COUNT',8,100,v12rVal(layer,'genCount',44),function(v){layer.genCount=v;});speed();v12rRange(extras,'DEPTH',0,100,v12rVal(layer,'genDepth',62),function(v){layer.genDepth=v;});v12rRange(extras,'SIZE',10,100,v12rVal(layer,'genSize',55),function(v){layer.genSize=v;});}
    else if(ty==='flow'){v12rRange(extras,'COUNT',12,150,v12rVal(layer,'genCount',72),function(v){layer.genCount=v;});speed();v12rRange(extras,'TRAIL',0,100,v12rVal(layer,'genTrail',52),function(v){layer.genTrail=v;});v12rRange(extras,'SPREAD',10,100,v12rVal(layer,'genSpread',72),function(v){layer.genSpread=v;});v12rRange(extras,'THICK',1,100,v12rVal(layer,'genThickness',20),function(v){layer.genThickness=v;});}
    else if(ty==='ribbon'){v12rRange(extras,'COUNT',1,12,v12rVal(layer,'genCount',5),function(v){layer.genCount=v;});speed();v12rRange(extras,'LENGTH',20,100,v12rVal(layer,'genLength',72),function(v){layer.genLength=v;});v12rRange(extras,'TURB',0,100,v12rVal(layer,'genTurbulence',56),function(v){layer.genTurbulence=v;});v12rRange(extras,'THICK',1,100,v12rVal(layer,'genThickness',34),function(v){layer.genThickness=v;});}
    else if(ty==='kaleido'){v12rRange(extras,'ARMS',3,12,v12rVal(layer,'genSides',6),function(v){layer.genSides=v;});v12rRange(extras,'POINTS',4,40,v12rVal(layer,'genCount',18),function(v){layer.genCount=v;});speed();v12rRange(extras,'SPREAD',10,100,v12rVal(layer,'genSpread',70),function(v){layer.genSpread=v;});v12rRange(extras,'THICK',1,100,v12rVal(layer,'genThickness',24),function(v){layer.genThickness=v;});}
    else{v12rRange(extras,'COUNT',2,30,v12rVal(layer,'genCount',10),function(v){layer.genCount=v;});speed();v12rRange(extras,'TURB',0,100,v12rVal(layer,'genTurbulence',58),function(v){layer.genTurbulence=v;});v12rRange(extras,'THICK',1,100,v12rVal(layer,'genThickness',28),function(v){layer.genThickness=v;});}
  }
  renderExtras();var size=menu.querySelector('.labSizeControlV2');menu.insertBefore(box,size||null);
}

function v12rClearMarks(){if(!list)return;var rows=list.querySelectorAll('.labLayerV2');for(var i=0;i<rows.length;i++){rows[i].classList.remove('dragBefore');rows[i].classList.remove('dragAfter');}}
function v12rStopAutoScroll(){v12rAutoScrollVelocity=0;if(v12rAutoScrollRaf){cancelAnimationFrame(v12rAutoScrollRaf);v12rAutoScrollRaf=0;}}
function v12rAutoScrollStep(){v12rAutoScrollRaf=0;if(!v12rLayerDrag||!list||!v12rAutoScrollVelocity)return;list.scrollTop+=v12rAutoScrollVelocity;v12rAutoScrollRaf=requestAnimationFrame(v12rAutoScrollStep);}
function v12rSetAutoScroll(clientY){if(!list)return;var r=list.getBoundingClientRect(),edge=Math.min(34,Math.max(22,r.height*.13)),v=0;if(clientY<r.top+edge)v=-v12rClamp((r.top+edge-clientY)/edge*8,2,8);else if(clientY>r.bottom-edge)v=v12rClamp((clientY-(r.bottom-edge))/edge*8,2,8);v12rAutoScrollVelocity=v;if(v&&!v12rAutoScrollRaf)v12rAutoScrollRaf=requestAnimationFrame(v12rAutoScrollStep);if(!v)v12rStopAutoScroll();}
function v12rFindDrop(clientY){if(!list)return null;var rows=list.querySelectorAll('.labLayerV2'),best=null,bestDist=1e9;for(var i=0;i<rows.length;i++){var row=rows[i],id=row.dataset.layerId;if(!id||id===v12rLayerDrag.fromId)continue;var r=row.getBoundingClientRect(),cy=(r.top+r.bottom)/2,d=Math.abs(clientY-cy);if(clientY>=r.top&&clientY<=r.bottom){var ratio=(clientY-r.top)/Math.max(1,r.height),after=ratio>.56?true:(ratio<.44?false:(v12rLayerDrag.targetId===id?v12rLayerDrag.after:clientY>cy));return {row:row,id:id,after:after};}if(d<bestDist){bestDist=d;best={row:row,id:id,after:clientY>cy};}}return best;}
function v12rHookLayerDrag(){if(!list)return;var rows=list.querySelectorAll('.labLayerV2');for(var i=0;i<rows.length;i++){var row=rows[i],zone=row.querySelector('.labDragZoneV2');if(!zone||zone.dataset.v12rHooked==='1')continue;zone.dataset.v12rHooked='1';zone.draggable=false;zone.title='Drag layer / click to select';zone.style.touchAction='none';zone.addEventListener('pointerdown',function(ev){if(ev.button!==undefined&&ev.button!==0)return;var r=this.closest('.labLayerV2'),id=r&&r.dataset.layerId;if(!id)return;v12rLayerDrag={pointerId:ev.pointerId,fromId:id,startX:ev.clientX,startY:ev.clientY,targetId:'',after:false,active:false,zone:this};try{this.setPointerCapture(ev.pointerId);}catch(e){}},true);}}
function v12rLayerMove(ev){var d=v12rLayerDrag;if(!d||d.pointerId!==ev.pointerId)return;var dx=ev.clientX-d.startX,dy=ev.clientY-d.startY;if(!d.active&&Math.hypot(dx,dy)<5)return;d.active=true;d.zone.classList.add('dragging');v12rSetAutoScroll(ev.clientY);var hit=v12rFindDrop(ev.clientY);v12rClearMarks();if(hit){d.targetId=hit.id;d.after=hit.after;hit.row.classList.add(hit.after?'dragAfter':'dragBefore');}ev.preventDefault();ev.stopImmediatePropagation();}
function v12rLayerEnd(ev){var d=v12rLayerDrag;if(!d||d.pointerId!==ev.pointerId)return;v12rStopAutoScroll();v12rClearMarks();try{d.zone.classList.remove('dragging');if(d.zone.hasPointerCapture&&d.zone.hasPointerCapture(ev.pointerId))d.zone.releasePointerCapture(ev.pointerId);}catch(e){}v12rLayerDrag=null;if(d.active){v12rSuppressClickUntil=Date.now()+220;if(d.targetId&&d.targetId!==d.fromId)v2Move(d.fromId,d.targetId,d.after);ev.preventDefault();ev.stopImmediatePropagation();}}
document.addEventListener('pointermove',v12rLayerMove,true);document.addEventListener('pointerup',v12rLayerEnd,true);document.addEventListener('pointercancel',v12rLayerEnd,true);document.addEventListener('click',function(ev){if(Date.now()<v12rSuppressClickUntil&&ev.target&&ev.target.closest&&ev.target.closest('.labDragZoneV2')){ev.preventDefault();ev.stopImmediatePropagation();}},true);

function v12rCanvasPoint(ev){var r=labCanvas.getBoundingClientRect();if(!r.width||!r.height)return null;return {x:(ev.clientX-r.left)*(labCanvas.width/r.width),y:(ev.clientY-r.top)*(labCanvas.height/r.height)};}
function v12rCanvasHit(layer,p){var rot=((previewRotation%360)+360)%360,g=v2LayerGeometry(layer,labCanvas.width,labCanvas.height,rot),padX=Math.max(8,labCanvas.width*.012),padY=Math.max(8,labCanvas.height*.012);return p&&p.x>=g.x-padX&&p.x<=g.x+g.w+padX&&p.y>=g.y-padY&&p.y<=g.y+g.h+padY;}
function v12rSchedulePaint(){if(v12rCanvasPaintRaf)return;v12rCanvasPaintRaf=requestAnimationFrame(function(){v12rCanvasPaintRaf=0;v2Paint();});}
labCanvas.addEventListener('pointerdown',function(ev){if(!v2Selected)return;var layer=v2LayerById(v2Selected),p=v12rCanvasPoint(ev);if(!layer||!p||!v12rCanvasHit(layer,p))return;v12rCanvasDrag={pointerId:ev.pointerId,startX:p.x,startY:p.y,layer:layer,startLX:Number(layer.x)||0,startLY:Number(layer.y)||0,active:false};try{labCanvas.setPointerCapture(ev.pointerId);}catch(e){}},true);
labCanvas.addEventListener('pointermove',function(ev){var d=v12rCanvasDrag;if(!d||d.pointerId!==ev.pointerId)return;var p=v12rCanvasPoint(ev);if(!p)return;var dx=p.x-d.startX,dy=p.y-d.startY;if(!d.active&&Math.hypot(dx,dy)<2.5)return;d.active=true;d.layer.x=v12rClamp(d.startLX+dx/Math.max(1,labCanvas.width),-.9,.9);d.layer.y=v12rClamp(d.startLY+dy/Math.max(1,labCanvas.height),-.9,.9);v2Pointer=null;v12rSchedulePaint();ev.preventDefault();ev.stopImmediatePropagation();},true);
function v12rCanvasEnd(ev){var d=v12rCanvasDrag;if(!d||d.pointerId!==ev.pointerId)return;v2Pointer=null;try{if(labCanvas.hasPointerCapture&&labCanvas.hasPointerCapture(ev.pointerId))labCanvas.releasePointerCapture(ev.pointerId);}catch(e){}v12rCanvasDrag=null;if(d.active){ev.preventDefault();ev.stopImmediatePropagation();}}
labCanvas.addEventListener('pointerup',v12rCanvasEnd,true);labCanvas.addEventListener('pointercancel',v12rCanvasEnd,true);

function v12rBlackDetached(){try{if(v5DetachedWindow&&!v5DetachedWindow.closed){var d=v5DetachedWindow.document;if(d&&d.body)d.body.style.background='#000';var c=d&&d.querySelector('canvas');if(c)c.style.background='#000';}}catch(e){}}
function v12rApplyBlackStage(){labFrame.style.background='#000';labCanvas.style.background='#000';var st=document.getElementById('prismaV12RedoStyle');if(!st){st=document.createElement('style');st.id='prismaV12RedoStyle';st.textContent='#labFrame,#labCanvas{background:#000!important}.labFrame.pseudoFull,#labFrame:fullscreen,#labFrame:-webkit-full-screen{background:#000!important}.labDragZoneV2{touch-action:none!important}.labLayerV2.dragBefore{box-shadow:inset 0 2px 0 #af37ff}.labLayerV2.dragAfter{box-shadow:inset 0 -2px 0 #af37ff}.labGeneratorControlsV12R{border-top:1px solid #236b75}';document.head.appendChild(st);}v12rBlackDetached();}

function v12rInit(){
  if(typeof v5DrawGenerator!=='function'||typeof v5DecorateGeneratorMenu!=='function'||typeof v2RenderList!=='function'||typeof v10ColorRow!=='function'){setTimeout(v12rInit,60);return;}
  v12rBaseDrawGenerator=v5DrawGenerator;v12rBaseDecorateGeneratorMenu=v5DecorateGeneratorMenu;v5DrawGenerator=v12rDrawGenerator;v5DecorateGeneratorMenu=v12rDecorateGeneratorMenu;
  var baseRender=v2RenderList;v2RenderList=function(){var out=baseRender();v12rHookLayerDrag();return out;};
  if(typeof v5DetachOutput==='function'){var baseDetach=v5DetachOutput;v5DetachOutput=function(){var out=baseDetach.apply(this,arguments);setTimeout(v12rBlackDetached,0);setTimeout(v12rBlackDetached,120);return out;};}
  if(typeof v5PushDetached==='function'){var basePush=v5PushDetached;v5PushDetached=function(){v12rBlackDetached();return basePush.apply(this,arguments);};}
  v12rApplyBlackStage();v2RenderList();v2EnsureAnimation();v2Paint();
}
setTimeout(v12rInit,0);
})();
