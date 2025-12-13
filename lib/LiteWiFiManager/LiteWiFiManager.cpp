#include "LiteWiFiManager.h"

LiteWiFiManager::LiteWiFiManager(Print *logger) : _server(80) {
  (void)logger;  // logger kept for API compatibility; printing gated by DEBUG
}

#define WM_LOG(msg) DBG_LOG("*wm:", msg)
#define WM_LOGF(fmt, ...) DBG_LOGF("*wm:", fmt, ##__VA_ARGS__)

bool LiteWiFiManager::begin(const char *apSsid,
                            const char *apPassword,
                            unsigned long configPortalTimeoutMs,
                            bool forcePortal) {
  WiFi.persistent(false);  // avoid flash writes unless saving new creds

  if (!forcePortal && connectWithStored()) {
    return true;
  }

  startPortal(apSsid, apPassword, configPortalTimeoutMs);

  // Blocking loop until portal ends or times out; the user can also call loop()
  // manually and exit this function sooner if desired.
  while (_portalActive) {
    loop();
    delay(10);
  }

  return isConnected();
}

void LiteWiFiManager::loop() {
  if (_portalActive) {
    _dns.processNextRequest();
    _server.handleClient();

    if (_connectRequested) {
      _connectRequested = false;
      stopPortal();
      connectWithNew(_newSsid, _newPass);
    }

    if (_portalDeadline > 0 && millis() > _portalDeadline) {
      WM_LOG("Config portal timeout");
      stopPortal();
    }
  }
}

bool LiteWiFiManager::connectWithStored(unsigned long connectTimeoutMs) {
  WiFi.mode(WIFI_STA);
  String ssid = WiFi.SSID();
  if (ssid.length() == 0) {
    WM_LOG("No stored WiFi credentials");
    return false;
  }

  WiFi.begin();

  WM_LOGF("Connecting to stored SSID: %s\n", ssid.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < connectTimeoutMs) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setAutoReconnect(true);
    WM_LOG("Connected with stored credentials");
    return true;
  }

  WM_LOG("Stored credentials failed");
  return false;
}

bool LiteWiFiManager::connectWithNew(const String &ssid,
                                     const String &password,
                                     unsigned long connectTimeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);  // write to native storage
  WiFi.begin(ssid.c_str(), password.c_str());
  WiFi.persistent(false);

  WM_LOGF("Connecting to %s\n", ssid.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < connectTimeoutMs) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setAutoReconnect(true);
    WM_LOG("Connected and saved");
    return true;
  }

  WM_LOG("Connection failed");
  return false;
}

void LiteWiFiManager::startPortal(const char *apSsid,
                                  const char *apPassword,
                                  unsigned long timeoutMs) {
  WiFi.mode(WIFI_AP_STA);
  bool apResult = WiFi.softAP(apSsid, apPassword);
  if (apResult) {
    WM_LOGF("AP started: %s\n", apSsid);
  } else {
    WM_LOG("Failed to start AP");
  }

  IPAddress apIp = WiFi.softAPIP();
  _dns.start(53, "*", apIp);

  _server.on("/", [this]() { handleRoot(); });
  _server.on("/save", [this]() { handleSave(); });
  _server.on("/scan", [this]() { handleScan(); });
  _server.onNotFound([this]() {
    String redirect = String("http://") + WiFi.softAPIP().toString();
    _server.sendHeader("Location", redirect, true);
    _server.send(302, "text/plain", "");
  });
  _server.begin();

  _portalActive = true;
  _portalDeadline = timeoutMs > 0 ? millis() + timeoutMs : 0;
}

void LiteWiFiManager::stopPortal() {
  if (_portalActive) {
    _dns.stop();
    _server.stop();
    WiFi.softAPdisconnect(true);
    _portalActive = false;
  }
}

