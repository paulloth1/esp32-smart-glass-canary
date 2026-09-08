#pragma once
/*
 * Smart Glass Canary -- read-only device page.
 *
 * Served over the normal Wi-Fi connection once the device is on the network.
 * There is no access point and no DNS responder here: this is a plain HTTP
 * server on the station interface, which is what makes "Visit device" in the
 * browser installer resolve to something real.
 *
 * Deliberately read-only. Nothing here changes configuration, so anyone who
 * reaches the device on the LAN can see what it is doing but cannot reconfigure
 * or unconfigure it.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#define WEBUI_LOG_SIZE 20

struct WebLogEntry {
  char    when[26];      // ISO-8601 local, or "unsynced"
  char    vendor[28];
  int8_t  rssi;
  uint8_t score;
};

static WebLogEntry g_weblog[WEBUI_LOG_SIZE];
static uint8_t     g_weblogCount = 0;   // how many slots are filled
static uint8_t     g_weblogNext  = 0;   // next slot to write (ring)
static WebServer   g_web(80);
static bool        g_webUp = false;

// Newest-first iteration over the ring.
static const WebLogEntry* weblogAt(uint8_t i) {
  if (i >= g_weblogCount) return nullptr;
  int idx = (int)g_weblogNext - 1 - (int)i;
  while (idx < 0) idx += WEBUI_LOG_SIZE;
  return &g_weblog[idx];
}

static void weblogAdd(const char* when, const char* vendor, int8_t rssi, uint8_t score) {
  WebLogEntry& e = g_weblog[g_weblogNext];
  strncpy(e.when,   when   ? when   : "",        sizeof(e.when) - 1);   e.when[sizeof(e.when)-1]   = 0;
  strncpy(e.vendor, vendor ? vendor : "unknown", sizeof(e.vendor) - 1); e.vendor[sizeof(e.vendor)-1] = 0;
  e.rssi  = rssi;
  e.score = score;
  g_weblogNext = (g_weblogNext + 1) % WEBUI_LOG_SIZE;
  if (g_weblogCount < WEBUI_LOG_SIZE) g_weblogCount++;
}

static void webAppendEscaped(String& out, const char* s) {
  for (const char* p = s; *p; ++p) {
    switch (*p) {
      case '<': out += "&lt;";  break;
      case '>': out += "&gt;";  break;
      case '&': out += "&amp;"; break;
      case '"': out += "&quot;"; break;
      default:  out += *p;
    }
  }
}

static void webHandleRoot() {
  char ts[40];
  isoNow(ts, sizeof(ts));
  uint32_t up = millis() / 1000;

  String h;
  h.reserve(4096);
  h += F("<!doctype html><html lang=en><meta charset=utf-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>Smart Glass Canary</title>"
         "<meta http-equiv=refresh content=10>"
         "<style>"
         ":root{--bg:#fbfbfa;--fg:#1c1c1a;--mut:#6b6b66;--ln:#e3e3df;--ok:#1a7f37;--hit:#b3261e}"
         "@media(prefers-color-scheme:dark){:root{--bg:#16161a;--fg:#e8e8e4;--mut:#9a9a94;--ln:#2c2c31;--ok:#4ac26b;--hit:#ff7b72}}"
         "body{margin:0;padding:2rem 1.25rem;background:var(--bg);color:var(--fg);"
         "font:15px/1.55 system-ui,-apple-system,sans-serif;display:flex;justify-content:center}"
         "main{width:100%;max-width:34rem}h1{font-size:1.3rem;margin:0 0 .2rem}"
         ".sub{color:var(--mut);margin:0 0 1.5rem;font-size:.9rem}"
         "table{width:100%;border-collapse:collapse;margin:0 0 1.75rem;font-size:.9rem}"
         "th,td{text-align:left;padding:.4rem 0;border-bottom:1px solid var(--ln);vertical-align:top}"
         "th{color:var(--mut);font-weight:400;width:9.5rem}"
         "h2{font-size:1rem;margin:0 0 .6rem}"
         ".ok{color:var(--ok)}.hit{color:var(--hit)}"
         ".empty{color:var(--mut);font-size:.9rem}"
         "code{font-size:.85em}"
         "footer{color:var(--mut);font-size:.8rem;border-top:1px solid var(--ln);padding-top:1rem;margin-top:1.5rem}"
         "a{color:inherit}"
         "</style><main><h1>Smart Glass Canary</h1>"
         "<p class=sub>Read-only status. This page cannot change any settings.</p><table>");

  h += F("<tr><th>State</th><td>");
  if (presentCount() > 0) h += F("<b class=hit>glasses nearby</b>");
  else                    h += F("<span class=ok>nothing detected</span>");
  h += F("</td></tr>");

  h += F("<tr><th>Firmware</th><td>"); h += FW_VERSION; h += F("</td></tr>");
  h += F("<tr><th>Device time</th><td>"); h += ts; h += F("</td></tr>");
  h += F("<tr><th>Uptime</th><td>");
  h += String(up / 3600); h += F("h "); h += String((up % 3600) / 60); h += F("m</td></tr>");
  h += F("<tr><th>Wi-Fi</th><td>"); webAppendEscaped(h, WiFi.SSID().c_str());
  h += F(" &middot; "); h += WiFi.localIP().toString();
  h += F(" &middot; "); h += String(WiFi.RSSI()); h += F(" dBm</td></tr>");

#if MQTT_ENABLED
  h += F("<tr><th>MQTT</th><td>");
  if (g_cfg.mqttHost[0] == '\0')      h += F("<span class=empty>not configured</span>");
  else if (g_mqtt.connected())        { h += F("connected to "); webAppendEscaped(h, g_cfg.mqttHost); }
  else                                { h += F("disconnected from "); webAppendEscaped(h, g_cfg.mqttHost); }
  h += F("</td></tr>");
#endif

  h += F("<tr><th>Alerts</th><td>"); h += String(g_alertCount); h += F("</td></tr>");
  h += F("<tr><th>Advertisements</th><td>"); h += String(g_advSeen); h += F(" seen</td></tr>");
  h += F("<tr><th>Scan restarts</th><td>"); h += String(g_scanRestarts);
  if (g_scanRestarts) h += F(" <span class=empty>(recovered from a stalled scan)</span>");
  h += F("</td></tr>");
  h += F("<tr><th>Free memory</th><td>"); h += String(ESP.getFreeHeap() / 1024); h += F(" KB</td></tr>");
  h += F("</table><h2>Recent detections</h2>");

  if (g_weblogCount == 0) {
    h += F("<p class=empty>Nothing detected since the last restart. "
           "This list is held in memory and clears on reboot.</p>");
  } else {
    h += F("<table><tr><th>When</th><th>What</th></tr>");
    for (uint8_t i = 0; i < g_weblogCount; ++i) {
      const WebLogEntry* e = weblogAt(i);
      if (!e) break;
      h += F("<tr><td>"); webAppendEscaped(h, e->when);
      h += F("</td><td>"); webAppendEscaped(h, e->vendor);
      h += F(" &middot; "); h += String(e->rssi); h += F(" dBm &middot; score ");
      h += String(e->score); h += F("</td></tr>");
    }
    h += F("</table>");
  }

  h += F("<footer>A detection means glasses were nearby. Silence does not mean "
         "nobody is recording &mdash; glasses go quiet once paired to a phone. "
         "<a href=\"https://github.com/paulloth1/esp32-smart-glass-canary\">Details</a>."
         "</footer></main></html>");

  g_web.send(200, "text/html; charset=utf-8", h);
}

// Machine-readable equivalent, for anything that would rather not scrape HTML.
static void webHandleStatus() {
  char ts[40];
  isoNow(ts, sizeof(ts));
  String j = "{";
  j += "\"fw\":\"" FW_VERSION "\",";
  j += "\"time\":\"" + String(ts) + "\",";
  j += "\"uptime_s\":" + String(millis() / 1000) + ",";
  j += "\"present\":" + String(presentCount()) + ",";
  j += "\"alerts\":" + String(g_alertCount) + ",";
  j += "\"adv_seen\":" + String(g_advSeen) + ",";
  j += "\"scan_restarts\":" + String(g_scanRestarts) + ",";
  j += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  g_web.send(200, "application/json", j);
}

static void webStart() {
  if (g_webUp) return;
  g_web.on("/", webHandleRoot);
  g_web.on("/api/status", webHandleStatus);
  g_web.onNotFound([]() { g_web.send(404, "text/plain", "Not found"); });
  g_web.begin();
  g_webUp = true;
  Serial.printf("{\"event\":\"webui\",\"state\":\"listening\",\"url\":\"http://%s/\"}\n",
                WiFi.localIP().toString().c_str());
}

static void webStop() {
  if (!g_webUp) return;
  g_web.stop();
  g_webUp = false;
}

static void webService() {
  if (g_webUp) g_web.handleClient();
}
