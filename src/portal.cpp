#include "portal.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "gnss.h"
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
small{color:#777}code{word-break:break-all}
</style></head><body>
<header>UAV-BOS Fahrzeug-Tracker</header>
<main>
<section><h2>Status</h2><table id="st"><tr><td>Lade...</td></tr></table></section>
<section><h2>Einstellungen</h2>
<form method="POST" action="/save" onsubmit="return chk()">
<label>WLAN (Fahrzeug-Router / Hotspot)</label>
<div class="row"><select id="nets" onchange="if(this.value)ssid.value=this.value"><option value="">- Netzwerke suchen -</option></select>
<button type="button" class="sec" style="margin-top:0;flex:0 0 auto" onclick="scan()">Suchen</button></div>
<label>SSID</label><input name="ssid" id="ssid" maxlength="32" required>
<label>WLAN-Passwort <small id="passhint"></small></label><input name="pass" id="pass" type="password" maxlength="63">
<label>Request-URL (vollstaendig, wird unveraendert per POST aufgerufen)</label>
<input name="url" id="url" type="url" placeholder="https://api.beta.uav-bos.de/telemetry/objects/vehicle-key/api-key">
<small id="urlhint"></small>
<label>Sendeintervall (Sekunden)</label><input name="interval" id="interval" type="number" min="1" max="3600" required>
<label>Passwort fuer Config-AP und Weboberflaeche, Benutzer "admin" <small id="aphint">(min. 8 Zeichen, leer = kein Passwort)</small></label>
<input name="appass" id="appass" type="password" maxlength="63">
<label id="apclrrow" style="display:none"><input type="checkbox" name="apclear" value="1" style="width:auto"> Passwort entfernen</label>
<button type="submit">Speichern &amp; Neustart</button>
</form></section>
<section><h2>Wartung</h2>
<form method="POST" action="/reset" onsubmit="return confirm('Alle Einstellungen loeschen?')">
<button class="sec" type="submit">Werkseinstellungen</button></form>
<small id="fw"></small></section>
</main>
<script>
const $=id=>document.getElementById(id);
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function chk(){const a=$('appass').value;if(a&&a.length<8){alert('AP-Passwort min. 8 Zeichen');return false}return true}
async function load(){
 const s=await (await fetch('/settings')).json();
 $('ssid').value=s.ssid;$('interval').value=s.interval;$('url').value=s.url;
 $('passhint').textContent=s.hasPass?'(leer = unveraendert)':'';
 if(s.hasApPass){$('aphint').textContent='(gesetzt, leer = unveraendert)';$('apclrrow').style.display='block'}
 $('urlhint').textContent=s.urlMasked?'Gespeicherte URL: '+s.urlMasked+' - leer lassen = unveraendert':'';
 $('fw').textContent='Firmware '+s.fw+' | '+s.mac;
}
async function status(){
 try{const s=await (await fetch('/status')).json();
 const up=s.up,g=s.gps;
 const sendOk=up.code>=200&&up.code<300;
 let r=[['WLAN',s.wifi],['IP',s.ip],
 ['GPS',g.valid?'<span class="ok">Fix</span>':'<span class="err">kein Fix</span>'],
 ['Satelliten / HDOP',g.sats+' / '+g.hdop.toFixed(1)],
 ['Position',g.age>=0?g.lat.toFixed(6)+', '+g.lon.toFixed(6):'-'],
 ['Hoehe / Geschw.',g.alt.toFixed(0)+' m / '+(g.speed*3.6).toFixed(0)+' km/h'],
 ['Kurs / Genauigkeit',g.heading.toFixed(0)+'&deg; / '+g.acc.toFixed(1)+' m'+(g.gst?' (GST)':' (HDOP)')],
 ['Letztes Senden',up.ago<0?'-':'vor '+up.ago+' s, <span class="'+(sendOk?'ok':'err')+'">'+up.code+'</span>'],
 ['Gesendet / Fehler',up.sent+' / '+up.fail]];
 if(up.error)r.push(['Fehler','<code>'+esc(up.error)+'</code>']);
 if(up.payload)r.push(['Letzte Daten','<code>'+esc(up.payload)+'</code>']);
 $('st').innerHTML=r.map(x=>'<tr><td>'+x[0]+'</td><td>'+x[1]+'</td></tr>').join('');
 }catch(e){}
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

  long ago = up.lastAttemptMs ? (long)((millis() - up.lastAttemptMs) / 1000) : -1;
  String json = "{\"wifi\":\"" + jsonEscape(wifi) + "\",\"ip\":\"" + ip + "\",\"gps\":" + gps;
  json += ",\"up\":{\"code\":" + String(up.lastHttpCode) + ",\"ago\":" + String(ago) +
          ",\"sent\":" + String(up.sentCount) + ",\"fail\":" + String(up.failCount) +
          ",\"error\":\"" + jsonEscape(up.lastError) + "\",\"payload\":\"" + jsonEscape(up.lastPayload) + "\"}}";
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

  String ssid = server.arg("ssid");
  ssid.trim();
  if (!ssid.length()) {
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
  server.on("/settings", HTTP_GET, guarded(handleSettings));
  server.on("/status", HTTP_GET, guarded(handleStatus));
  server.on("/scan", HTTP_GET, guarded(handleScan));
  server.on("/save", HTTP_POST, guarded(handleSave));
  server.on("/reset", HTTP_POST, guarded(handleReset));
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

} // namespace portal
