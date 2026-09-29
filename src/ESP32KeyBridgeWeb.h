#pragma once

#include <Arduino.h>

#if !defined(ARDUINO_ARCH_ESP32) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "ESP32KeyBridgeWeb supports ESP32-S3 only"
#endif

#include <WebServer.h>
#include <WiFi.h>

#include "ESP32KeyBridgeSettings.h"

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
footer{color:var(--muted);font-size:.86rem;margin-top:22px}code{color:var(--accent)}
</style>
</head>
<body>
<main>
<header><div class="eyebrow">Local device settings</div><h1>ESP32 KeyBridge</h1><p class="lede">This phase is read-only. It confirms that your phone can reach the S3 and shows the active persisted keyboard-layout settings.</p></header>
<section class="grid" aria-live="polite">
<article class="card"><div class="label">Keyboard layout</div><div class="value" id="input-layout">Loading…</div></article>
<article class="card"><div class="label">Host layout</div><div class="value" id="host-layout">Loading…</div></article>
<article class="card"><div class="label">Layout conversion</div><div class="value status"><span class="dot" id="conversion-dot"></span><span id="conversion">Loading…</span></div></article>
<article class="card"><div class="label">Toggle HID usage</div><div class="value" id="toggle">Loading…</div></article>
<article class="card wide"><div class="label">Device status</div><div class="value" id="device-status">Connecting…</div></article>
</section>
<footer>Served directly by the ESP32-S3. API: <code>/api/v1/config</code> and <code>/api/v1/status</code>.</footer>
</main>
<script>
const $=id=>document.getElementById(id);
async function json(path){const r=await fetch(path,{cache:'no-store'});if(!r.ok)throw new Error(r.status);return r.json()}
async function refreshConfig(){try{const c=await json('/api/v1/config');$('input-layout').textContent=c.inputLayout;$('host-layout').textContent=c.hostLayout;$('toggle').textContent='0x'+c.layoutConversionToggleUsage.toString(16).padStart(2,'0').toUpperCase()}catch(e){$('device-status').textContent='Configuration unavailable'}}
async function refreshStatus(){try{const s=await json('/api/v1/status');$('conversion').textContent=s.layoutConversionEnabled?'Enabled':'Disabled';$('conversion-dot').classList.toggle('on',s.layoutConversionEnabled);$('device-status').textContent=`Online · ${s.wifiClients} phone${s.wifiClients===1?'':'s'} connected${s.mergedOverflow||s.outputOverflow?' · key overflow':''}`}catch(e){$('device-status').textContent='Device disconnected'}}
refreshConfig();refreshStatus();setInterval(refreshStatus,1500);
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
};

// ESP32-S3 SoftAP plus a small read-only settings website. The sketch owns
// scheduling and must call update() frequently from loop(). Phase 2 has no
// mutation endpoints, captive portal, or keyboard-navigation channel.
class Esp32S3SettingsWebServer
{
public:
  Esp32S3SettingsWebServer(const KeyboardLayoutSettings &settings,
                           const ESP32KeyBridge &bridge, uint16_t port = 80)
      : settings_(settings), bridge_(bridge), server_(port)
  {
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

    server_.begin();
    started_ = true;
    return true;
  }

  void update()
  {
    if (started_)
    {
      server_.handleClient();
    }
  }

  void end()
  {
    if (!started_)
    {
      return;
    }
    server_.stop();
    WiFi.softAPdisconnect(true);
    started_ = false;
  }

  bool started() const { return started_; }
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
    server_.on("/api/v1/status", HTTP_GET, [this]() { sendStatus(); });
    server_.on("/healthz", HTTP_GET, [this]() {
      commonHeaders();
      server_.send(200, "text/plain; charset=utf-8", "ok\n");
    });
    server_.onNotFound([this]() {
      commonHeaders();
      server_.send(404, "application/json; charset=utf-8",
                   "{\"error\":\"not_found\"}\n");
    });
  }

  void sendConfig()
  {
    char body[256];
    snprintf(body, sizeof(body),
             "{\"schemaVersion\":%lu,\"inputLayout\":\"%s\","
             "\"hostLayout\":\"%s\",\"layoutConversionEnabled\":%s,"
             "\"layoutConversionToggleUsage\":%u}\n",
             static_cast<unsigned long>(KeyboardLayoutSettings::CurrentVersion),
             settings_.inputLayoutName(), settings_.hostLayoutName(),
             settings_.layoutConversionEnabled() ? "true" : "false",
             static_cast<unsigned>(settings_.layoutConversionToggleUsage()));
    commonHeaders();
    server_.send(200, "application/json; charset=utf-8", body);
  }

  void sendStatus()
  {
    char body[192];
    snprintf(body, sizeof(body),
             "{\"layoutConversionEnabled\":%s,\"wifiClients\":%u,"
             "\"mergedOverflow\":%s,\"outputOverflow\":%s}\n",
             bridge_.layoutConversionEnabled() ? "true" : "false",
             static_cast<unsigned>(connectedClients()),
             bridge_.mergedOverflow() ? "true" : "false",
             bridge_.outputOverflow() ? "true" : "false");
    commonHeaders();
    server_.send(200, "application/json; charset=utf-8", body);
  }

  const KeyboardLayoutSettings &settings_;
  const ESP32KeyBridge &bridge_;
  WebServer server_;
  bool handlersRegistered_ = false;
  bool started_ = false;
};

} // namespace esp32keybridge
