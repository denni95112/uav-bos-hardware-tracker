#include "portal.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "display.h"
#include "gateway.h"
#include "gnss.h"
#include "mesh.h"
#include "uplink.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

namespace portal {

namespace {

const char kPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>UAV-BOS Fahrzeug-Tracker</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#f2f2f2;color:#222}
header{background:#b71c1c;color:#fff;padding:12px 16px;font-size:1.2em;font-weight:600}
main{max-width:520px;margin:auto;padding:12px}
section{background:#fff;border-radius:8px;padding:12px 16px;margin-bottom:12px;box-shadow:0 1px 3px #0002}
h2{font-size:1em;margin:0 0 8px}
label{display:block;margin-top:10px;font-size:.9em;color:#555}
input,select{width:100%;box-sizing:border-box;padding:8px;font-size:1em;border:1px solid #bbb;border-radius:4px}
button{margin-top:14px;padding:10px 14px;font-size:1em;border:0;border-radius:4px;background:#b71c1c;color:#fff;cursor:pointer}
button.sec{background:#666}
.row{display:flex;gap:8px}.row>*{flex:1}
table{width:100%;border-collapse:collapse;font-size:.9em}td{padding:3px 0}td:first-child{color:#666;width:45%}
.ok{color:#2e7d32;font-weight:600}.err{color:#c62828;font-weight:600}
small,p.hint{color:#777}p.hint{font-size:.8em;line-height:1.35;margin:2px 0 0}
a{color:#b71c1c}code{word-break:break-all}
.row button{margin-top:4px}
.nt td,.nt td:first-child{width:auto;color:#222;padding:3px 4px 3px 0}.nt tr:first-child td{color:#666;font-weight:600}
</style></head><body>
<header>UAV-BOS Fahrzeug-Tracker</header>
<main>
<section><h2>Steuerung</h2>
<label>Betriebsart (sofort, ohne Neustart)</label>
<div class="row" id="modes" style="flex-wrap:wrap"><button type="button" class="sec" data-m="0" onclick="setMode(0)">Nur WLAN</button>
<button type="button" class="sec" data-m="1" onclick="setMode(1)">Gateway</button>
<button type="button" class="sec" data-m="3" onclick="setMode(3)">Offline</button>
<button type="button" class="sec" data-m="2" onclick="setMode(2)">Nur LoRa</button></div>
<p class="hint">Nur WLAN sendet die Position ins Fahrzeug-WLAN. Gateway macht dasselbe und leitet zusaetzlich Positionen anderer Tracker aus dem Funknetz ans Internet. Offline haelt das WLAN nur fuer diese Seite: keine Positionsmeldung an UAV BOS und keine Update-Pruefung, die Position geht ueber LoRa. Nur LoRa schaltet das WLAN aus, die Position geht nur ueber Funk.</p>
<label>Display-Beleuchtung</label>
<div class="row"><button type="button" class="sec" id="blbtn" onclick="toggleBl()">-</button></div>
<p class="hint">Schaltet nur die Hintergrundbeleuchtung. Der Tracker sendet weiter. Die Taste 3 s halten macht dasselbe.</p>
<small id="ctlmsg"></small></section>
<section><h2>Status</h2><table id="st"><tr><td>Lade...</td></tr></table></section>
<section id="meshsec" style="display:none"><h2>LoRa-Mesh</h2><table id="mt"></table>
<table id="nodes" class="nt" style="margin-top:10px"></table></section>
<section><h2>Einstellungen</h2>
<form method="POST" action="/save" onsubmit="return chk()">
<label>WLAN (Fahrzeug-Router / Hotspot)</label>
<div class="row"><select id="nets" onchange="if(this.value)ssid.value=this.value"><option value="">- Netzwerke suchen -</option></select>
<button type="button" class="sec" style="margin-top:0;flex:0 0 auto" onclick="scan()">Suchen</button></div>
<label>SSID</label><input name="ssid" id="ssid" maxlength="32">
<p class="hint">Name des Fahrzeug-WLANs, nur 2,4 GHz. Bei "Nur LoRa" nicht noetig. "Suchen" listet die Netze in Reichweite.</p>
<label>WLAN-Passwort <small id="passhint"></small></label><input name="pass" id="pass" type="password" maxlength="63">
<p class="hint">Passwort dieses WLANs. Ein leeres Feld laesst ein gespeichertes Passwort stehen, ausser die SSID wird geaendert.</p>
<label>Request-URL</label>
<input name="url" id="url" type="url" placeholder="https://gps.beta.uav-bos.de/telemetry/objects/vehicle-key/api-key">
<small id="urlhint"></small>
<p class="hint">Adresse, an die die Position unveraendert per POST geht. Sie enthaelt den API-Schluessel. Im Fahrzeug-WLAN wird sie nur maskiert angezeigt.</p>
<label>Sendeintervall (Sekunden)</label><input name="interval" id="interval" type="number" min="1" max="3600" required>
<p class="hint">Abstand der Positionsmeldungen ueber WLAN, 1 bis 3600 Sekunden.</p>
<label>Betriebsart</label>
<select name="mode" id="mode"><option value="0">Nur WLAN</option><option value="1">Gateway (WLAN + LoRa-Mesh)</option>
<option value="3">Offline (WLAN an, kein Uplink)</option><option value="2">Nur LoRa (WLAN aus)</option></select>
<p class="hint">Gilt nach dem Speichern und bleibt nach einem Neustart erhalten. Ein kurzer Tastendruck schaltet im Betrieb dasselbe um.</p>
<label>LoRa-Sendeintervall (Sekunden, min. 15)</label><input name="lorainterval" id="lorainterval" type="number" min="15" max="3600" required>
<p class="hint">Abstand der Positionen ueber Funk. Bei vielen Trackern hoeher setzen, etwa 10 mal die Anzahl der Tracker in Sekunden. Bei hoher Sendezeit verlaengert der Tracker das Intervall selbst.</p>
<h2 style="margin-top:16px">LoRa / Meshtastic</h2>
<small>Alle Tracker und die Meshtastic-Knoten, die weiterleiten sollen, brauchen dasselbe Profil und denselben Slot.
Standard ist Meshtastic EU_868 LongFast.</small>
<label>Modemprofil <a href="/hilfe/profil">Profile erklaeren</a></label>
<select name="preset" id="preset" onchange="slotVis()"><option value="0">ShortFast (SF7, 250 kHz)</option>
<option value="1">ShortSlow (SF8, 250 kHz)</option><option value="2">MediumFast (SF9, 250 kHz)</option>
<option value="3">MediumSlow (SF10, 250 kHz)</option><option value="4">LongFast (SF11, 250 kHz, Standard)</option>
<option value="5">LongModerate (SF11, 125 kHz)</option><option value="6">LongSlow (SF12, 125 kHz)</option></select>
<div id="slotrow"><label>Frequenz-Slot</label>
<select name="slot" id="slot"><option value="0">Meshtastic-Standard</option><option value="1">1 (869,4625 MHz)</option>
<option value="2">2 (869,5875 MHz)</option></select>
<p class="hint">Nur bei LongModerate und LongSlow. Profile mit 250 kHz senden immer auf 869,525 MHz. "Standard" waehlt den Slot wie Meshtastic aus dem Profilnamen.</p></div>
<label>Hop-Limit eigener Pakete (1-7)</label>
<input name="hops" id="hops" type="number" min="1" max="7" required>
<p class="hint">Wie oft ein eigenes Paket hoechstens weitergeleitet wird. Meshtastic nutzt 3. Mehr Hops reichen weiter und belasten jeden Knoten dazwischen.</p>
<label>Sendeleistung (dBm, 2-22)</label><input name="txpower" id="txpower" type="number" min="2" max="22" required>
<p class="hint">22 dBm ist das Maximum des Funkchips. Niedriger nur, wenn die Reichweite bewusst klein bleiben soll.</p>
<label>Weiterleitung</label>
<select name="relay" id="relay" onchange="relayVis()"><option value="0">Alle Pakete (wie Meshtastic "ALL")</option>
<option value="1">Nur Pakete der UAV-BOS-Tracker</option><option value="2">Keine</option></select>
<p class="hint">"Alle" reicht auch fremde Meshtastic-Pakete weiter und vergroessert deren Netz. "Nur Tracker" bleibt auf dem UAV-BOS-Kanal. "Keine" leitet nichts weiter.</p>
<div id="fairrow"><label>Sendezeit fuer fremde Pakete (% pro Stunde, 0-10)</label>
<input name="fairtime" id="fairtime" type="number" min="0" max="10" required>
<p class="hint">Fremde Meshtastic-Pakete werden nur weitergeleitet, solange die gesamte Sendezeit unter diesem Wert liegt. 0 schaltet das aus, 10 geht bis zur gesetzlichen Grenze. Tracker-Pakete werden immer weitergeleitet.</p></div>
<label>Mesh-Schluessel <small id="keyhint"></small></label>
<div class="row"><input name="meshkey" id="meshkey" type="password" maxlength="64" autocomplete="off" spellcheck="false">
<button type="button" class="sec" style="margin-top:0;flex:0 0 auto" onclick="showKey()">Anzeigen</button>
<button type="button" class="sec" style="margin-top:0;flex:0 0 auto" onclick="genKey()">Neu</button></div>
<p class="hint">Base64, 16 oder 32 Byte. Auf allen Trackern der Organisation gleich, sonst verstehen sie sich nicht. "Neu" nur verwenden, wenn danach jedes Geraet denselben Schluessel bekommt.</p>
<label>Passwort fuer Config-AP und Weboberflaeche, Benutzer "admin" <small id="aphint">(min. 8 Zeichen, leer = kein Passwort)</small></label>
<input name="appass" id="appass" type="password" maxlength="63">
<p class="hint">Schuetzt diesen Zugang. Mindestens 8 Zeichen. Ein leeres Feld laesst ein gesetztes Passwort stehen.</p>
<label id="apclrrow" style="display:none"><input type="checkbox" name="apclear" value="1" style="width:auto"> Passwort entfernen</label>
<button type="submit">Speichern &amp; Neustart</button>
</form></section>
<section><h2>Wartung</h2>
<form method="POST" action="/reset" onsubmit="return confirm('Alle Einstellungen loeschen?')">
<button class="sec" type="submit">Werkseinstellungen</button></form>
<p class="hint">Loescht WLAN, URL, Mesh-Schluessel und alle LoRa-Einstellungen. Der Tracker startet neu und oeffnet den Config-AP.</p>
<small id="fw"></small></section>
</main>
<script>
const $=id=>document.getElementById(id);
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function chk(){const a=$('appass').value;if(a&&a.length<8){alert('AP-Passwort min. 8 Zeichen');return false}
 if($('mode').value!='2'&&!$('ssid').value.trim()){alert('SSID fehlt');return false}
 const k=$('meshkey').value.trim();if(k&&!keyOk(k)){alert('Mesh-Schluessel ungueltig (Base64, 16 oder 32 Byte)');return false}
 return true}
function keyOk(k){try{const n=atob(k).length;return n==16||n==32}catch(e){return false}}
function showKey(){$('meshkey').type='text'}
function genKey(){
 if($('meshkey').value&&!confirm('Neuen Schluessel erzeugen? Alle anderen Tracker brauchen danach denselben Schluessel.'))return;
 const b=new Uint8Array(32);crypto.getRandomValues(b);$('meshkey').value=btoa(String.fromCharCode(...b));showKey();
}
async function load(){
 const s=await (await fetch('/settings')).json();
 $('ssid').value=s.ssid;$('interval').value=s.interval;$('url').value=s.url;
 $('mode').value=s.mode;$('lorainterval').value=s.lorainterval;
 $('preset').value=s.preset;$('slot').value=s.slot;$('hops').value=s.hops;$('txpower').value=s.txpower;
 $('relay').value=s.relay;$('fairtime').value=s.fairtime;slotVis();relayVis();
 $('passhint').textContent=s.hasPass?'(leer = unveraendert)':'';
 if(s.hasApPass){$('aphint').textContent='(gesetzt, leer = unveraendert)';$('apclrrow').style.display='block'}
 $('urlhint').textContent=s.urlMasked?'Gespeicherte URL: '+s.urlMasked+' - leer lassen = unveraendert':'';
 $('meshkey').value=s.meshKey;
 if(!s.meshKeySet)$('keyhint').innerHTML='<span class="err">Standard-Schluessel aktiv, bitte setzen</span>';
 else if(!s.meshKey)$('keyhint').textContent='(gesetzt, nur am Config-AP sichtbar, leer = unveraendert)';
 $('fw').textContent='Firmware '+s.fw+' | '+s.mac;
}
function slotVis(){$('slotrow').style.display=$('preset').value>=5?'block':'none'}
function relayVis(){$('fairrow').style.display=$('relay').value=='0'?'block':'none'}
async function status(){
 try{const s=await (await fetch('/status')).json();
 const up=s.up,g=s.gps,m=s.mesh,gw=s.gw;
 const sendOk=up.code>=200&&up.code<300;
 let r=[['Betriebsart',esc(s.mode)],['WLAN',esc(s.wifi)],['IP',esc(s.ip)],
 ['GPS',g.valid?'<span class="ok">Fix</span>':'<span class="err">kein Fix</span>'],
 ['Satelliten / HDOP',g.sats+' / '+g.hdop.toFixed(1)],
 ['Position',g.age>=0?g.lat.toFixed(6)+', '+g.lon.toFixed(6):'-'],
 ['Hoehe / Geschw.',g.alt.toFixed(0)+' m / '+(g.speed*3.6).toFixed(0)+' km/h'],
 ['Kurs / Genauigkeit',g.heading.toFixed(0)+'&deg; / '+g.acc.toFixed(1)+' m'+(g.gst?' (GST)':' (HDOP)')],
 ['Letztes Senden',s.modeId==3?'aus (keine Meldung an UAV BOS, kein Update)':(up.ago<0?'-':'vor '+up.ago+' s, <span class="'+(sendOk?'ok':'err')+'">'+up.code+'</span>')]];
 if(s.modeId!=3)r.push(['Gesendet / Fehler',up.sent+' / '+up.fail]);
 if(s.modeId!=3&&up.error)r.push(['Fehler','<code>'+esc(up.error)+'</code>']);
 if(s.modeId!=3&&up.payload)r.push(['Letzte Daten','<code>'+esc(up.payload)+'</code>']);
 if(m.error)r.push(['LoRa','<span class="err">'+esc(m.error)+'</span>']);
 $('st').innerHTML=rows(r);
 showCtl(s.modeId,s.backlight);
 $('meshsec').style.display=m.active?'block':'none';
 if(m.active){
  let t=[['Eigene Knoten-ID',m.node+(m.placeholder?' <span class="err">Standard-Schluessel!</span>':'')],
  ['Funk',esc(m.preset)+', '+m.freq.toFixed(4)+' MHz, SF'+m.sf+', '+m.bw+' kHz, CR 4/'+m.cr+', '+m.txp+' dBm, '+m.hops+' Hops'],
  ['Verbundene Tracker<br><small>in den letzten 5 min gehoert</small>','<b>'+m.act+'</b> ('+m.direct+' direkt, '+(m.act-m.direct)+' ueber Weiterleitung)'],
  ['Tracker mit Positionsdaten<br><small>letzte Stunde</small>','<b>'+m.pos1h+'</b>'],
  ['Eigene LoRa-Sendungen',m.tx+(m.lastTx>=0?' (Position vor '+age(m.lastTx)+')':'')+
   (m.scale>1?', <span class="err">Intervall x'+m.scale+' (Netz ausgelastet)</span>':'')],
  ['Weitergeleitete Pakete',m.relay+' ('+m.relayOwn+' Tracker, '+m.relayForeign+' fremde)'+
   (m.fdrop?', <span class="err">'+m.fdrop+' fremde verworfen</span>':'')],
  ['Empfangen ueber Meshtastic<br><small>letzter Weiterleiter kein Tracker</small>',m.viaForeign],
  ['URL wiederholt<br><small>keine Weiterleitung gehoert</small>',m.credRetry],
  ['Positionen selbst zugestellt statt weitergeleitet',m.posNoRelay],
  ['Airtime letzte Stunde',m.air.toFixed(1)+' % von 10 % ('+m.airTracker.toFixed(1)+' % Tracker)'+(m.blocked?', <span class="err">'+m.blocked+' blockiert</span>':'')]];
  if(m.rx)t.push(['Letzter Empfang','RSSI '+m.rssi+' dBm, SNR '+m.snr.toFixed(1)+' dB']);
  if(gw)t.push(['An UAV BOS weitergeleitet / Fehler / verworfen',gw.fwd+' / '+gw.fail+' / '+gw.drop+' ('+gw.known+' Tracker-URLs bekannt)']);
  $('mt').innerHTML=rows(t);
  $('nodes').innerHTML=m.nodes.length?'<tr><td>Tracker</td><td>zuletzt</td><td>Position</td><td>Weg</td><td>Signal</td></tr>'+
   m.nodes.map(n=>'<tr><td>'+n.id+'</td><td>vor '+age(n.ago)+'</td><td>'+(n.pos<0?'-':'vor '+age(n.pos)+' ('+n.cnt+')')+
   '</td><td>'+(n.hops?n.hops+' Hop'+(n.hops>1?'s':'')+(n.mt?' (Meshtastic)':''):'direkt')+'</td><td>'+n.rssi+' dBm / '+n.snr.toFixed(1)+' dB</td></tr>').join('')
   :'<tr><td>Noch keine anderen Tracker gehoert</td></tr>';
 }
 }catch(e){}
}
function rows(r){return r.map(x=>'<tr><td>'+x[0]+'</td><td>'+x[1]+'</td></tr>').join('')}
function age(s){return s<120?s+' s':s<7200?Math.round(s/60)+' min':Math.round(s/3600)+' h'}
let curMode=-1,blState=true;
function showCtl(mode,bl){
 curMode=mode;blState=bl;
 document.querySelectorAll('#modes button').forEach(b=>b.className=b.dataset.m==mode?'':'sec');
 $('blbtn').textContent=bl?'Display ist an - ausschalten':'Display ist aus - einschalten';
}
async function post(url,body){
 const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body||''});
 const t=await r.text();if(!r.ok)throw new Error(t);return t;
}
async function setMode(m){
 const old=curMode;
 if(m==old)return;
 if(m==2&&!confirm('"Nur LoRa" schaltet das WLAN aus. Diese Seite ist danach nur ueber den Config-AP erreichbar (Taste 10 s halten). Fortfahren?'))return;
 try{await post('/mode','mode='+m)}catch(e){alert(e.message);return}
 $('mode').value=m;showCtl(m,blState);
 $('ctlmsg').textContent=(m==2||old==2)?'Wird umgeschaltet, die Verbindung zu dieser Seite bricht ab.':'Umgeschaltet.';
}
async function toggleBl(){
 try{const r=JSON.parse(await post('/backlight'));showCtl(curMode,r.on)}catch(e){alert(e.message)}
}
async function scan(){
 const sel=$('nets');sel.innerHTML='<option>Suche...</option>';
 for(let i=0;i<20;i++){
  const r=await (await fetch('/scan')).json();
  if(!r.running){sel.innerHTML='<option value="">- '+r.nets.length+' Netzwerke -</option>'+
   r.nets.map(n=>'<option value="'+esc(n.ssid)+'">'+esc(n.ssid)+' ('+n.rssi+' dBm'+(n.open?', offen':'')+')</option>').join('');return}
  await new Promise(res=>setTimeout(res,1000));
 }
 sel.innerHTML='<option value="">Suche fehlgeschlagen</option>';
}
load();status();setInterval(status,2000);
</script></body></html>)HTML";

const char kPresetHelpPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Modemprofile</title>
<style>
body{font-family:system-ui,sans-serif;margin:0;background:#f2f2f2;color:#222}
header{background:#b71c1c;color:#fff;padding:12px 16px;font-size:1.2em;font-weight:600}
main{max-width:520px;margin:auto;padding:12px}
section{background:#fff;border-radius:8px;padding:12px 16px;margin-bottom:12px;box-shadow:0 1px 3px #0002}
h2{font-size:1em;margin:16px 0 8px}h2:first-child{margin-top:0}
p,li{font-size:.9em;line-height:1.4}
table{width:100%;border-collapse:collapse;font-size:.8em;margin-top:8px}
th,td{text-align:left;padding:4px 6px 4px 0;border-bottom:1px solid #eee;vertical-align:top}
th{color:#666;font-weight:600}
a{color:#b71c1c}
</style></head><body>
<header>Modemprofile</header>
<main><section>
<p><a href="/">Zurueck zu den Einstellungen</a></p>
<h2>Was ein Profil ist</h2>
<p>Das Modemprofil legt fest, wie der Tracker funkt: Frequenz, Bandbreite, Spreizfaktor (SF) und
Fehlerkorrektur. Zwei Geraete hoeren sich nur, wenn Profil und Frequenz-Slot gleich sind.
Das gilt fuer die Tracker untereinander und fuer Meshtastic-Knoten, die Pakete weiterleiten sollen.</p>
<p>Das Profil ist nicht der Meshtastic-Kanal. Der Kanal (Name und Schluessel) verschluesselt den Inhalt.
Der Tracker sendet immer auf dem Kanal UAV-BOS. Den Schluessel stellt "Mesh-Schluessel" ein.
Meshtastic-Geraete koennen die Pakete weiterleiten, ohne sie zu lesen.</p>
<h2>Reichweite und Sendezeit</h2>
<p>Ein hoeherer Spreizfaktor reicht weiter und braucht laenger fuer dasselbe Paket.
Im Band 869,4 bis 869,65 MHz sind 10&nbsp;% Sendezeit pro Stunde erlaubt.
Lange Profile verbrauchen das Budget schnell, dann fallen eigene Positionen aus.</p>
<table>
<tr><th>Profil</th><th>Funk</th><th>Position</th><th>Wann</th></tr>
<tr><td>ShortFast</td><td>SF7, 250 kHz</td><td>0,05 s</td><td>Kurze Strecke, viele Fahrzeuge</td></tr>
<tr><td>ShortSlow</td><td>SF8, 250 kHz</td><td>0,09 s</td><td>Etwas weiter als ShortFast</td></tr>
<tr><td>MediumFast</td><td>SF9, 250 kHz</td><td>0,16 s</td><td>Mittlere Reichweite</td></tr>
<tr><td>MediumSlow</td><td>SF10, 250 kHz</td><td>0,3 s</td><td>Mittlere Reichweite, robuster</td></tr>
<tr><td>LongFast</td><td>SF11, 250 kHz</td><td>0,56 s</td><td>Standard. Die meisten Meshtastic-Netze in der EU nutzen das</td></tr>
<tr><td>LongModerate</td><td>SF11, 125 kHz, CR 4/8</td><td>1,8 s</td><td>Weiter als LongFast, deutlich mehr Sendezeit</td></tr>
<tr><td>LongSlow</td><td>SF12, 125 kHz, CR 4/8</td><td>3,3 s</td><td>Weiteste Reichweite. 30 s Intervall sprengt die 10&nbsp;%</td></tr>
</table>
<p>Die Zeiten gelten fuer eine Positionsmeldung. Weiterleitungen kommen dazu.</p>
<h2>Frequenz</h2>
<ul>
<li>250 kHz (ShortFast bis LongFast): genau ein Platz, immer 869,525 MHz. Der Frequenz-Slot wird ignoriert.</li>
<li>125 kHz (LongModerate, LongSlow): zwei Plaetze. Slot 1 ist 869,4625 MHz, Slot 2 ist 869,5875 MHz.</li>
<li>"Meshtastic-Standard" waehlt den Slot aus dem Profilnamen, so wie Meshtastic es bei einem Kanal ohne
eigenen Namen tut. Hat das Netz vor Ort einen eigenen Kanalnamen, den Slot von Hand auf denselben Wert setzen.</li>
</ul>
<h2>Empfehlung</h2>
<p>LongFast lassen, solange das Meshtastic-Netz vor Ort nichts anderes verwendet.
Vor einem Wechsel das Profil dort nachsehen. Alle Tracker muessen danach dasselbe Profil haben,
sonst hoeren sie sich nicht mehr.</p>
</section></main></body></html>)HTML";

const char kSavedPage[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Gespeichert</title></head>
<body style="font-family:sans-serif;text-align:center;padding-top:40px">
<h2>%MSG%</h2><p>Der Tracker startet neu.</p></body></html>)HTML";

WebServer server(80);
DNSServer dns;
TrackerConfig *cfgRef = nullptr;
bool apMode = false;
bool running = false;
bool reboot = false;
uint32_t activityMs = 0;
bool modeRequested = false;
TrackerMode requestedMode = TrackerMode::WifiOnly;

String jsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 8);
  for (char c : s) {
    switch (c) {
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    default:
      if ((uint8_t)c < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", c);
        out += buf;
      } else {
        out += c;
      }
    }
  }
  return out;
}

// Hides the credential part of the URL (everything after the host) when shown on the vehicle network.
String maskUrl(const String &url) {
  int scheme = url.indexOf("://");
  int pathStart = url.indexOf('/', scheme >= 0 ? scheme + 3 : 0);
  if (pathStart < 0) return url;
  String masked = url.substring(0, pathStart + 1);
  int lastSlash = url.lastIndexOf('/');
  if (lastSlash > pathStart) masked += "...";
  masked += "/***";
  return masked;
}

void touch() { activityMs = millis(); }

void sendSaved(const char *msg) {
  String page = FPSTR(kSavedPage);
  page.replace("%MSG%", msg);
  server.send(200, "text/html", page);
}

void handleRoot() {
  touch();
  server.send_P(200, "text/html", kPage);
}

void handlePresetHelp() {
  touch();
  server.send_P(200, "text/html", kPresetHelpPage);
}

void handleSettings() {
  touch();
  String json = "{";
  json += "\"ssid\":\"" + jsonEscape(cfgRef->wifiSsid) + "\",";
  json += "\"hasPass\":" + String(cfgRef->wifiPass.length() ? "true" : "false") + ",";
  json += "\"hasApPass\":" + String(cfgRef->apPass.length() ? "true" : "false") + ",";
  // The full URL contains the API key; only reveal it on the local config AP.
  json += "\"url\":\"" + (apMode ? jsonEscape(cfgRef->url) : String("")) + "\",";
  json += "\"urlMasked\":\"" + (!apMode && cfgRef->url.length() ? jsonEscape(maskUrl(cfgRef->url)) : String("")) + "\",";
  json += "\"interval\":" + String(cfgRef->intervalSec) + ",";
  json += "\"lorainterval\":" + String(cfgRef->loraIntervalSec) + ",";
  json += "\"mode\":" + String((int)cfgRef->mode) + ",";
  const MeshSettings &ms = cfgRef->mesh;
  json += "\"preset\":" + String((int)ms.preset) + ",\"slot\":" + String(ms.slot) + ",\"hops\":" +
          String(ms.hopLimit) + ",\"txpower\":" + String(ms.txPowerDbm) + ",\"relay\":" + String((int)ms.relayMode) +
          ",\"fairtime\":" + String(ms.foreignAirtimePct) + ",";
  // Like the URL, the mesh key is only revealed on the local config AP.
  bool keySet = cfgRef->meshKey.length() && cfgRef->meshKey != config::kPlaceholderMeshKey;
  json += "\"meshKey\":\"" + (apMode && keySet ? jsonEscape(cfgRef->meshKey) : String("")) + "\",";
  json += "\"meshKeySet\":" + String(keySet ? "true" : "false") + ",";
  json += "\"fw\":\"" FW_VERSION "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\"}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  touch();
  GnssFix f = gnss::current();
  const UplinkStatus &up = uplink::status();

  String wifi;
  if (WiFi.status() == WL_CONNECTED) wifi = WiFi.SSID() + " (" + String(WiFi.RSSI()) + " dBm)";
  else if (apMode) wifi = "Config-AP aktiv";
  else wifi = "nicht verbunden";
  String ip = apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

  char gps[320];
  snprintf(gps, sizeof(gps),
           "{\"valid\":%s,\"sats\":%u,\"hdop\":%.2f,\"lat\":%.7f,\"lon\":%.7f,\"alt\":%.1f,\"speed\":%.2f,"
           "\"heading\":%.1f,\"acc\":%.1f,\"gst\":%s,\"age\":%ld,\"chars\":%lu}",
           f.valid ? "true" : "false", f.satellites, f.hdop, f.latitude, f.longitude, f.altitude, f.speedMps,
           f.heading, f.accuracy, f.accuracyFromGst ? "true" : "false",
           f.ageMs == UINT32_MAX ? -1L : (long)f.ageMs, (unsigned long)gnss::charsProcessed());

  MeshStats m = mesh::stats();
  char meshJson[896];
  snprintf(meshJson, sizeof(meshJson),
           "{\"ok\":%s,\"active\":%s,\"placeholder\":%s,\"node\":\"!%08lx\",\"heard\":%u,\"act\":%u,\"direct\":%u,"
           "\"pos1h\":%u,\"rx\":%lu,\"relay\":%lu,\"relayOwn\":%lu,\"relayForeign\":%lu,\"fdrop\":%lu,"
           "\"viaForeign\":%lu,\"credRetry\":%lu,\"posNoRelay\":%lu,\"scale\":%u,\"tx\":%lu,\"blocked\":%lu,\"lastTx\":%ld,\"air\":%.2f,"
           "\"airTracker\":%.2f,\"rssi\":%d,"
           "\"snr\":%.1f,\"preset\":\"%s\",\"freq\":%.4f,\"sf\":%u,\"bw\":%.0f,\"cr\":%u,\"txp\":%d,\"hops\":%u,"
           "\"error\":\"%s\",\"nodes\":[",
           m.ok ? "true" : "false", m.active ? "true" : "false", m.placeholderKey ? "true" : "false",
           (unsigned long)m.nodeNum, m.heardNodes, m.activeNodes, m.directNodes, m.positionNodes1h,
           (unsigned long)m.rxCount, (unsigned long)m.relayCount, (unsigned long)m.ownRelayCount,
           (unsigned long)m.foreignRelayCount, (unsigned long)m.foreignDropped, (unsigned long)m.viaForeignCount,
           (unsigned long)m.credRetries, (unsigned long)m.positionsNotRelayed, m.intervalScale,
           (unsigned long)m.txCount, (unsigned long)m.txBlocked,
           m.lastTxMs ? (long)((millis() - m.lastTxMs) / 1000) : -1L, m.airtimePercent, m.trackerAirtimePercent,
           m.lastRssi, m.lastSnr,
           m.presetName, m.freqMhz, m.spreadingFactor, m.bandwidthKhz, m.codingRate, m.txPowerDbm, m.hopLimit,
           cfgRef->usesLora() ? m.error : "");

  String nodesJson;
  MeshNodeInfo nodes[32];
  size_t count = mesh::nodes(nodes, 32);
  for (size_t i = 0; i < count; i++) {
    char buf[160];
    snprintf(buf, sizeof(buf),
             "%s{\"id\":\"!%08lx\",\"ago\":%lu,\"pos\":%ld,\"cnt\":%lu,\"hops\":%u,\"mt\":%s,\"rssi\":%d,\"snr\":%.1f}",
             i ? "," : "", (unsigned long)nodes[i].node, (unsigned long)nodes[i].lastAgoSec,
             (long)nodes[i].lastPosAgoSec, (unsigned long)nodes[i].positions, nodes[i].hops,
             nodes[i].viaForeign ? "true" : "false", nodes[i].rssi, nodes[i].snr);
    nodesJson += buf;
  }

  long ago = up.lastAttemptMs ? (long)((millis() - up.lastAttemptMs) / 1000) : -1;
  String json = "{\"mode\":\"" + String(config::modeName(cfgRef->mode)) + "\",\"modeId\":" +
                String((int)cfgRef->mode) + ",\"backlight\":" + (display::backlightOn() ? "true" : "false") +
                ",\"wifi\":\"" + jsonEscape(wifi) + "\",\"ip\":\"" + ip + "\",\"gps\":" + gps;
  json += ",\"up\":{\"code\":" + String(up.lastHttpCode) + ",\"ago\":" + String(ago) +
          ",\"sent\":" + String(up.sentCount) + ",\"fail\":" + String(up.failCount) +
          ",\"error\":\"" + jsonEscape(up.lastError) + "\",\"payload\":\"" + jsonEscape(up.lastPayload) + "\"}";
  json += ",\"mesh\":" + String(meshJson) + nodesJson + "]}";
  if (cfgRef->mode == TrackerMode::Gateway) {
    const GatewayStats &gw = gateway::stats();
    json += ",\"gw\":{\"fwd\":" + String(gw.forwarded) + ",\"fail\":" + String(gw.failed) +
            ",\"drop\":" + String(gw.dropped) + ",\"known\":" + String(gw.knownNodes) + "}";
  }
  json += "}";
  server.send(200, "application/json", json);
}

void handleScan() {
  touch();
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", "{\"running\":true}");
    return;
  }
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    server.send(200, "application/json", "{\"running\":true}");
    return;
  }

  String json = "{\"running\":false,\"nets\":[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    if (!first) json += ",";
    first = false;
    json += "{\"ssid\":\"" + jsonEscape(ssid) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
            ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  json += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleSave() {
  touch();
  TrackerConfig next = *cfgRef;

  long mode = server.hasArg("mode") ? server.arg("mode").toInt() : (long)cfgRef->mode;
  next.mode = (TrackerMode)constrain(mode, (long)TrackerMode::WifiOnly, (long)TrackerMode::Offline);

  String ssid = server.arg("ssid");
  ssid.trim();
  if (!ssid.length() && next.usesWifi()) {
    server.send(400, "text/plain", "SSID fehlt");
    return;
  }
  String pass = server.arg("pass");
  if (pass.length() || ssid != cfgRef->wifiSsid) next.wifiPass = pass;
  next.wifiSsid = ssid;

  String url = server.arg("url");
  url.trim();
  if (url.length()) {
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
      server.send(400, "text/plain", "URL muss mit http:// oder https:// beginnen");
      return;
    }
    next.url = url;
  }
  if (!next.url.length()) {
    server.send(400, "text/plain", "URL fehlt");
    return;
  }

  long interval = server.arg("interval").toInt();
  next.intervalSec = constrain(interval, (long)config::kMinIntervalSec, (long)config::kMaxIntervalSec);
  long loraInterval = server.hasArg("lorainterval") ? server.arg("lorainterval").toInt() : cfgRef->loraIntervalSec;
  next.loraIntervalSec =
      constrain(loraInterval, (long)config::kMinLoraIntervalSec, (long)config::kMaxLoraIntervalSec);

  MeshSettings &ms = next.mesh;
  auto argOr = [](const char *name, long fallback) {
    return server.hasArg(name) && server.arg(name).length() ? server.arg(name).toInt() : fallback;
  };
  long preset = argOr("preset", (long)ms.preset);
  if (preset < 0 || preset >= (long)LoraPreset::Count) {
    server.send(400, "text/plain", "Ungueltiges Modemprofil");
    return;
  }
  ms.preset = (LoraPreset)preset;
  ms.slot = constrain(argOr("slot", ms.slot), 0L, (long)config::kMaxSlot);
  ms.hopLimit = constrain(argOr("hops", ms.hopLimit), (long)config::kMinHopLimit, (long)config::kMaxHopLimit);
  ms.txPowerDbm =
      constrain(argOr("txpower", ms.txPowerDbm), (long)config::kMinTxPowerDbm, (long)config::kMaxTxPowerDbm);
  ms.relayMode = (RelayMode)constrain(argOr("relay", (long)ms.relayMode), (long)RelayMode::All, (long)RelayMode::None);
  ms.foreignAirtimePct = constrain(argOr("fairtime", ms.foreignAirtimePct), 0L, (long)config::kMaxForeignAirtimePct);

  String meshKey = server.arg("meshkey");
  meshKey.trim();
  if (meshKey.length()) {
    if (!config::validMeshKey(meshKey)) {
      server.send(400, "text/plain", "Mesh-Schluessel ungueltig (Base64, 16 oder 32 Byte)");
      return;
    }
    next.meshKey = meshKey;
  }

  String apPass = server.arg("appass");
  if (apPass.length() && apPass.length() < 8) {
    server.send(400, "text/plain", "AP-Passwort min. 8 Zeichen");
    return;
  }
  if (server.hasArg("apclear")) next.apPass = "";
  else if (apPass.length()) next.apPass = apPass;

  config::save(next);
  *cfgRef = next;
  sendSaved("Einstellungen gespeichert");
  reboot = true;
}

void handleMode() {
  touch();
  long m = server.hasArg("mode") ? server.arg("mode").toInt() : -1;
  if (m < (long)TrackerMode::WifiOnly || m > (long)TrackerMode::Offline) {
    server.send(400, "text/plain", "Ungueltige Betriebsart");
    return;
  }
  TrackerMode mode = (TrackerMode)m;
  if (!cfgRef->hasUrl()) {
    server.send(400, "text/plain", "Zuerst die Request-URL speichern");
    return;
  }
  if (mode != TrackerMode::LoraOnly && !cfgRef->hasWifi()) {
    server.send(400, "text/plain", "Zuerst das WLAN (SSID) speichern");
    return;
  }
  requestedMode = mode;
  modeRequested = true;
  server.send(200, "text/plain", "OK");
}

void handleBacklight() {
  touch();
  if (server.hasArg("on")) display::setBacklight(server.arg("on") == "1");
  else display::toggleBacklight();
  server.send(200, "application/json", String("{\"on\":") + (display::backlightOn() ? "true" : "false") + "}");
}

void handleReset() {
  touch();
  config::clear();
  sendSaved("Werkseinstellungen wiederhergestellt");
  reboot = true;
}

void handleNotFound() {
  if (apMode) {
    // Captive portal: send every unknown request (OS connectivity checks included) to the settings page.
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "Not found");
}

// With an AP password set, the web UI also requires it (user "admin"), both on the AP and on the vehicle WiFi.
std::function<void()> guarded(void (*handler)()) {
  return [handler]() {
    if (cfgRef->apPass.length() >= 8 && !server.authenticate("admin", cfgRef->apPass.c_str())) {
      server.requestAuthentication(BASIC_AUTH, "UAV-BOS Tracker");
      return;
    }
    handler();
  };
}

void setupRoutes() {
  static bool registered = false;
  if (registered) return;
  registered = true;
  server.on("/", HTTP_GET, guarded(handleRoot));
  server.on("/hilfe/profil", HTTP_GET, guarded(handlePresetHelp));
  server.on("/settings", HTTP_GET, guarded(handleSettings));
  server.on("/status", HTTP_GET, guarded(handleStatus));
  server.on("/scan", HTTP_GET, guarded(handleScan));
  server.on("/save", HTTP_POST, guarded(handleSave));
  server.on("/reset", HTTP_POST, guarded(handleReset));
  server.on("/mode", HTTP_POST, guarded(handleMode));
  server.on("/backlight", HTTP_POST, guarded(handleBacklight));
  server.onNotFound(handleNotFound);
}

} // namespace

void startAp(TrackerConfig &cfg, const String &apSsid) {
  stop();
  cfgRef = &cfg;
  apMode = true;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA); // STA part is needed for the network scan
  const char *pass = cfg.apPass.length() >= 8 ? cfg.apPass.c_str() : nullptr;
  WiFi.softAP(apSsid.c_str(), pass);
  delay(100);

  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());

  setupRoutes();
  server.begin();
  running = true;
  touch();
  WiFi.scanNetworks(true);
  Serial.printf("[portal] AP %s started on %s\n", apSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

void startSta(TrackerConfig &cfg) {
  stop();
  cfgRef = &cfg;
  apMode = false;
  setupRoutes();
  server.begin();
  running = true;
  Serial.printf("[portal] web UI on http://%s/\n", WiFi.localIP().toString().c_str());
}

void stop() {
  if (!running) return;
  server.stop();
  if (apMode) {
    dns.stop();
    WiFi.softAPdisconnect(true);
  }
  running = false;
  apMode = false;
}

void loop() {
  if (!running) return;
  if (apMode) {
    dns.processNextRequest();
    if (WiFi.softAPgetStationNum() > 0) touch();
  }
  server.handleClient();
}

bool isApMode() { return running && apMode; }
bool rebootRequested() { return reboot; }
uint32_t lastActivityMs() { return activityMs; }
uint8_t apClients() { return apMode ? WiFi.softAPgetStationNum() : 0; }

bool takeModeRequest(TrackerMode &mode) {
  if (!modeRequested) return false;
  modeRequested = false;
  mode = requestedMode;
  return true;
}

} // namespace portal
