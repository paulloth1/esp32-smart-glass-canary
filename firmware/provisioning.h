#pragma once
/*
 * Smart Glass Canary -- runtime configuration.
 *
 * Wi-Fi and MQTT settings live in NVS rather than being compiled in. That is
 * what makes a distributable firmware image possible: the binary carries no
 * credentials, so it can be handed to anyone and configured per device.
 *
 * Settings arrive over Improv Wi-Fi on the USB serial connection -- the same
 * one the browser used to flash the device. There is deliberately no setup
 * access point: a SoftAP would mean broadcasting an open network, contending
 * for the single 2.4GHz radio the BLE scanner needs, and carrying a web server
 * and DNS responder for a job the serial link already does.
 *
 * Values in secrets.h still work as the defaults when NVS is empty, so a
 * developer flashing their own build keeps the old workflow.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include "config.h"

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
  // Store an explicit empty SSID rather than leaving the key absent. With no
  // key, cfgLoad() falls back to the compile-time secrets.h default and the
  // device comes straight back up "configured" -- which silently defeats both
  // the serial reprovision and the BOOT-button factory reset on any build that
  // has real credentials compiled in.
  g_prefs.putString("ssid", "");
  g_prefs.end();
  Serial.println("{\"event\":\"config\",\"action\":\"cleared\"}");
}

// Credentials handed to us over Improv. The library connects but does not
// remember, so persist them or a reboot loses them.
static void cfgSaveWifi(const char* ssid, const char* pass) {
  g_prefs.begin("canary", false);
  g_prefs.putString("ssid", ssid ? ssid : "");
  g_prefs.putString("pass", pass ? pass : "");
  g_prefs.end();
  cfgCopy(g_cfg.wifiSsid, sizeof(g_cfg.wifiSsid), ssid);
  cfgCopy(g_cfg.wifiPass, sizeof(g_cfg.wifiPass), pass);
  Serial.printf("{\"event\":\"config\",\"action\":\"saved\",\"source\":\"improv\","
                "\"ssid\":\"%s\"}\n", g_cfg.wifiSsid);
}

// Hold BOOT at power-on to wipe credentials, so the device can be handed on or
// re-provisioned without a serial console.
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
