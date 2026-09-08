#pragma once
/*
 * Smart Glass Canary -- runtime provisioning.
 *
 * Wi-Fi and MQTT settings live in NVS rather than being compiled in. That is
 * what makes a distributable firmware image possible: the binary carries no
 * credentials, so it can be handed to anyone, and each device is configured
 * once through a captive portal on its own access point.
 *
 * Values in secrets.h still work -- they act as the initial defaults when NVS
 * is empty, so a developer flashing their own build keeps the old workflow.
 * Anything saved through the portal takes precedence from then on.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

#include "config.h"
#include "portal_page.h"

struct CanaryConfig {
  char     wifiSsid[33];   // 32 chars max per 802.11
  char     wifiPass[65];   // 63 chars max for WPA2-PSK
  char     mqttHost[64];   // empty disables MQTT
  uint16_t mqttPort;
  char     mqttUser[33];
  char     mqttPass[65];
};

static CanaryConfig g_cfg;
static Preferences  g_prefs;

static WebServer   g_portalServer(80);
static DNSServer   g_dns;
static bool        g_portalUp = false;
static String      g_networksJson = "[]";

// A compiled-in placeholder is not a configuration.
static bool cfgIsPlaceholder(const char* s) {
  return s[0] == '\0' || strcmp(s, "YOUR_SSID") == 0 || strcmp(s, "YOUR_PASSWORD") == 0;
}

static bool cfgConfigured() { return !cfgIsPlaceholder(g_cfg.wifiSsid); }

static void cfgCopy(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

static void cfgLoad() {
  memset(&g_cfg, 0, sizeof(g_cfg));
  g_prefs.begin("canary", true);           // read-only

  // NVS first, falling back to whatever secrets.h supplied at build time.
  String ssid = g_prefs.getString("ssid", WIFI_SSID);
  String pass = g_prefs.getString("pass", WIFI_PASS);
  String host = g_prefs.getString("mhost", MQTT_HOST);
  String user = g_prefs.getString("muser", MQTT_USER);
  String mpw  = g_prefs.getString("mpass", MQTT_PASS);
  g_cfg.mqttPort = g_prefs.getUShort("mport", MQTT_PORT);
  g_prefs.end();

  cfgCopy(g_cfg.wifiSsid, sizeof(g_cfg.wifiSsid), ssid.c_str());
  cfgCopy(g_cfg.wifiPass, sizeof(g_cfg.wifiPass), pass.c_str());
  cfgCopy(g_cfg.mqttHost, sizeof(g_cfg.mqttHost), host.c_str());
  cfgCopy(g_cfg.mqttUser, sizeof(g_cfg.mqttUser), user.c_str());
  cfgCopy(g_cfg.mqttPass, sizeof(g_cfg.mqttPass), mpw.c_str());

  if (cfgIsPlaceholder(g_cfg.wifiSsid)) g_cfg.wifiSsid[0] = '\0';
  if (cfgIsPlaceholder(g_cfg.wifiPass)) g_cfg.wifiPass[0] = '\0';

  Serial.printf("{\"event\":\"config\",\"configured\":%d,\"ssid\":\"%s\","
                "\"mqtt_host\":\"%s\",\"mqtt_port\":%u}\n",
                cfgConfigured() ? 1 : 0, g_cfg.wifiSsid, g_cfg.mqttHost, g_cfg.mqttPort);
}

static void cfgClear() {
  g_prefs.begin("canary", false);
  g_prefs.clear();
  g_prefs.end();
  Serial.println("{\"event\":\"config\",\"action\":\"cleared\"}");
}

// ------------------------------------------------------------ scanning

static void jsonEscapeInto(String& out, const char* s) {
  for (const char* p = s; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\')      { out += '\\'; out += (char)c; }
    else if (c < 0x20 || c > 0x7E)  { out += '_'; }   // keep the JSON clean
    else                             { out += (char)c; }
  }
}

// Scan is blocking (~2-4s) but only ever runs while the setup portal is open,
// where a brief pause costs nothing. BLE keeps running on its own task.
static void portalScan() {
  int n = WiFi.scanNetworks(false, false);
  String j = "[";
  for (int i = 0; i < n && i < 20; ++i) {
    if (j.length() > 1) j += ',';
    j += "{\"s\":\"";
    jsonEscapeInto(j, WiFi.SSID(i).c_str());
    j += "\",\"r\":" + String(WiFi.RSSI(i));
    j += ",\"e\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? 0 : 1);
    j += "}";
  }
  j += "]";
  g_networksJson = j;
  WiFi.scanDelete();
  Serial.printf("{\"event\":\"portal_scan\",\"found\":%d}\n", n);
}

// ------------------------------------------------------------- handlers

static void portalHandleRoot() {
  String page = FPSTR(PORTAL_HTML);
  page.replace("%%NETWORKS%%", g_networksJson);
  g_portalServer.send(200, "text/html; charset=utf-8", page);
}

static void portalHandleRescan() {
  portalScan();
  g_portalServer.sendHeader("Location", "/", true);
  g_portalServer.send(302, "text/plain", "");
}

static void portalHandleSave() {
  String ssid = g_portalServer.arg("ssid");
  if (ssid.length() == 0) {
    g_portalServer.send(400, "text/plain", "A network name is required.");
    return;
  }

  g_prefs.begin("canary", false);
  g_prefs.putString("ssid",  ssid);
  g_prefs.putString("pass",  g_portalServer.arg("pass"));
  g_prefs.putString("mhost", g_portalServer.arg("mqtt_host"));
  g_prefs.putString("muser", g_portalServer.arg("mqtt_user"));
  g_prefs.putString("mpass", g_portalServer.arg("mqtt_pass"));
  uint16_t port = (uint16_t)g_portalServer.arg("mqtt_port").toInt();
  g_prefs.putUShort("mport", port ? port : 1883);
  g_prefs.end();

  Serial.printf("{\"event\":\"provisioned\",\"ssid\":\"%s\"}\n", ssid.c_str());

  g_portalServer.send(200, "text/html; charset=utf-8",
    "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>body{font:16px system-ui;margin:0;padding:2rem;max-width:30rem}"
    "h1{font-size:1.2rem}</style>"
    "<h1>Saved</h1><p>The canary is restarting and will try to join "
    "<b>" + ssid + "</b>.</p><p>You can close this page and rejoin your normal "
    "Wi-Fi. If the network cannot be reached, the setup access point comes "
    "back so you can try again.</p>");

  delay(1200);          // let the response actually reach the browser
  ESP.restart();
}

// Captive-portal detection probes from iOS, Android and Windows. Answering
// them with a redirect is what makes the setup page pop up on its own.
static void portalHandleNotFound() {
  g_portalServer.sendHeader("Location", "http://4.3.2.1/", true);
  g_portalServer.send(302, "text/plain", "");
}

// ------------------------------------------------------------ lifecycle

static void portalStart() {
  if (g_portalUp) return;

  WiFi.mode(WIFI_AP_STA);            // AP for setup, STA so we can scan
  IPAddress apIp(4, 3, 2, 1);        // memorable, and not a common LAN range
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  if (strlen(AP_PASSWORD) >= 8) WiFi.softAP(AP_SSID, AP_PASSWORD);
  else                          WiFi.softAP(AP_SSID);

  portalScan();

  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  g_dns.start(53, "*", apIp);        // every lookup resolves to us

  g_portalServer.on("/", portalHandleRoot);
  g_portalServer.on("/save", HTTP_POST, portalHandleSave);
  g_portalServer.on("/rescan", portalHandleRescan);
  g_portalServer.onNotFound(portalHandleNotFound);
  g_portalServer.begin();

  g_portalUp = true;
  Serial.printf("{\"event\":\"portal_open\",\"ap\":\"%s\",\"open\":%d,\"url\":\"http://4.3.2.1/\"}\n",
                AP_SSID, strlen(AP_PASSWORD) >= 8 ? 0 : 1);
}

static void portalService() {
  if (!g_portalUp) return;
  g_dns.processNextRequest();
  g_portalServer.handleClient();
}

static void portalStop() {
  if (!g_portalUp) return;
  g_portalServer.stop();
  g_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  g_portalUp = false;
  Serial.println("{\"event\":\"portal_closed\"}");
}

static bool portalActive() { return g_portalUp; }

// Hold BOOT at power-on to wipe credentials. This is the way back when the
// Wi-Fi password changes or the device moves to a different house.
static void checkFactoryReset() {
  pinMode(PIN_FACTORY_RESET, INPUT_PULLUP);
  if (digitalRead(PIN_FACTORY_RESET) != LOW) return;

  Serial.println("{\"event\":\"factory_reset\",\"state\":\"hold_to_confirm\"}");
  uint32_t start = millis();
  while (digitalRead(PIN_FACTORY_RESET) == LOW) {
    if (millis() - start >= FACTORY_RESET_HOLD_MS) {
      cfgClear();
      Serial.println("{\"event\":\"factory_reset\",\"state\":\"done\"}");
      delay(300);
      ESP.restart();
    }
    delay(50);
  }
  Serial.println("{\"event\":\"factory_reset\",\"state\":\"aborted\"}");
}
