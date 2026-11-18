#!/usr/bin/env python3
"""
Generates include/web_portal_gz.h containing a gzipped Embedded Lite Portal
for the ESP32 Outdoor GPS Tracker (http://192.168.4.1).
"""

import gzip
import os

HTML_PORTAL = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Outdoor Tracker Sync</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#000;color:#e2e8f0;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;padding:12px;max-width:600px;margin:0 auto}
h1{font-size:18px;color:#00ffff;margin-bottom:10px;display:flex;justify-content:space-between;align-items:center}
.badge{font-size:11px;background:#0f172a;border:1px solid #1e293b;padding:3px 8px;border-radius:12px;color:#00ff87}
.tabs{display:flex;gap:6px;margin-bottom:12px}
.tab{flex:1;padding:8px;background:#0d1117;border:1px solid #21262d;border-radius:8px;color:#94a3b8;font-size:13px;font-weight:600;text-align:center;cursor:pointer}
.tab.active{background:#162032;border-color:#00ffff;color:#00ffff}
.card{background:#0d1117;border:1px solid #21262d;border-radius:10px;padding:12px;margin-bottom:10px}
.row{display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #161b22;font-size:13px}
.row:last-child{border-bottom:none}
.k{color:#6b7280}.v{color:#f8fafc;font-weight:600}
button,.btn{background:#1f2937;color:#f8fafc;border:1px solid #374151;padding:7px 11px;border-radius:6px;font-size:12px;font-weight:600;cursor:pointer}
button.pri{background:#0369a1;border-color:#0284c7;color:#fff}
button.ok{background:#065f46;border-color:#10b981;color:#fff}
button.del{background:#7f1d1d;border-color:#ef4444;color:#fff}
input,select{width:100%;padding:8px;margin:4px 0 10px;background:#000;color:#fff;border:1px solid #30363d;border-radius:6px;font-size:13px}
label{font-size:12px;color:#94a3b8;display:block;margin-top:4px}
.hidden{display:none}
#msg{font-size:12px;color:#00ff87;margin-bottom:8px;min-height:16px}
</style>
</head>
<body>
<h1>Outdoor Tracker <span class="badge" id="bdg">SYNC</span></h1>
<div id="msg"></div>
<div class="tabs">
  <div class="tab active" onclick="sw('st',this)">Status</div>
  <div class="tab" onclick="sw('lg',this)">Logs</div>
  <div class="tab" onclick="sw('rt',this)">Routes</div>
  <div class="tab" onclick="sw('tm',this)">Team</div>
</div>

<div id="st" class="pane">
  <div class="card" id="stBox">Loading...</div>
  <button class="pri" style="width:100%" onclick="loadStatus()">Refresh Status</button>
</div>

<div id="lg" class="pane hidden">
  <div class="card" id="lgBox">Loading...</div>
</div>

<div id="rt" class="pane hidden">
  <div class="card">
    <label>Upload GPX (Auto RDP Compress -> active.bin)</label>
    <input type="file" id="gpxFile" accept=".gpx">
    <label>RDP Tolerance (meters)</label>
    <input type="number" id="rdpTol" value="5" min="1" max="50">
    <button class="pri" style="width:100%" onclick="uploadGpx()">Compress &amp; Upload Route</button>
  </div>
  <div class="card" id="rtBox">Loading...</div>
</div>

<div id="tm" class="pane hidden">
  <div class="card">
    <label><input type="checkbox" id="tmEn" style="width:auto;margin-right:6px">Enable Team Beacon</label>
    <label>Team PIN (1-65535)</label><input type="number" id="tmId" value="1024">
    <label>Node ID (1-8)</label><input type="number" id="tmNd" value="1" min="1" max="8">
    <label>Callsign (max 6 chars)</label><input type="text" id="tmCs" maxlength="6" value="HIKER1">
    <label>Separation Alert (m)</label><input type="number" id="tmAd" value="250" min="50" max="5000">
    <button class="ok" style="width:100%" onclick="saveTeam()">Save Team Config</button>
  </div>
</div>

<script>
const $=id=>document.getElementById(id);
function msg(t,err){const m=$('msg');m.style.color=err?'#ff3b30':'#00ff87';m.textContent=t;}
function sw(id,el){
  document.querySelectorAll('.pane').forEach(p=>p.classList.add('hidden'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  $(id).classList.remove('hidden');el.classList.add('active');
  if(id==='st')loadStatus();if(id==='lg')loadLogs();if(id==='rt')loadRoutes();if(id==='tm')loadTeam();
}
async function loadStatus(){
  try{
    const d=await(await fetch('/api/status')).json();
    $('bdg').textContent=d.bat_pct+'%';
    $('stBox').innerHTML=[
      ['Board',d.board],
      ['Battery',d.bat_v.toFixed(2)+' V ('+d.bat_pct+'%)'],
      ['GPS',d.gps_fix?('FIX '+d.sats+' sats'):('Searching ('+d.sats+' sats)')],
      ['Coords',d.gps_fix?(d.lat.toFixed(6)+', '+d.lon.toFixed(6)):'--'],
      ['Baro Alt',d.baro_alt.toFixed(1)+' m'],
      ['SD Card',d.sd_ok?('OK ('+d.sd_free_mb+' MB)'):'Not Mounted'],
      ['Route',d.route_name||'None'],
      ['Logging',d.logging?'REC ACTIVE':'Idle']
    ].map(([k,v])=>`<div class="row"><span class="k">${k}</span><span class="v">${v}</span></div>`).join('');
  }catch(e){msg('Status error: '+e,1);}
}
async function loadLogs(){
  try{
    const d=await(await fetch('/api/logs')).json();
    if(!d.files.length){$('lgBox').innerHTML='<div class="k">No CSV logs found</div>';return;}
    $('lgBox').innerHTML=d.files.map(f=>`<div class="row">
      <div><div class="v">${f.name}</div><div class="k">${(f.size/1024).toFixed(1)} KB</div></div>
      <div style="display:flex;gap:4px">
        <button class="pri" onclick="dlGpx('${f.name}')">GPX</button>
        <button onclick="location.href='/api/logs/download?name='+encodeURIComponent('${f.name}')">CSV</button>
        <button class="del" onclick="delLog('${f.name}')">X</button>
      </div></div>`).join('');
  }catch(e){msg('Logs error: '+e,1);}
}
async function dlGpx(name){
  msg('Converting '+name+' to GPX...');
  const txt=await(await fetch('/api/logs/download?name='+encodeURIComponent(name))).text();
  const lines=txt.trim().split(/\r?\n/);
  let pts='';
  for(let i=1;i<lines.length;i++){
    const c=lines[i].split(',');if(c.length<4)continue;
    const t=new Date(parseInt(c[0],10)*1000).toISOString();
    pts+=`<trkpt lat="${c[1]}" lon="${c[2]}"><ele>${c[3]}</ele><time>${t}</time></trkpt>\n`;
  }
  const gpx=`<?xml version="1.0" encoding="UTF-8"?>\n<gpx version="1.1" creator="OutdoorTracker" xmlns="http://www.topografix.com/GPX/1/1"><trk><name>${name}</name><trkseg>\n${pts}</trkseg></trk></gpx>`;
  const a=document.createElement('a');
  a.href=URL.createObjectURL(new Blob([gpx],{type:'application/gpx+xml'}));
  a.download=name.replace(/\.csv$/i,'.gpx');a.click();
  msg('Downloaded '+a.download);
}
async function delLog(name){
  if(!confirm('Delete '+name+'?'))return;
  await fetch('/api/logs?name='+encodeURIComponent(name),{method:'DELETE'});
  loadLogs();msg('Deleted '+name);
}
async function loadRoutes(){
  try{
    const d=await(await fetch('/api/routes')).json();
    if(!d.files.length){$('rtBox').innerHTML='<div class="k">No .bin routes</div>';return;}
    $('rtBox').innerHTML=d.files.map(f=>`<div class="row">
      <div class="v">${f.name} (${(f.size/1024).toFixed(1)} KB)</div>
      <button class="ok" onclick="actRt('${f.name}')">Activate</button></div>`).join('');
  }catch(e){msg('Routes error: '+e,1);}
}
async function actRt(name){
  await fetch('/api/routes/activate?name='+encodeURIComponent(name),{method:'POST'});
  msg('Activated '+name);
}
function ptDistM(p,a,b){
  const R=6371000,rad=Math.PI/180,cos=Math.cos(a[0]*rad);
  const px=(p[1]-a[1])*rad*R*cos,py=(p[0]-a[0])*rad*R;
  const bx=(b[1]-a[1])*rad*R*cos,by=(b[0]-a[0])*rad*R;
  const l2=bx*bx+by*by;if(!l2)return Math.hypot(px,py);
  const t=Math.max(0,Math.min(1,(px*bx+py*by)/l2));
  return Math.hypot(px-t*bx,py-t*by);
}
function rdp(pts,eps){
  if(pts.length<3)return pts;
  let dmax=0,idx=0;
  for(let i=1;i<pts.length-1;i++){
    const d=ptDistM(pts[i],pts[0],pts[pts.length-1]);
    if(d>dmax){dmax=d;idx=i;}
  }
  if(dmax>eps){
    const r1=rdp(pts.slice(0,idx+1),eps),r2=rdp(pts.slice(idx),eps);
    return r1.slice(0,-1).concat(r2);
  }
  return [pts[0],pts[pts.length-1]];
}
function havKm(a,b){
  const R=6371,rad=Math.PI/180,dLat=(b[0]-a[0])*rad,dLon=(b[1]-a[1])*rad;
  const s=Math.sin(dLat/2)**2+Math.cos(a[0]*rad)*Math.cos(b[0]*rad)*Math.sin(dLon/2)**2;
  return 2*R*Math.atan2(Math.sqrt(s),Math.sqrt(1-s));
}
async function uploadGpx(){
  const fi=$('gpxFile').files[0];if(!fi){msg('Select a .gpx file first',1);return;}
  msg('Parsing & compressing GPX...');
  const xml=new DOMParser().parseFromString(await fi.text(),'text/xml');
  const nodes=[...xml.querySelectorAll('trkpt,rtept')];
  const raw=nodes.map(n=>[parseFloat(n.getAttribute('lat')),parseFloat(n.getAttribute('lon'))]).filter(p=>!isNaN(p[0])&&!isNaN(p[1]));
  if(raw.length<2){msg('No valid track points',1);return;}
  const tol=parseFloat($('rdpTol').value)||5;
  const pts=rdp(raw,tol);
  let dist=0,minLa=90,maxLa=-90,minLo=180,maxLo=-180;
  for(let i=0;i<pts.length;i++){
    const[la,lo]=pts[i];
    if(la<minLa)minLa=la;if(la>maxLa)maxLa=la;if(lo<minLo)minLo=lo;if(lo>maxLo)maxLo=lo;
    if(i>0)dist+=havKm(pts[i-1],pts[i]);
  }
  const buf=new ArrayBuffer(96+pts.length*8),dv=new DataView(buf),u8=new Uint8Array(buf);
  'ROUT'.split('').forEach((c,i)=>u8[i]=c.charCodeAt(0));
  dv.setUint16(4,1,true);dv.setUint16(6,0,true);dv.setUint32(8,pts.length,true);
  dv.setFloat32(12,dist,true);dv.setFloat32(16,minLa,true);dv.setFloat32(20,maxLa,true);
  dv.setFloat32(24,minLo,true);dv.setFloat32(28,maxLo,true);
  const nm=fi.name.replace(/\.gpx$/i,'').slice(0,31);
  for(let i=0;i<nm.length;i++)u8[32+i]=nm.charCodeAt(i);
  for(let i=0;i<pts.length;i++){
    dv.setFloat32(96+i*8,pts[i][0],true);
    dv.setFloat32(96+i*8+4,pts[i][1],true);
  }
  const res=await fetch('/api/routes/upload?name=active.bin',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:buf});
  if(res.ok){msg(`Uploaded active.bin (${raw.length}->${pts.length} pts, ${dist.toFixed(1)} km)`);loadRoutes();}
  else msg('Upload failed',1);
}
async function loadTeam(){
  try{
    const d=await(await fetch('/api/team')).json();
    $('tmEn').checked=!!d.enabled;$('tmId').value=d.team_id||1024;
    $('tmNd').value=d.node_id||1;$('tmCs').value=d.callsign||'HIKER1';
    $('tmAd').value=d.alert_dist_m||250;
  }catch(e){}
}
async function saveTeam(){
  const body=JSON.stringify({
    enabled:$('tmEn').checked,team_id:+$('tmId').value,node_id:+$('tmNd').value,
    callsign:$('tmCs').value.toUpperCase().slice(0,6),alert_dist_m:+$('tmAd').value
  });
  const r=await fetch('/api/team',{method:'POST',headers:{'Content-Type':'application/json'},body});
  msg(r.ok?'Team config saved':'Save failed',!r.ok);
}
loadStatus();
</script>
</body>
</html>
"""


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_path = os.path.join(root, "include", "web_portal_gz.h")
    gz = gzip.compress(HTML_PORTAL.encode("utf-8"), compresslevel=9)

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("#pragma once\n#include <stdint.h>\n#include <pgmspace.h>\n\n")
        f.write(f"// Auto-generated by tools/gen_web_portal.py ({len(HTML_PORTAL)}B -> {len(gz)}B gzip)\n")
        f.write(f"static const uint32_t WEB_PORTAL_GZ_LEN = {len(gz)};\n")
        f.write("static const uint8_t WEB_PORTAL_GZ[] PROGMEM = {\n")
        for i in range(0, len(gz), 16):
            chunk = ", ".join(f"0x{b:02x}" for b in gz[i : i + 16])
            f.write(f"    {chunk},\n")
        f.write("};\n")

    print(f"Generated {out_path}: {len(HTML_PORTAL)} bytes -> {len(gz)} bytes gzip")


if __name__ == "__main__":
    main()