void LiteWiFiManager::handleRoot() {
  const char *page =
      "<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>WiFi Setup</title>"
      "<style>"
      "body{margin:0;padding:0;font-family:Segoe UI,system-ui,-apple-system,sans-serif;"
      "background:radial-gradient(circle at 15% 20%,rgba(56,99,255,0.18),transparent 25%),"
      "radial-gradient(circle at 85% 10%,rgba(0,200,255,0.2),transparent 22%),#0b1020;"
      "color:#e6e9f0;display:flex;align-items:flex-start;justify-content:center;min-height:100vh;padding:24px 12px;box-sizing:border-box;}"
      ".card{background:#11182a;border:1px solid #1f2940;border-radius:16px;"
      "padding:24px 22px;box-shadow:0 18px 60px rgba(0,0,0,0.38);width:100%;max-width:620px;}"
      "h2{margin:0 0 8px;font-weight:700;letter-spacing:-0.3px;}"
      ".sub{margin:0 0 14px;color:#9fb0cc;font-size:14px;}"
      "label{display:block;font-size:14px;margin:14px 0 6px;color:#b8c2d6;}"
      "input,select,button{width:100%;box-sizing:border-box;font-size:15px;border-radius:10px;"
      "border:1px solid #26324b;padding:12px;background:#0f1524;color:#e6e9f0;outline:none;"
      "transition:border-color .2s,box-shadow .2s;}"
      "input:focus,select:focus{border-color:#4da3ff;box-shadow:0 0 0 3px rgba(77,163,255,0.18);}"
      ".pass-wrap{display:flex;align-items:stretch;gap:10px;}"
      ".pass-wrap input{flex:1;}"
      ".toggle-pass{width:90px;display:flex;align-items:center;justify-content:center;"
      "padding:12px;background:#16233a;border:1px solid #26324b;border-radius:10px;"
      "color:#7fb6ff;font-size:12px;font-weight:700;cursor:pointer;}"
      "button{margin-top:18px;background:linear-gradient(135deg,#1f6feb,#52b6ff);"
      "border:none;font-weight:700;cursor:pointer;box-shadow:0 10px 30px rgba(31,111,235,0.35);}"
      "button:hover{transform:translateY(-1px);}"
      ".row{display:flex;gap:10px;align-items:center;flex-wrap:wrap;}"
      ".status{margin-top:12px;font-size:13px;color:#9fb0cc;}"
      ".list{margin-top:12px;max-height:240px;overflow:auto;border:1px solid #26324b;border-radius:12px;}"
      ".item{padding:12px;cursor:pointer;display:flex;justify-content:space-between;align-items:center;"
      "border-bottom:1px solid #1c2438;background:#0f1524;}"
      ".item:last-child{border-bottom:none;}"
      ".item:hover{background:#16233a;}"
      ".ssid{font-weight:600;font-size:14px;}"
      ".badge{background:#24324f;color:#c7d4ec;font-size:11px;padding:4px 8px;border-radius:20px;}"
      "</style>"
      "</head><body><div class='card'>"
      "<h2>Wi‑Fi Setup</h2><p class='sub'>Connect the device to your Wi‑Fi. Scan and pick a network, then add the password.</p>"
      "<div class='row'><select id='networks'><option>Scanning...</option></select>"
      "<button type='button' id='scanBtn' style='width:140px'>Rescan</button></div>"
      "<div class='list' id='list'></div>"
      "<form method='POST' action='/save'>"
      "<label>SSID</label><input name='ssid' id='ssid' maxlength='31' required>"
      "<label>Password</label><div class='pass-wrap'><input name='pass' id='pass' type='password' maxlength='63'>"
      "<button class='toggle-pass' id='togglePass' type='button'>Show</button></div>"
      "<button type='submit'>Save and connect</button>"
      "<div class='status' id='status'>Select a network or enter SSID and password.</div>"
      "</form>"
      "</div>"
      "<script>"
      "const sel=document.getElementById('networks');"
      "const list=document.getElementById('list');"
      "const ssid=document.getElementById('ssid');"
      "const pass=document.getElementById('pass');"
      "const togglePass=document.getElementById('togglePass');"
      "const statusEl=document.getElementById('status');"
      "const scanBtn=document.getElementById('scanBtn');"
      "function render(nets){"
      " sel.innerHTML='';list.innerHTML='';"
      " nets.forEach(n=>{"
      "  const opt=document.createElement('option');opt.textContent=n.ssid;opt.value=n.ssid;sel.appendChild(opt);"
      "  const div=document.createElement('div');div.className='item';"
      "  div.innerHTML=`<span class='ssid'>${n.ssid}</span><span class='badge'>${n.rssi} dBm</span>`;"
      "  div.onclick=()=>{ssid.value=n.ssid;statusEl.textContent='SSID set.';sel.value=n.ssid;};list.appendChild(div);"
      " });"
      " if(!nets.length){sel.innerHTML='<option>No networks found</option>';list.innerHTML='<div class=\"item\"><span class=\"ssid\">No networks found</span></div>';}"
      "}"
      "async function scan(){statusEl.textContent='Scanning...';scanBtn.disabled=true;"
      " try{const res=await fetch('/scan');const data=await res.json();render(data.networks||[]);statusEl.textContent='Pick a network or enter it manually.';}"
      " catch(e){statusEl.textContent='Scan failed';}"
      " scanBtn.disabled=false;"
      "}"
      "scanBtn.onclick=scan;"
      "scan();"
      "sel.onchange=()=>{ssid.value=sel.value;statusEl.textContent='SSID set.';};"
      "togglePass.onclick=()=>{"
      " if(pass.type==='password'){pass.type='text';togglePass.textContent='Hide';}"
      " else{pass.type='password';togglePass.textContent='Show';}"
      "};"
      "</script>"
      "</body></html>";

  _server.send(200, "text/html", page);
}

void LiteWiFiManager::handleSave() {
  if (!_server.hasArg("ssid")) {
    _server.send(400, "text/plain", "Missing ssid");
    return;
  }

  _newSsid = _server.arg("ssid");
  _newPass = _server.hasArg("pass") ? _server.arg("pass") : "";
  _newSsid.trim();
  _newPass.trim();

  if (_newSsid.isEmpty()) {
    _server.send(400, "text/plain", "SSID required");
    return;
  }

  _server.send(200, "text/plain",
               "Saved. Closing portal and connecting...");

  _connectRequested = true;
}

void LiteWiFiManager::handleScan() {
  int n = WiFi.scanNetworks();
  String json = "{\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]}";
  _server.send(200, "application/json", json);
}
