#pragma once

// Captive portal page, served from PROGMEM.
static const char PORTAL_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Glass Canary — Setup</title>
<style>
:root{
--bg:#f4f6f7;--card:#fff;--fg:#15181b;--mut:#5c6672;--bd:#d7dde2;
--acc:#1f6f6b;--accf:#fff;--fld:#fff;--sel:#e6f2f1;
}
@media(prefers-color-scheme:dark){:root{
--bg:#131619;--card:#1b1f23;--fg:#e7ebee;--mut:#9aa5b1;--bd:#2f363d;
--acc:#5fbfb7;--accf:#0e1113;--fld:#22272c;--sel:#1e2f31;
}}
*{box-sizing:border-box}
body{margin:0;padding:16px 14px 32px;background:var(--bg);color:var(--fg);
font:16px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
-webkit-text-size-adjust:100%}
main{max-width:520px;margin:0 auto}
h1{font-size:20px;margin:0 0 2px}
h2{font-size:14px;letter-spacing:.04em;text-transform:uppercase;color:var(--mut);margin:22px 0 8px;font-weight:600}
p{margin:0 0 12px}
.sub{color:var(--mut);font-size:14px}
.card{background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:14px}
label{display:block;font-size:14px;color:var(--mut);margin:12px 0 4px}
input{width:100%;min-height:46px;padding:10px 12px;font-size:16px;font-family:inherit;
color:var(--fg);background:var(--fld);border:1px solid var(--bd);border-radius:8px}
input:focus{outline:2px solid var(--acc);outline-offset:-1px}
.row{display:flex;gap:8px}
.row input{flex:1}
button{font:inherit;min-height:46px;border-radius:8px;cursor:pointer}
.gh{padding:0 14px;background:var(--fld);color:var(--fg);border:1px solid var(--bd);white-space:nowrap}
#go{width:100%;margin-top:18px;background:var(--acc);color:var(--accf);border:0;font-weight:600;font-size:17px}
#go[disabled]{opacity:.6;cursor:default}
#nets{border:1px solid var(--bd);border-radius:10px;overflow:hidden;background:var(--card)}
.net{display:flex;align-items:center;gap:10px;width:100%;min-height:52px;padding:8px 12px;
background:none;color:var(--fg);border:0;border-bottom:1px solid var(--bd);text-align:left}
.net:last-child{border-bottom:0}
.net[aria-pressed=true]{background:var(--sel);box-shadow:inset 3px 0 0 var(--acc)}
.nm{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.op{font-size:12px;color:var(--mut)}
.lk{width:14px;height:14px;fill:var(--mut);flex:none}
.bars{display:flex;align-items:flex-end;gap:2px;height:14px;flex:none}
.bars i{width:3px;background:var(--bd);border-radius:1px}
.bars i:nth-child(1){height:4px}.bars i:nth-child(2){height:7px}
.bars i:nth-child(3){height:10px}.bars i:nth-child(4){height:14px}
.bars i.on{background:var(--acc)}
details{margin-top:22px;border:1px solid var(--bd);border-radius:10px;background:var(--card)}
summary{padding:14px;font-size:15px;cursor:pointer;list-style:none}
summary::-webkit-details-marker{display:none}
summary::after{content:"+";float:right;color:var(--mut)}
details[open] summary::after{content:"–"}
details[open] summary{border-bottom:1px solid var(--bd)}
.dbody{padding:2px 14px 16px}
.note{color:var(--mut);font-size:13px;line-height:1.5}
#empty{padding:14px}
#st{margin-top:12px;font-size:14px;color:var(--mut)}
footer{margin-top:26px;border-top:1px solid var(--bd);padding-top:14px}
</style>

<main>
<h1>Smart Glass Canary</h1>
<p class="sub">This device listens for camera-equipped smart glasses nearby and alerts you with its LED and buzzer. Connecting it to Wi-Fi is optional, and only needed if you want it to report to a server as well.</p>

<form id="f" method="POST" action="/save">
<h2>Choose a network</h2>
<div id="nets"></div>

<div class="card" style="margin-top:14px">
<label for="ssid">Network name</label>
<input id="ssid" name="ssid" required autocapitalize="none" autocorrect="off" spellcheck="false" placeholder="Tap a network above, or type it here">

<label for="pass">Password</label>
<div class="row">
<input id="pass" name="pass" type="password" autocapitalize="none" autocorrect="off" spellcheck="false">
<button type="button" class="gh" id="tg" aria-label="Show password">Show</button>
</div>
<p class="note" style="margin:8px 0 0">Leave empty if the network has no password.</p>
</div>

<details>
<summary>Advanced: MQTT reporting</summary>
<div class="dbody">
<p class="note">Only needed if you run an MQTT broker and want alerts sent to it. Leave the address empty to keep MQTT off.</p>
<label for="mh">Broker address</label>
<input id="mh" name="mqtt_host" autocapitalize="none" autocorrect="off" spellcheck="false" placeholder="e.g. 192.168.1.10">
<label for="mp">Port</label>
<input id="mp" name="mqtt_port" type="number" min="1" max="65535" value="1883">
<label for="mu">Username</label>
<input id="mu" name="mqtt_user" autocapitalize="none" autocorrect="off" spellcheck="false">
<label for="mw">Password</label>
<input id="mw" name="mqtt_pass" type="password">
</div>
</details>

<button id="go" type="submit">Save and restart</button>
<p id="st" hidden>Saving. The device will restart and try to join your network. You can close this page.</p>
</form>

<footer>
<p class="note">The Wi-Fi password is written to the board's flash memory as plain text. It is not encrypted, so anyone holding the board could read it back. Worth knowing if the canary sits somewhere public.</p>
</footer>
</main>

<script>
const NETWORKS = %%NETWORKS%%;
var $=function(i){return document.getElementById(i)};
var ssid=$('ssid'),pass=$('pass'),list=$('nets'),go=$('go');

function strength(r){return r>=-55?4:r>=-67?3:r>=-78?2:1}

function draw(){
  var seen={},ns=[],i,n,k;
  for(i=0;i<NETWORKS.length;i++){
    n=NETWORKS[i];
    if(!n||!n.s)continue;
    k=seen[n.s];
    if(k===undefined){seen[n.s]=ns.length;ns.push(n)}
    else if(n.r>ns[k].r){ns[k]=n}
  }
  ns.sort(function(a,b){return b.r-a.r});
  if(!ns.length){
    list.innerHTML='<p class="note" id="empty">No networks found. Type your network name below, or scan again.</p>';
    var b=document.createElement('button');
    b.type='button';b.className='net';b.textContent='Scan again';
    b.onclick=function(){location.reload()};
    list.appendChild(b);
    return;
  }
  ns.forEach(function(n){
    var b=document.createElement('button');
    b.type='button';b.className='net';b.setAttribute('aria-pressed','false');
    var t=document.createElement('span');
    t.className='nm';t.textContent=n.s;
    b.appendChild(t);
    if(n.e){
      var s=document.createElement('span');
      s.className='lk';
      s.innerHTML='<svg viewBox="0 0 24 24" class="lk" aria-hidden="true"><rect x="5" y="10" width="14" height="10" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3h-2.2V7a1.8 1.8 0 0 0-3.6 0v3z"/></svg>';
      b.appendChild(s);
    }else{
      var o=document.createElement('span');
      o.className='op';o.textContent='open';
      b.appendChild(o);
    }
    var g=document.createElement('span'),j,f=strength(n.r);
    g.className='bars';
    g.title=n.r+' dBm';
    for(j=1;j<5;j++){
      var v=document.createElement('i');
      if(j<=f)v.className='on';
      g.appendChild(v);
    }
    b.appendChild(g);
    b.onclick=function(){
      var all=list.querySelectorAll('.net'),m;
      for(m=0;m<all.length;m++)all[m].setAttribute('aria-pressed','false');
      b.setAttribute('aria-pressed','true');
      ssid.value=n.s;
      pass.value='';
      if(n.e)pass.focus();
    };
    list.appendChild(b);
  });
}

$('tg').onclick=function(){
  var h=pass.type==='password';
  pass.type=h?'text':'password';
  this.textContent=h?'Hide':'Show';
  this.setAttribute('aria-label',(h?'Hide':'Show')+' password');
};

$('f').addEventListener('submit',function(){
  setTimeout(function(){
    go.disabled=true;
    go.textContent='Saving...';
    $('st').hidden=false;
  },0);
});

draw();
</script>
</html>
)HTML";
