#pragma once

#include <Arduino.h>

#if !defined(ARDUINO_ARCH_ESP32) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "ESP32KeyBridgeWeb supports ESP32-S3 only"
#endif

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "ESP32KeyBridgeMacroSettings.h"
#include "ESP32KeyBridgeMacroSettingsJson.h"
#include "ESP32KeyBridgeSettings.h"
#include "ESP32KeyBridgeSettingsJson.h"
#include "ESP32KeyBridgeSettingsNavigation.h"

namespace esp32keybridge
{
namespace detail
{
static const char kSettingsIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 KeyBridge</title>
<style>
:root{color-scheme:dark;--bg:#101512;--panel:#19211c;--line:#304137;--text:#f1f6f2;--muted:#a8b8ad;--accent:#74d49a;--warn:#f3c969}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#1e3025 0,var(--bg) 48%);color:var(--text);font:16px/1.45 system-ui,sans-serif}
main{width:min(720px,calc(100% - 32px));margin:0 auto;padding:44px 0 60px}header{margin-bottom:28px}.eyebrow{color:var(--accent);font-size:.78rem;font-weight:700;letter-spacing:.14em;text-transform:uppercase}
h1{font-size:clamp(2rem,8vw,3.4rem);line-height:1;margin:.3rem 0 .8rem}.lede{color:var(--muted);max-width:55ch}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:14px}
.card{background:color-mix(in srgb,var(--panel) 94%,transparent);border:1px solid var(--line);border-radius:18px;padding:20px;box-shadow:0 16px 50px #0004}.wide{grid-column:1/-1}
.label{color:var(--muted);font-size:.78rem;letter-spacing:.09em;text-transform:uppercase}.value{font-size:1.45rem;font-weight:650;margin-top:6px;word-break:break-word}
.status{display:inline-flex;align-items:center;gap:8px}.dot{width:10px;height:10px;border-radius:50%;background:var(--warn);box-shadow:0 0 16px var(--warn)}.dot.on{background:var(--accent);box-shadow:0 0 16px var(--accent)}
select{width:100%;margin-top:8px;padding:11px 12px;border:1px solid var(--line);border-radius:10px;background:#111813;color:var(--text);font:inherit}label.card{display:block}.check{display:flex;align-items:center;gap:12px;margin-top:13px;font-size:1.08rem}.check input{width:20px;height:20px;accent-color:var(--accent)}
.macro-card{margin-top:16px}.macro-row{display:grid;grid-template-columns:minmax(120px,.35fr) 1fr;gap:12px;margin-top:12px}.macro-row textarea{width:100%;min-height:48px;resize:vertical;padding:10px 12px;border:1px solid var(--line);border-radius:10px;background:#111813;color:var(--text);font:inherit}.hint{color:var(--muted);font-size:.88rem;margin:6px 0 0}@media(max-width:600px){.macro-row{grid-template-columns:1fr}}
.actions{display:flex;align-items:center;gap:14px;margin-top:18px}button{border:0;border-radius:12px;background:var(--accent);color:#0d1710;padding:12px 20px;font:700 1rem system-ui;cursor:pointer}button:disabled{opacity:.45;cursor:not-allowed}.message{color:var(--muted)}.message.error{color:#ff9f94}
footer{color:var(--muted);font-size:.86rem;margin-top:22px}code{color:var(--accent)}#keyboard-status{display:block;margin-top:7px}
</style>
</head>
<body>
<main>
<header><div class="eyebrow">Local device settings</div><h1>ESP32 KeyBridge</h1><p class="lede">Choose how the attached keyboard is engraved and how the connected host interprets keys. Saving verifies the new NVS record before applying it.</p></header>
<form id="settings-form">
<section class="grid">
<label class="card"><span class="label">Keyboard layout</span><select id="input-layout" required></select></label>
<label class="card"><span class="label">Host layout</span><select id="host-layout" required></select></label>
<label class="card"><span class="label">Layout conversion</span><span class="check"><input id="conversion" type="checkbox">Start enabled</span></label>
<label class="card"><span class="label">Toggle key</span><select id="toggle"><option value="0">Disabled</option><option value="72">Pause</option><option value="71">Scroll Lock</option><option value="69">F12</option></select></label>
<article class="card wide"><div class="label">Device status</div><div class="value" id="device-status">Connecting…</div></article>
</section>
<div class="actions"><button id="save" type="submit">Save settings</button><span class="message" id="save-status" role="status"></span></div>
</form>
<form id="macro-form" class="card macro-card">
<div class="label">Text macros</div><div class="value">Function-key shortcuts</div>
<p class="hint">Choose a unique F1–F24 trigger and up to 31 characters. Disabled slots must be empty.</p>
<div id="macro-slots"></div>
<div class="actions"><button id="save-macros" type="submit">Save macros</button><span class="message" id="macro-status" role="status"></span></div>
</form>
<footer>Served directly by the ESP32-S3. API: <code>/api/v1/config</code>, <code>/api/v1/macros</code>, and <code>/api/v1/status</code>.<span id="keyboard-status">Checking wired keyboard navigation…</span></footer>
</main>
<script>
const $=id=>document.getElementById(id);
let editable=false;
let macroAvailable=false;
async function json(path){const r=await fetch(path,{cache:'no-store'});if(!r.ok)throw new Error(r.status);return r.json()}
function message(text,error=false){$('save-status').textContent=text;$('save-status').classList.toggle('error',error)}
function macroMessage(text,error=false){$('macro-status').textContent=text;$('macro-status').classList.toggle('error',error)}
function ensureToggle(value){const s=$('toggle');if(![...s.options].some(o=>Number(o.value)===value)){const o=document.createElement('option');o.value=value;o.textContent='HID 0x'+value.toString(16).padStart(2,'0').toUpperCase();s.append(o)}s.value=String(value)}
async function load(){try{const [l,c]=await Promise.all([json('/api/v1/layouts'),json('/api/v1/config')]);for(const id of ['input-layout','host-layout']){const s=$(id);s.replaceChildren(...l.layouts.map(name=>{const o=document.createElement('option');o.value=name;o.textContent=name;return o}))}$('input-layout').value=c.inputLayout;$('host-layout').value=c.hostLayout;$('conversion').checked=c.layoutConversionEnabled;ensureToggle(c.layoutConversionToggleUsage);editable=!!c.editable;$('save').disabled=!editable;if(!editable)message('Read-only mode')}catch(e){editable=false;message('Configuration unavailable',true);$('save').disabled=true}}
function macroOptions(){const values=[[0,'Disabled']];for(let i=1;i<=12;i++)values.push([57+i,'F'+i]);for(let i=13;i<=24;i++)values.push([91+i,'F'+i]);return values}
function buildMacroRows(){const root=$('macro-slots');for(let i=0;i<4;i++){const row=document.createElement('div');row.className='macro-row';const label=document.createElement('label');label.textContent='Macro '+(i+1);const select=document.createElement('select');select.id='macro-trigger-'+i;for(const [value,name] of macroOptions()){const o=document.createElement('option');o.value=value;o.textContent=name;select.append(o)}label.append(select);const text=document.createElement('textarea');text.id='macro-text-'+i;text.maxLength=124;text.placeholder='Text typed when the trigger is pressed';select.addEventListener('change',()=>{text.disabled=select.value==='0';if(text.disabled)text.value=''});row.append(label,text);root.append(row)}}
async function loadMacros(){try{const m=await json('/api/v1/macros');m.slots.forEach((slot,i)=>{const trigger=$('macro-trigger-'+i),text=$('macro-text-'+i);trigger.value=String(slot.triggerUsage);text.value=slot.text;text.disabled=slot.triggerUsage===0});macroAvailable=true;$('save-macros').disabled=!editable;if(!editable)macroMessage('Read-only mode')}catch(e){macroAvailable=false;macroMessage('Macro configuration unavailable',true);$('save-macros').disabled=true}}
async function refreshStatus(){try{const s=await json('/api/v1/status');$('device-status').textContent=`Online · conversion ${s.layoutConversionEnabled?'on':'off'} · ${s.wifiClients} phone${s.wifiClients===1?'':'s'} connected${s.mergedOverflow||s.outputOverflow?' · key overflow':''}`}catch(e){$('device-status').textContent='Device disconnected'}}
function controls(){return [...document.querySelectorAll('select,input,textarea,button')].filter(e=>!e.disabled)}
function moveFocus(delta){const c=controls();if(!c.length)return;let i=c.indexOf(document.activeElement);if(i<0)i=delta>0?-1:0;i=(i+delta+c.length)%c.length;c[i].focus()}
function navigate(action){const e=document.activeElement;if(action==='next'){moveFocus(1);return}if(action==='previous'){moveFocus(-1);return}if(action==='cancel'){load();loadMacros();return}if(!e||e===document.body){moveFocus(1);return}if(e.tagName==='SELECT'&&['up','down','left','right','home','end'].includes(action)){const last=e.options.length-1;e.selectedIndex=action==='home'?0:action==='end'?last:Math.max(0,Math.min(last,e.selectedIndex+(['up','left'].includes(action)?-1:1)));e.dispatchEvent(new Event('change'));return}if(e.type==='checkbox'){if(action==='activate')e.checked=!e.checked;else if(['up','left'].includes(action))e.checked=false;else if(['down','right'].includes(action))e.checked=true;return}if(action==='activate'){if(e.tagName==='BUTTON')e.click();else moveFocus(1);return}if(action==='home'){const c=controls();if(c.length)c[0].focus();return}if(action==='end'){const c=controls();if(c.length)c[c.length-1].focus();return}if(['up','left'].includes(action))moveFocus(-1);else if(['down','right'].includes(action))moveFocus(1)}
let navWasAvailable=false;
async function pollNavigation(){if(document.hidden){navWasAvailable=false;$('keyboard-status').textContent='Wired keyboard navigation paused';setTimeout(pollNavigation,250);return}try{const n=await json('/api/v1/navigation');$('keyboard-status').textContent=!n.available?'Wired keyboard navigation unavailable':n.capture?'Wired keyboard navigation active: Tab/arrows, Enter/Space, Esc':'Release all keys to start wired keyboard navigation';if(n.capture&&!navWasAvailable&&document.activeElement===document.body)moveFocus(1);navWasAvailable=!!n.capture;if(n.action&&n.action!=='none')navigate(n.action)}catch(e){navWasAvailable=false;$('keyboard-status').textContent='Wired keyboard navigation disconnected'}finally{setTimeout(pollNavigation,80)}}
$('settings-form').addEventListener('submit',async e=>{e.preventDefault();$('save').disabled=true;message('Saving…');const body={schemaVersion:1,inputLayout:$('input-layout').value,hostLayout:$('host-layout').value,layoutConversionEnabled:$('conversion').checked,layoutConversionToggleUsage:Number($('toggle').value)};try{const r=await fetch('/api/v1/config',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const result=await r.json();if(!r.ok)throw new Error(result.message||result.error||r.status);message('Saved and applied');await load();await refreshStatus()}catch(err){message('Save failed: '+err.message,true)}finally{$('save').disabled=!editable}});
$('macro-form').addEventListener('submit',async e=>{e.preventDefault();$('save-macros').disabled=true;macroMessage('Saving…');const body={schemaVersion:1,slots:[0,1,2,3].map(i=>({triggerUsage:Number($('macro-trigger-'+i).value),text:$('macro-text-'+i).value}))};try{const r=await fetch('/api/v1/macros',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const result=await r.json();if(!r.ok)throw new Error(result.message||result.error||r.status);macroMessage('Saved and applied');await loadMacros()}catch(err){macroMessage('Save failed: '+err.message,true)}finally{$('save-macros').disabled=!editable||!macroAvailable}});
buildMacroRows();load().then(loadMacros);refreshStatus();pollNavigation();setInterval(refreshStatus,1500);
</script>
</body>
</html>)HTML";
} // namespace detail

enum class SettingsWebErrorCode : uint8_t
{
  None = 0,
  InvalidSsid,
  InvalidPassword,
  InvalidChannel,
  InvalidMaxConnections,
  WifiStartFailed,
  DnsStartFailed,
};

struct SettingsWebError
{
  SettingsWebErrorCode code = SettingsWebErrorCode::None;
  const char *message = nullptr;
};

struct SettingsAccessPointConfig
{
  const char *ssid = nullptr;
  const char *password = nullptr;
  uint8_t channel = 1;
  bool hidden = false;
  uint8_t maxConnections = 1;
  bool captivePortal = false;
};

// ESP32-S3 SoftAP plus the local settings website. The sketch owns scheduling
// and must call update() frequently from loop(). A const construction remains
// read-only; the mutable construction plus enableEditing() adds the Phase 3
// validated, persisted PUT endpoint. Phase 4 can bind a keyboard-navigation
// source whose capture lease is held only while the browser page is polling.
// Phase 6 optionally adds wildcard DNS and captive-portal probe redirects.
class Esp32S3SettingsWebServer
{
public:
  Esp32S3SettingsWebServer(const KeyboardLayoutSettings &settings,
                           const ESP32KeyBridge &bridge, uint16_t port = 80)
      : settings_(&settings), bridge_(&bridge), server_(port)
  {
  }

  Esp32S3SettingsWebServer(KeyboardLayoutSettings &settings,
                           ESP32KeyBridge &bridge, uint16_t port = 80)
      : settings_(&settings), bridge_(&bridge), editableSettings_(&settings),
        editableBridge_(&bridge), server_(port)
  {
  }

  // The referenced objects must outlive this server. Because request handling
  // occurs inside update(), save/apply runs in the same context as the
  // sketch's bridge.update() call rather than a Wi-Fi task.
  bool enableEditing(KeyboardLayoutSettingsStore &store,
                     ESP32KeyBridgeConfig &config, InputConfig &inputConfig)
  {
    if (editableSettings_ == nullptr || editableBridge_ == nullptr || started_ ||
        (macroConfig_ != nullptr && macroConfig_ != &config))
    {
      return false;
    }
    store_ = &store;
    config_ = &config;
    inputConfig_ = &inputConfig;
    return true;
  }

  bool editingEnabled() const
  {
    return editableSettings_ != nullptr && editableBridge_ != nullptr &&
           store_ != nullptr && config_ != nullptr && inputConfig_ != nullptr;
  }

  bool enableKeyboardNavigation(SettingsNavigationSource &navigation)
  {
    if (started_)
    {
      return false;
    }
    navigation_ = &navigation;
    return true;
  }

  bool enableMacroEditing(KeyboardMacroSettings &settings,
                          KeyboardMacroSettingsStore &store,
                          ESP32KeyBridgeConfig &config)
  {
    if (editableBridge_ == nullptr || started_ ||
        (config_ != nullptr && config_ != &config))
    {
      return false;
    }
    macroSettings_ = &settings;
    macroStore_ = &store;
    macroConfig_ = &config;
    return true;
  }

  bool begin(const SettingsAccessPointConfig &config,
             SettingsWebError *error = nullptr)
  {
    clearError(error);
    if (started_)
    {
      return true;
    }
    if (!validLength(config.ssid, 1, 32))
    {
      return fail(error, SettingsWebErrorCode::InvalidSsid,
                  "SoftAP SSID must contain 1 to 32 bytes");
    }
    if (!validLength(config.password, 8, 63))
    {
      return fail(error, SettingsWebErrorCode::InvalidPassword,
                  "SoftAP password must contain 8 to 63 bytes");
    }
    if (config.channel < 1 || config.channel > 13)
    {
      return fail(error, SettingsWebErrorCode::InvalidChannel,
                  "SoftAP channel must be between 1 and 13");
    }
    if (config.maxConnections < 1 || config.maxConnections > 4)
    {
      return fail(error, SettingsWebErrorCode::InvalidMaxConnections,
                  "SoftAP maxConnections must be between 1 and 4");
    }

    if (!handlersRegistered_)
    {
      registerHandlers();
      handlersRegistered_ = true;
    }
    if (!WiFi.mode(WIFI_AP) ||
        !WiFi.softAP(config.ssid, config.password, config.channel,
                     config.hidden ? 1 : 0, config.maxConnections))
    {
      WiFi.softAPdisconnect(true);
      return fail(error, SettingsWebErrorCode::WifiStartFailed,
                  "could not start the ESP32-S3 SoftAP");
    }

    captivePortalEnabled_ = config.captivePortal;
    if (captivePortalEnabled_ &&
        !dnsServer_.start(53, "*", WiFi.softAPIP()))
    {
      captivePortalEnabled_ = false;
      WiFi.softAPdisconnect(true);
      return fail(error, SettingsWebErrorCode::DnsStartFailed,
                  "could not start captive-portal DNS");
    }

    server_.begin();
    started_ = true;
    return true;
  }

  void update()
  {
    if (started_)
    {
      if (captivePortalEnabled_)
      {
        dnsServer_.processNextRequest();
      }
      server_.handleClient();
      if (navigationLeaseActive_ &&
          static_cast<uint32_t>(millis() - lastNavigationPollMs_) >
              NavigationLeaseMilliseconds)
      {
        navigation_->setNavigationCaptureEnabled(false);
        navigationLeaseActive_ = false;
      }
    }
  }

  void end()
  {
    if (!started_)
    {
      return;
    }
    server_.stop();
    dnsServer_.stop();
    WiFi.softAPdisconnect(true);
    releaseNavigationCapture();
    captivePortalEnabled_ = false;
    started_ = false;
  }

  bool started() const { return started_; }
  bool captivePortalEnabled() const { return started_ && captivePortalEnabled_; }
  IPAddress address() const { return started_ ? WiFi.softAPIP() : IPAddress(); }
  uint8_t connectedClients() const { return started_ ? WiFi.softAPgetStationNum() : 0; }

private:
  static bool validLength(const char *value, size_t minimum, size_t maximum)
  {
    if (value == nullptr)
    {
      return false;
    }
    size_t length = 0;
    while (value[length] != '\0')
    {
      if (++length > maximum)
      {
        return false;
      }
    }
    return length >= minimum;
  }

  static void clearError(SettingsWebError *error)
  {
    if (error != nullptr)
    {
      *error = SettingsWebError();
    }
  }

  static bool fail(SettingsWebError *error, SettingsWebErrorCode code,
                   const char *message)
  {
    if (error != nullptr)
    {
      error->code = code;
      error->message = message;
    }
    return false;
  }

  void commonHeaders()
  {
    server_.sendHeader("Cache-Control", "no-store");
    server_.sendHeader("X-Content-Type-Options", "nosniff");
    server_.sendHeader("X-Frame-Options", "DENY");
    server_.sendHeader(
        "Content-Security-Policy",
        "default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'");
  }

  void registerHandlers()
  {
    server_.on("/", HTTP_GET, [this]() {
      commonHeaders();
      server_.send_P(200, PSTR("text/html; charset=utf-8"),
                     detail::kSettingsIndexHtml);
    });
    server_.on("/api/v1/config", HTTP_GET, [this]() { sendConfig(); });
    server_.on("/api/v1/config", HTTP_PUT, [this]() { updateConfig(); });
    server_.on("/api/v1/layouts", HTTP_GET, [this]() { sendLayouts(); });
    server_.on("/api/v1/macros", HTTP_GET, [this]() { sendMacros(); });
    server_.on("/api/v1/macros", HTTP_PUT, [this]() { updateMacros(); });
    server_.on("/api/v1/navigation", HTTP_GET, [this]() { sendNavigation(); });
    server_.on("/api/v1/status", HTTP_GET, [this]() { sendStatus(); });
    server_.on("/healthz", HTTP_GET, [this]() {
      commonHeaders();
      server_.send(200, "text/plain; charset=utf-8", "ok\n");
    });
    server_.onNotFound([this]() {
      if (captivePortalEnabled_ && server_.method() == HTTP_GET &&
          server_.uri() != "/api" && !server_.uri().startsWith("/api/"))
      {
        redirectToPortal();
        return;
      }
      commonHeaders();
      server_.send(404, "application/json; charset=utf-8",
                   "{\"error\":\"not_found\"}\n");
    });
  }

  void redirectToPortal()
  {
    const String location = String("http://") + WiFi.softAPIP().toString() + "/";
    commonHeaders();
    server_.sendHeader("Location", location, true);
    server_.send(302, "text/plain; charset=utf-8",
                 "Open ESP32 KeyBridge settings\n");
  }

  void sendConfig()
  {
    char body[256];
    snprintf(body, sizeof(body),
             "{\"schemaVersion\":%lu,\"inputLayout\":\"%s\","
             "\"hostLayout\":\"%s\",\"layoutConversionEnabled\":%s,"
             "\"layoutConversionToggleUsage\":%u,\"editable\":%s}\n",
             static_cast<unsigned long>(KeyboardLayoutSettings::CurrentVersion),
             settings_->inputLayoutName(), settings_->hostLayoutName(),
             settings_->layoutConversionEnabled() ? "true" : "false",
             static_cast<unsigned>(settings_->layoutConversionToggleUsage()),
             editingEnabled() ? "true" : "false");
    commonHeaders();
    server_.send(200, "application/json; charset=utf-8", body);
  }

  void sendLayouts()
  {
    char body[256] = {};
    size_t used = static_cast<size_t>(snprintf(body, sizeof(body), "{\"layouts\":["));
    for (size_t i = 0; i < KeyboardLayout::presetCount() && used < sizeof(body); ++i)
    {
      const char *name = KeyboardLayout::presetName(i);
      const int added = snprintf(body + used, sizeof(body) - used,
                                 "%s\"%s\"", i == 0 ? "" : ",", name);
      if (added < 0 || static_cast<size_t>(added) >= sizeof(body) - used)
      {
        sendApiError(500, "internal_error", nullptr,
                     "layout registry response is too large");
        return;
      }
      used += static_cast<size_t>(added);
    }
    if (snprintf(body + used, sizeof(body) - used, "]}\n") >=
        static_cast<int>(sizeof(body) - used))
    {
      sendApiError(500, "internal_error", nullptr,
                   "layout registry response is too large");
      return;
    }
    commonHeaders();
    server_.send(200, "application/json; charset=utf-8", body);
  }

  void updateConfig()
  {
    if (!editingEnabled())
    {
      sendApiError(405, "read_only", nullptr,
                   "configuration editing is not enabled");
      return;
    }

    const String &requestBody = server_.arg("plain");
    if (requestBody.length() == 0 || requestBody.length() > 512)
    {
      sendApiError(requestBody.length() > 512 ? 413 : 400, "invalid_request",
                   "json", "request body must contain at most 512 bytes");
      return;
    }

    KeyboardLayoutSettings candidate;
    LayoutSettingsError error;
    if (!decodeKeyboardLayoutSettingsJson(requestBody.c_str(), requestBody.length(),
                                          candidate, &error))
    {
      sendApiError(400, "invalid_settings", error.field, error.message);
      return;
    }
    if (macroSettings_ != nullptr &&
        macroSettings_->usesTrigger(candidate.layoutConversionToggleUsage()))
    {
      sendApiError(409, "trigger_conflict", "layoutConversionToggleUsage",
                   "layout conversion toggle conflicts with a macro trigger");
      return;
    }

    const KeyboardLayoutSettings previous = *editableSettings_;
    if (!store_->save(candidate, &error))
    {
      sendApiError(500, "storage_failed", error.field, error.message);
      return;
    }

    *editableSettings_ = candidate;
    if (!editableSettings_->apply(*editableBridge_, *config_, *inputConfig_, &error))
    {
      *editableSettings_ = previous;
      LayoutSettingsError rollbackError;
      store_->save(previous, &rollbackError);
      previous.apply(*editableBridge_, *config_, *inputConfig_, &rollbackError);
      sendApiError(500, "apply_failed", error.field, error.message);
      return;
    }

    sendConfig();
  }

  void sendApiError(int status, const char *code, const char *field,
                    const char *message)
  {
    char body[320];
    snprintf(body, sizeof(body),
             "{\"error\":\"%s\",\"field\":\"%s\",\"message\":\"%s\"}\n",
             code != nullptr ? code : "error", field != nullptr ? field : "",
             message != nullptr ? message : "request failed");
    commonHeaders();
    server_.send(status, "application/json; charset=utf-8", body);
  }

  void sendStatus()
  {
    char body[192];
    snprintf(body, sizeof(body),
             "{\"layoutConversionEnabled\":%s,\"wifiClients\":%u,"
             "\"mergedOverflow\":%s,\"outputOverflow\":%s}\n",
             bridge_->layoutConversionEnabled() ? "true" : "false",
             static_cast<unsigned>(connectedClients()),
             bridge_->mergedOverflow() ? "true" : "false",
             bridge_->outputOverflow() ? "true" : "false");
    commonHeaders();
    server_.send(200, "application/json; charset=utf-8", body);
  }

  void sendMacros()
  {
    if (macroSettings_ == nullptr)
    {
      sendApiError(404, "not_available", nullptr,
                   "macro settings are not configured");
      return;
    }
    char body[1536] = {};
    MacroSettingsError error;
    if (encodeKeyboardMacroSettingsJson(*macroSettings_, body, sizeof(body),
                                        &error) == 0)
    {
      sendApiError(500, "internal_error", error.field, error.message);
      return;
    }
    commonHeaders();
    server_.send(200, "application/json; charset=utf-8", body);
  }

  void updateMacros()
  {
    if (macroSettings_ == nullptr || macroStore_ == nullptr ||
        macroConfig_ == nullptr || editableBridge_ == nullptr)
    {
      sendApiError(405, "read_only", nullptr,
                   "macro editing is not enabled");
      return;
    }
    const String &requestBody = server_.arg("plain");
    if (requestBody.length() == 0 || requestBody.length() > 1536)
    {
      sendApiError(requestBody.length() > 1536 ? 413 : 400, "invalid_request",
                   "json", "request body must contain at most 1536 bytes");
      return;
    }

    KeyboardMacroSettings candidate;
    MacroSettingsError error;
    if (!decodeKeyboardMacroSettingsJson(requestBody.c_str(),
                                         requestBody.length(), candidate, &error))
    {
      sendApiError(400, "invalid_macro_settings", error.field, error.message);
      return;
    }
    if (settings_ != nullptr &&
        candidate.usesTrigger(settings_->layoutConversionToggleUsage()))
    {
      sendApiError(409, "trigger_conflict", "triggerUsage",
                   "macro trigger conflicts with the layout conversion toggle");
      return;
    }
    const KeyboardMacroSettings previous = *macroSettings_;
    if (!macroStore_->save(candidate, &error))
    {
      sendApiError(500, "storage_failed", error.field, error.message);
      return;
    }
    *macroSettings_ = candidate;
    if (!macroSettings_->apply(*editableBridge_, *macroConfig_, &error))
    {
      *macroSettings_ = previous;
      MacroSettingsError rollbackError;
      macroStore_->save(previous, &rollbackError);
      previous.apply(*editableBridge_, *macroConfig_, &rollbackError);
      sendApiError(500, "apply_failed", error.field, error.message);
      return;
    }
    sendMacros();
  }

  void sendNavigation()
  {
    if (navigation_ == nullptr)
    {
      commonHeaders();
      server_.send(200, "application/json; charset=utf-8",
                   "{\"available\":false,\"capture\":false,"
                   "\"sequence\":0,\"action\":\"none\",\"overflow\":0}\n");
      return;
    }

    navigation_->setNavigationCaptureEnabled(true);
    navigationLeaseActive_ = true;
    lastNavigationPollMs_ = millis();
    const bool captureActive = navigation_->navigationCaptureEnabled();
    SettingsNavigationEvent event;
    const bool haveEvent = navigation_->popNavigationEvent(event);
    char body[192];
    snprintf(body, sizeof(body),
             "{\"available\":true,\"capture\":%s,\"sequence\":%lu,"
             "\"action\":\"%s\",\"overflow\":%lu}\n",
             captureActive ? "true" : "false",
             static_cast<unsigned long>(haveEvent ? event.sequence : 0),
             settingsNavigationActionName(haveEvent ? event.action
                                                     : SettingsNavigationAction::None),
             static_cast<unsigned long>(navigation_->navigationOverflowCount()));
    commonHeaders();
    server_.send(200, "application/json; charset=utf-8", body);
  }

  void releaseNavigationCapture()
  {
    if (navigation_ != nullptr)
    {
      navigation_->setNavigationCaptureEnabled(false);
    }
    navigationLeaseActive_ = false;
  }

  const KeyboardLayoutSettings *settings_ = nullptr;
  const ESP32KeyBridge *bridge_ = nullptr;
  KeyboardLayoutSettings *editableSettings_ = nullptr;
  ESP32KeyBridge *editableBridge_ = nullptr;
  KeyboardLayoutSettingsStore *store_ = nullptr;
  ESP32KeyBridgeConfig *config_ = nullptr;
  InputConfig *inputConfig_ = nullptr;
  KeyboardMacroSettings *macroSettings_ = nullptr;
  KeyboardMacroSettingsStore *macroStore_ = nullptr;
  ESP32KeyBridgeConfig *macroConfig_ = nullptr;
  SettingsNavigationSource *navigation_ = nullptr;
  DNSServer dnsServer_;
  WebServer server_;
  static constexpr uint32_t NavigationLeaseMilliseconds = 1200;
  uint32_t lastNavigationPollMs_ = 0;
  bool handlersRegistered_ = false;
  bool started_ = false;
  bool captivePortalEnabled_ = false;
  bool navigationLeaseActive_ = false;
};

} // namespace esp32keybridge
