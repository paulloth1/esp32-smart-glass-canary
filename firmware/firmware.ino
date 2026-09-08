/*
 * Smart Glass Canary
 * ------------------
 * An ESP32 that watches the BLE spectrum for camera-equipped smart glasses
 * (Meta Ray-Ban, Oakley Meta, Snap Spectacles and friends) and raises an
 * alert — onboard LED, buzzer, serial, and optionally a Wi-Fi webhook or
 * MQTT publish — when one comes into range.
 *
 * Tune behaviour in config.h. Tune what counts as glasses in signatures.h.
 *
 * How it works
 *   A continuous active BLE scan feeds every advertisement into a scoring
 *   function. Signals (advertised name, manufacturer company ID, public-MAC
 *   OUI, service UUID) each add points; crossing ALERT_SCORE_THRESHOLD with
 *   adequate RSSI over CONFIRM_HITS sightings raises the alert. Devices go
 *   quiet after DEVICE_TIMEOUT_MS, which fires an all-clear.
 *
 * Threading
 *   NimBLE delivers advertisements on its own host task. That callback does
 *   nothing but score the packet and push a small record onto a queue —
 *   no Wi-Fi, no HTTP, no blocking. loop() drains the queue and owns all
 *   state and I/O. Keep it that way; doing network work in the callback
 *   will stall the BLE host and drop advertisements.
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string>
#include <time.h>

#include "config.h"
#include "signatures.h"

#if WIFI_ENABLED
  #include <WiFi.h>
  #include <ImprovWiFiLibrary.h>
  #include "provisioning.h"
  #if WEBHOOK_ENABLED
    #include <HTTPClient.h>
  #endif
  #if MQTT_ENABLED
    #include <PubSubClient.h>
    static WiFiClient   g_mqttNet;
    static PubSubClient g_mqtt(g_mqttNet);
  #endif
#endif

#define FW_VERSION   "1.1.0"
#define MAX_TRACKED  32
#define ALERT_HOLD_MS 8000UL

// ---------------------------------------------------------------- types

// One scored sighting, handed from the BLE task to loop().
struct Obs {
  char        addr[18];
  char        name[24];
  const char* vendor;
  char        why[56];
  uint8_t     score;
  int8_t      rssi;
};

// Per-device state, owned exclusively by loop().
struct Track {
  bool        used;
  bool        present;
  char        addr[18];
  char        name[24];
  const char* vendor;
  uint8_t     score;
  int8_t      bestRssi;
  int8_t      lastRssi;
  uint8_t     hits;
  uint32_t    firstSeen;
  uint32_t    lastSeen;
  uint32_t    lastAlert;
};

static Track         g_track[MAX_TRACKED];
static QueueHandle_t g_queue = nullptr;
static uint32_t      g_advSeen = 0;      // every advertisement, matched or not
static volatile uint32_t g_lastAdvMs = 0; // written from the BLE task
static volatile bool g_scanEnded = false; // set by onScanEnd, handled in loop()
static uint32_t      g_scanRestarts = 0;
static uint32_t      g_scanRetryAt = 0;   // backoff deadline after a failed restart
static bool          g_timeSynced = false;
static uint32_t      g_alertCount = 0;
static bool          g_verbose = LOG_ALL_DEVICES;

// indicator state
static uint32_t g_alertUntil  = 0;
static uint8_t  g_beepsLeft   = 0;
static uint32_t g_nextBeepAt  = 0;
static uint32_t g_lastBlink   = 0;
static bool     g_ledOn       = false;

// ------------------------------------------------------------- matching

// Case-insensitive substring test. needle must already be lowercase.
static bool lowerContains(const char* hay, const char* needle) {
  if (!hay || !needle || !*needle) return false;
  size_t nl = strlen(needle);
  for (const char* p = hay; *p; ++p) {
    size_t i = 0;
    while (i < nl && p[i] && tolower((unsigned char)p[i]) == needle[i]) ++i;
    if (i == nl) return true;
  }
  return false;
}

// Advertised names are arbitrary bytes chosen by someone else's device. Strip
// anything that would corrupt the JSON we emit, or the serial stream itself.
static void sanitize(char* s) {
  for (char* p = s; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x20 || c > 0x7E || c == '"' || c == '\\') *p = '_';
  }
}

static void appendWhy(char* why, size_t cap, const char* item) {
  size_t len = strlen(why);
  if (len && len + 1 < cap) { why[len++] = '+'; why[len] = 0; }
  strncat(why, item, cap - strlen(why) - 1);
}

/*
 * Score one advertisement. Returns the total; fills vendor and a short
 * human-readable explanation of which signals fired.
 */
static uint8_t scoreDevice(const NimBLEAdvertisedDevice* dev,
                           const char* name,
                           const char* addrStr,
                           const char** vendorOut,
                           char* why, size_t whyCap) {
  uint16_t total = 0;
  *vendorOut = "unknown";
  why[0] = 0;

  // --- advertised name
  if (name && name[0]) {
    for (size_t i = 0; i < N_NAME_RULES; ++i) {
      const NameRule& r = NAME_RULES[i];
      if (!tierEnabled(r.tier)) continue;
      if (lowerContains(name, r.needle)) {
        total += r.score;
        *vendorOut = r.vendor;
        appendWhy(why, whyCap, "name");
        break;                       // best (first) name match only
      }
    }
  }

  // --- manufacturer company ID (first 2 bytes, little-endian)
  if (dev->haveManufacturerData()) {
    std::string md = dev->getManufacturerData();
    if (md.size() >= 2) {
      uint16_t cid = (uint8_t)md[0] | ((uint16_t)(uint8_t)md[1] << 8);
      for (size_t i = 0; i < N_COMPANY_RULES; ++i) {
        const CompanyRule& r = COMPANY_RULES[i];
        if (!tierEnabled(r.tier)) continue;
        if (r.id == cid) {
          total += r.score;
          if (strcmp(*vendorOut, "unknown") == 0) *vendorOut = r.vendor;
          appendWhy(why, whyCap, "mfr");
          break;
        }
      }
    }
  }

  // --- MAC OUI, but only for public addresses. A random/resolvable private
  //     address carries no vendor information and rotates, so matching its
  //     top bytes would be noise.
#if OUI_RULES_ACTIVE
  // A random or resolvable private address carries no vendor bits and rotates
  // every few minutes, so its top bytes mean nothing. Public addresses only.
  if (dev->getAddress().getType() == BLE_ADDR_PUBLIC && strlen(addrStr) >= 11) {
    unsigned b0, b1, b2, b3;
    if (sscanf(addrStr, "%2x:%2x:%2x:%2x", &b0, &b1, &b2, &b3) == 4) {
      for (size_t i = 0; i < N_OUI_RULES; ++i) {
        const OuiRule& r = OUI_RULES[i];
        if (!tierEnabled(r.tier)) continue;
        if (r.b[0] != b0 || r.b[1] != b1 || r.b[2] != b2) continue;
        // 28-bit MA-M block: the vendor owns only the first 28 bits, and the
        // remaining 24-bit prefix is shared with a dozen-plus unrelated firms.
        // Without this nibble check the match is mostly false positives.
        if (r.nib >= 0 && (int)((b3 >> 4) & 0x0F) != r.nib) continue;
        total += r.score;
        if (strcmp(*vendorOut, "unknown") == 0) *vendorOut = r.vendor;
        appendWhy(why, whyCap, "oui");
        break;
      }
    }
  }
#else
  (void)addrStr;
#endif

  // --- advertised service UUIDs
#if UUID_RULES_ACTIVE
  if (dev->haveServiceUUID()) {
    bool uuidHit = false;

    for (size_t i = 0; i < N_UUID16_RULES && !uuidHit; ++i) {
      const Uuid16Rule& r = UUID16_RULES[i];
      if (!tierEnabled(r.tier)) continue;
      if (dev->isAdvertisingService(NimBLEUUID((uint16_t)r.uuid))) {
        total += r.score;
        if (strcmp(*vendorOut, "unknown") == 0) *vendorOut = r.vendor;
        uuidHit = true;
      }
    }

    for (size_t i = 0; i < N_UUID128_RULES && !uuidHit; ++i) {
      const Uuid128Rule& r = UUID128_RULES[i];
      if (!tierEnabled(r.tier)) continue;
      if (dev->isAdvertisingService(NimBLEUUID(std::string(r.uuid)))) {
        total += r.score;
        if (strcmp(*vendorOut, "unknown") == 0) *vendorOut = r.vendor;
        uuidHit = true;
      }
    }

    if (uuidHit) appendWhy(why, whyCap, "uuid");
  }
#endif

  if (!why[0]) strncpy(why, "none", whyCap - 1);
  return total > 255 ? 255 : (uint8_t)total;
}

// ------------------------------------------------------- BLE scan callback

class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    g_advSeen++;
    g_lastAdvMs = millis();   // watchdog liveness: any advert counts

    Obs o;
    memset(&o, 0, sizeof(o));
    o.rssi = (int8_t)dev->getRSSI();

    std::string a = dev->getAddress().toString();
    strncpy(o.addr, a.c_str(), sizeof(o.addr) - 1);

    if (dev->haveName()) {
      std::string n = dev->getName();
      strncpy(o.name, n.c_str(), sizeof(o.name) - 1);
      sanitize(o.name);
    }

    o.score = scoreDevice(dev, o.name, o.addr, &o.vendor, o.why, sizeof(o.why));

    // Forward matches always; everything else only in verbose mode.
    if (o.score == 0 && !g_verbose) return;

    // Never block the BLE host task — drop the observation if loop() is behind.
    xQueueSend(g_queue, &o, 0);
  }

  // NimBLE can end a scan on its own (controller error, duration lapse).
  // Restarting from inside the host task's callback is asking for trouble,
  // so just raise a flag and let loop() do it.
  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    (void)results;
    Serial.printf("{\"event\":\"scan_end\",\"reason\":%d}\n", reason);
    g_scanEnded = true;
  }
};

static ScanCB g_scanCb;

#if WIFI_ENABLED
// Improv Wi-Fi over serial: the browser that just flashed the device hands it
// credentials down the same USB connection. No access point, no captive portal,
// and none of the radio contention that comes with them -- the ESP32 shares one
// 2.4GHz radio, and a full-duty BLE scan starves Wi-Fi association.
static ImprovWiFi g_improv(&Serial);

// Set while the radio is deliberately handed to Wi-Fi for the Improv
// handshake, so the scan watchdog does not mistake the silence for a fault.
static bool g_bleePausedForSetup = false;

// Credentials arrive here. Persist them first -- the library only connects,
// it does not remember, and a reboot would otherwise lose them.
static bool improvConnect(const char* ssid, const char* password) {
  Serial.printf("{\"event\":\"improv\",\"state\":\"connecting\",\"ssid\":\"%s\"}\n", ssid);
  cfgSaveWifi(ssid, password);

  g_bleePausedForSetup = true;
  NimBLEDevice::getScan()->stop();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
  }

  bool ok = (WiFi.status() == WL_CONNECTED);
  NimBLEDevice::getScan()->start(0, false);
  g_lastAdvMs = millis();
  g_bleePausedForSetup = false;

  Serial.printf("{\"event\":\"improv\",\"state\":\"%s\",\"ip\":\"%s\"}\n",
                ok ? "connected" : "failed", WiFi.localIP().toString().c_str());
  return ok;
}

static void improvOnConnected(const char* ssid, const char* password) {
  (void)password;
  Serial.printf("{\"event\":\"improv\",\"state\":\"provisioned\",\"ssid\":\"%s\"}\n", ssid);
}

static void improvOnError(ImprovTypes::Error err) {
  Serial.printf("{\"event\":\"improv\",\"state\":\"error\",\"code\":%d}\n", (int)err);
}
#endif

// A stalled scan is this project's worst failure: everything looks healthy
// while the device is deaf. Restart on either signal - an explicit scan end,
// or a suspicious silence.
static void serviceScanWatchdog() {
#if WIFI_ENABLED
  // Silence is expected while the scan is intentionally paused for Improv.
  if (g_bleePausedForSetup) return;
#endif

  uint32_t now = millis();

  // Clear the flag as soon as we have read it, to narrow the window in which
  // an onScanEnd from the BLE task could be swallowed.
  bool ended = g_scanEnded;
  if (ended) g_scanEnded = false;

  // Capture the silence duration BEFORE re-arming, or the log reports nonsense.
  uint32_t silentFor = now - g_lastAdvMs;
  bool stalled = silentFor > SCAN_STALL_TIMEOUT_MS;
  if (!ended && !stalled) return;

  // Honour the backoff after a failed restart instead of hammering the stack.
  if (g_scanRetryAt && (int32_t)(g_scanRetryAt - now) > 0) return;

  g_scanRestarts++;
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->stop();
  bool ok = scan->start(0, false);

  if (ok) {
    g_lastAdvMs   = now;    // fresh window for the restarted scan
    g_scanRetryAt = 0;
  } else {
    // Do NOT re-arm: leaving the stall condition true means we come straight
    // back after the backoff rather than going deaf for another full timeout.
    g_scanRetryAt = now + SCAN_RETRY_BACKOFF_MS;
  }

  Serial.printf("{\"event\":\"scan_restart\",\"trigger\":\"%s\","
                "\"restarts\":%lu,\"ok\":%d,\"silent_for_ms\":%lu}\n",
                ended ? "scan_end" : "stall", (unsigned long)g_scanRestarts,
                ok ? 1 : 0, (unsigned long)silentFor);
}

// ------------------------------------------------------------- indicators

static void fireIndicators() {
  g_alertUntil = millis() + ALERT_HOLD_MS;
#if PIN_BUZZER >= 0
  g_beepsLeft  = BUZZER_BEEPS * 2;   // on/off pairs
  g_nextBeepAt = millis();
#endif
}

static void serviceIndicators() {
  uint32_t now = millis();
  bool alerting = (int32_t)(g_alertUntil - now) > 0;

  // Status LED: an occasional wink when idle, urgent flutter while alerting.
  if (!alerting && HEARTBEAT_PERIOD_MS == 0) {
    // Heartbeat disabled - hold the LED dark so any light means a detection.
    if (g_ledOn) {
      g_ledOn = false;
      digitalWrite(PIN_STATUS_LED, LOW);
    }
  } else {
    uint32_t period = alerting ? 80UL : HEARTBEAT_PERIOD_MS;
    if (now - g_lastBlink >= period) {
      g_lastBlink = now;
      g_ledOn = !g_ledOn;
      digitalWrite(PIN_STATUS_LED, g_ledOn ? HIGH : LOW);
#if PIN_ALERT_LED >= 0
      digitalWrite(PIN_ALERT_LED, (alerting && g_ledOn) ? HIGH : LOW);
#endif
    }
    // The idle heartbeat is a short wink, not a 50% duty square wave - at a
    // 30s period a half-on LED would read as "solid", not "alive".
    if (!alerting && g_ledOn && now - g_lastBlink > HEARTBEAT_WINK_MS) {
      g_ledOn = false;
      digitalWrite(PIN_STATUS_LED, LOW);
    }
  }

#if PIN_BUZZER >= 0
  if (g_beepsLeft && (int32_t)(now - g_nextBeepAt) >= 0) {
    if (g_beepsLeft % 2 == 0) tone(PIN_BUZZER, BUZZER_FREQ_HZ, 90);
    else                      noTone(PIN_BUZZER);
    g_nextBeepAt = now + 130;
    g_beepsLeft--;
    if (!g_beepsLeft) noTone(PIN_BUZZER);
  }
#endif
}

static int presentCount();   // defined with the tracking table below

#if WIFI_ENABLED
// Wall-clock time, so the serial history says "glasses at 14:32 on Tuesday"
// rather than "at uptime 4211s". Best-effort: events stay useful without it.
static void timeEnsure() {
  if (WiFi.status() != WL_CONNECTED) return;
  static bool configured = false;
  if (!configured) {
    configTzTime(NTP_TZ, NTP_SERVER1, NTP_SERVER2);
    configured = true;
  }
  if (g_timeSynced) return;
  struct tm tm;
  if (getLocalTime(&tm, 5)) {
    g_timeSynced = true;
    char b[40];
    strftime(b, sizeof(b), "%Y-%m-%dT%H:%M:%S%z", &tm);
    Serial.printf("{\"event\":\"time_sync\",\"time\":\"%s\"}\n", b);
  }
}
#endif

// Fills buf with local ISO-8601 time, or "unsynced" if NTP has not landed yet.
static void isoNow(char* buf, size_t n) {
  struct tm tm;
#if WIFI_ENABLED
  if (g_timeSynced && getLocalTime(&tm, 5)) {
    strftime(buf, n, "%Y-%m-%dT%H:%M:%S%z", &tm);
    return;
  }
#endif
  snprintf(buf, n, "unsynced");
}

// ---------------------------------------------------------------- network

#if WIFI_ENABLED
static void wifiEnsure() {
  static uint32_t lastTry = 0;
  uint32_t now = millis();

  if (WiFi.status() == WL_CONNECTED) return;

  // Nothing stored: wait for credentials over Improv rather than guessing.
  // The device still detects glasses and drives the LED and buzzer meanwhile.
  if (!cfgConfigured()) return;

  if (lastTry && now - lastTry < 30000UL) return;
  lastTry = now;

  Serial.printf("{\"event\":\"wifi\",\"state\":\"connecting\",\"ssid\":\"%s\"}\n",
                g_cfg.wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_cfg.wifiSsid, g_cfg.wifiPass);
}
#endif

#if WIFI_ENABLED && MQTT_ENABLED
static void mqttEnsure() {
  if (g_cfg.mqttHost[0] == '\0') return;   // no broker configured
  if (WiFi.status() != WL_CONNECTED || g_mqtt.connected()) return;
  static uint32_t lastTry = 0;
  uint32_t now = millis();
  if (lastTry && now - lastTry < 10000UL) return;
  lastTry = now;
  g_mqtt.setServer(g_cfg.mqttHost, g_cfg.mqttPort);

  // Register "offline" as the Last Will before anything else, so the broker
  // publishes it on our behalf if this device drops off ungracefully.
  const char* user = (g_cfg.mqttUser[0] != '\0') ? g_cfg.mqttUser : nullptr;
  const char* pass = (g_cfg.mqttPass[0] != '\0') ? g_cfg.mqttPass : nullptr;
  bool ok = g_mqtt.connect(MQTT_CLIENT_ID, user, pass,
                           MQTT_TOPIC_AVAIL, 0, true, "offline");
  if (ok) {
    g_mqtt.publish(MQTT_TOPIC_AVAIL, "online", true);
    // Re-assert current state on every reconnect rather than assuming clear:
    // a reconnect mid-detection must not wipe a live "present".
    g_mqtt.publish(MQTT_TOPIC_STATE, presentCount() ? "present" : "clear", true);
  }
}
#endif

// Push one event out over every enabled transport. `json` goes to the webhook
// (machine-readable); `text` goes to MQTT as a display-ready line, because the
// signage renders whatever string arrives without parsing it.
static void publishEvent(const char* json, const char* text) {
#if WIFI_ENABLED
  if (WiFi.status() != WL_CONNECTED) return;
  #if WEBHOOK_ENABLED
  {
    HTTPClient http;
    if (http.begin(WEBHOOK_URL)) {
      http.setConnectTimeout(3000);
      http.setTimeout(3000);
      http.addHeader("Content-Type", "application/json");
      int code = http.POST((uint8_t*)json, strlen(json));
      if (code <= 0) Serial.printf("{\"event\":\"webhook_error\",\"code\":%d}\n", code);
      http.end();
    }
  }
  #endif
  #if MQTT_ENABLED
  if (g_mqtt.connected()) g_mqtt.publish(MQTT_TOPIC_EVENT, text);
  #endif
#else
  (void)json; (void)text;
#endif
}

static void publishState(const char* state) {
#if WIFI_ENABLED && MQTT_ENABLED
  if (g_mqtt.connected()) g_mqtt.publish(MQTT_TOPIC_STATE, state, true);
#else
  (void)state;
#endif
}

// ------------------------------------------------------------- tracking

#if WIFI_ENABLED
  #include "webui.h"
#endif

static Track* findTrack(const char* addr) {
  for (int i = 0; i < MAX_TRACKED; ++i)
    if (g_track[i].used && strcmp(g_track[i].addr, addr) == 0) return &g_track[i];
  return nullptr;
}

// Reuse the stalest slot when the table is full — a busy room should not
// blind us to a new arrival.
static Track* claimTrack(const char* addr) {
  Track* best = nullptr;
  for (int i = 0; i < MAX_TRACKED; ++i) {
    if (!g_track[i].used) { best = &g_track[i]; break; }
    if (!best || g_track[i].lastSeen < best->lastSeen) best = &g_track[i];
  }
  memset(best, 0, sizeof(Track));
  best->used = true;
  strncpy(best->addr, addr, sizeof(best->addr) - 1);
  best->firstSeen = millis();
  best->bestRssi = -128;
  return best;
}

static int trackedCount() {
  int n = 0;
  for (int i = 0; i < MAX_TRACKED; ++i) if (g_track[i].used) n++;
  return n;
}

static int presentCount() {
  int n = 0;
  for (int i = 0; i < MAX_TRACKED; ++i) if (g_track[i].used && g_track[i].present) n++;
  return n;
}

static void raiseAlert(Track* t, const char* why) {
  g_alertCount++;
  t->lastAlert = millis();
  t->present   = true;

  char ts[40];
  isoNow(ts, sizeof(ts));

  char json[400];
  snprintf(json, sizeof(json),
    "{\"event\":\"glasses_detected\",\"time\":\"%s\",\"addr\":\"%s\",\"name\":\"%s\","
    "\"vendor\":\"%s\",\"score\":%u,\"rssi\":%d,\"best_rssi\":%d,\"why\":\"%s\","
    "\"uptime_s\":%lu}",
    ts, t->addr, t->name[0] ? t->name : "", t->vendor ? t->vendor : "unknown", t->score,
    t->lastRssi, t->bestRssi, why, (unsigned long)(millis() / 1000));

  char text[128];
  snprintf(text, sizeof(text), "Glasses detected: %s (%d dBm)",
           t->vendor ? t->vendor : "unknown", t->lastRssi);

#if WIFI_ENABLED
  weblogAdd(ts, t->vendor ? t->vendor : "unknown", t->lastRssi, t->score);
#endif

  Serial.println(json);
  fireIndicators();
  publishEvent(json, text);
  publishState("present");
}

static void expireStale() {
  uint32_t now = millis();
  for (int i = 0; i < MAX_TRACKED; ++i) {
    Track* t = &g_track[i];
    if (!t->used || now - t->lastSeen < DEVICE_TIMEOUT_MS) continue;

    bool wasPresent = t->present;
    if (wasPresent) {
      char ts[40];
      isoNow(ts, sizeof(ts));
      char json[320];
      snprintf(json, sizeof(json),
        "{\"event\":\"clear\",\"time\":\"%s\",\"addr\":\"%s\",\"vendor\":\"%s\","
        "\"seen_for_s\":%lu}",
        ts, t->addr, t->vendor ? t->vendor : "unknown", (unsigned long)((t->lastSeen - t->firstSeen) / 1000));
      char text[128];
      snprintf(text, sizeof(text), "All clear: %s left (seen %lus)",
               t->vendor ? t->vendor : "unknown",
               (unsigned long)((t->lastSeen - t->firstSeen) / 1000));
      Serial.println(json);
      publishEvent(json, text);
    }
    t->used = false;
    if (wasPresent && presentCount() == 0) publishState("clear");
  }
}

static void ingest(const Obs& o) {
  Track* t = findTrack(o.addr);
  if (!t) t = claimTrack(o.addr);

  t->lastSeen = millis();
  t->lastRssi = o.rssi;
  if (o.rssi > t->bestRssi) t->bestRssi = o.rssi;
  if (o.score > t->score)   { t->score = o.score; t->vendor = o.vendor; }
  if (o.name[0] && !t->name[0]) strncpy(t->name, o.name, sizeof(t->name) - 1);
  if (!t->vendor) t->vendor = o.vendor;

  if (g_verbose) {
    Serial.printf("{\"event\":\"adv\",\"addr\":\"%s\",\"name\":\"%s\",\"rssi\":%d,"
                  "\"score\":%u,\"why\":\"%s\"}\n",
                  o.addr, o.name, o.rssi, o.score, o.why);
  }

  bool strong  = o.score >= ALERT_SCORE_THRESHOLD;
  bool close   = o.rssi >= RSSI_ALERT_THRESHOLD;
  if (!strong || !close) return;

  if (++t->hits < CONFIRM_HITS) return;

  uint32_t now = millis();
  bool fresh = !t->lastAlert || (now - t->lastAlert) >= REALERT_COOLDOWN_MS;
  if (fresh) raiseAlert(t, o.why);
  else t->present = true;
}

// -------------------------------------------------------- serial console

// Blocking on purpose: this is a manual "can you see it?" test, and BLE
// scanning continues on its own task throughout.
static void ledSelfTest() {
  Serial.printf("{\"event\":\"led_test\",\"start\":true,\"status_pin\":%d,"
                "\"alert_pin\":%d,\"buzzer_pin\":%d,\"blinks\":5,"
                "\"expect\":\"5 slow blinks, 0.5s on / 0.5s off\"}\n",
                PIN_STATUS_LED, PIN_ALERT_LED, PIN_BUZZER);

  for (int i = 1; i <= 5; ++i) {
    digitalWrite(PIN_STATUS_LED, HIGH);
#if PIN_ALERT_LED >= 0
    digitalWrite(PIN_ALERT_LED, HIGH);
#endif
    delay(500);
    // Reading back an output pin confirms the pad really is being driven,
    // which separates "firmware bug" from "LED not wired / not present".
    Serial.printf("{\"event\":\"led_test\",\"blink\":%d,\"drive\":1,\"readback\":%d}\n",
                  i, digitalRead(PIN_STATUS_LED));

    digitalWrite(PIN_STATUS_LED, LOW);
#if PIN_ALERT_LED >= 0
    digitalWrite(PIN_ALERT_LED, LOW);
#endif
    delay(500);
    Serial.printf("{\"event\":\"led_test\",\"blink\":%d,\"drive\":0,\"readback\":%d}\n",
                  i, digitalRead(PIN_STATUS_LED));
  }

  g_lastBlink = millis();
  g_ledOn = false;
  Serial.println("{\"event\":\"led_test\",\"done\":true}");
}

static void printStatus() {
  char statusTs[40];
  isoNow(statusTs, sizeof(statusTs));
  Serial.printf("{\"event\":\"status\",\"fw\":\"%s\",\"uptime_s\":%lu,\"adv_seen\":%lu,"
                "\"alerts\":%lu,\"tracked\":%d,\"present\":%d,\"verbose\":%d,"
                "\"threshold\":%d,\"rssi_gate\":%d,\"heap\":%lu,"
                "\"scan_restarts\":%lu,\"adv_age_ms\":%lu,\"time\":\"%s\"",
                FW_VERSION, (unsigned long)(millis() / 1000),
                (unsigned long)g_advSeen, (unsigned long)g_alertCount,
                trackedCount(), presentCount(), g_verbose ? 1 : 0,
                ALERT_SCORE_THRESHOLD, RSSI_ALERT_THRESHOLD,
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)g_scanRestarts,
                (unsigned long)(millis() - g_lastAdvMs), statusTs);
#if WIFI_ENABLED
  Serial.printf(",\"wifi\":\"%s\",\"ip\":\"%s\",\"provisioned\":%d",
                WiFi.status() == WL_CONNECTED ? "up" : "down",
                WiFi.localIP().toString().c_str(),
                cfgConfigured() ? 1 : 0);
#endif
  Serial.println("}");
}

static void handleSerial() {
  if (!Serial.available()) return;

#if WIFI_ENABLED
  // Improv frames start with "IMPROV". Peek so the library still owns the
  // whole packet, then keep feeding it: it consumes one byte per call, so a
  // single hand-off would strand the rest of the frame and the console would
  // eat it. The short window closes on its own once the burst ends.
  static uint32_t improvUntil = 0;
  if (Serial.peek() == 'I' || (int32_t)(improvUntil - millis()) > 0) {
    for (int i = 0; i < 64 && Serial.available(); ++i) g_improv.handleSerial();
    improvUntil = millis() + 250;
    return;
  }
#endif

  int c = Serial.read();
  switch (c) {
    case 's': printStatus(); break;
    case 'v':
      g_verbose = !g_verbose;
      Serial.printf("{\"event\":\"verbose\",\"on\":%d}\n", g_verbose ? 1 : 0);
      break;
    case 'd':
      for (int i = 0; i < MAX_TRACKED; ++i) {
        Track* t = &g_track[i];
        if (!t->used) continue;
        Serial.printf("{\"event\":\"track\",\"addr\":\"%s\",\"name\":\"%s\",\"vendor\":\"%s\","
                      "\"score\":%u,\"rssi\":%d,\"best\":%d,\"hits\":%u,\"present\":%d,"
                      "\"age_s\":%lu}\n",
                      t->addr, t->name, t->vendor ? t->vendor : "unknown", t->score,
                      t->lastRssi, t->bestRssi, t->hits, t->present ? 1 : 0,
                      (unsigned long)((millis() - t->firstSeen) / 1000));
      }
      break;
    case 'l': ledSelfTest(); break;
#if WIFI_ENABLED
    case 'p':
      // Wipe credentials and restart, returning the device to an
      // unprovisioned state ready for Improv. The hardware equivalent is
      // holding BOOT while powering on.
      Serial.println("{\"event\":\"reprovision\",\"action\":\"clearing credentials\"}");
      cfgClear();
      delay(300);
      ESP.restart();
      break;
#endif
    case 'x':
      // Fault injection: kill the scan without restarting it, to prove the
      // watchdog actually recovers. NimBLE's stop() does not raise onScanEnd,
      // so this exercises the stall path specifically -- expect silence, then
      // a scan_restart with trigger "stall" after SCAN_STALL_TIMEOUT_MS.
      Serial.println("{\"event\":\"fault_injection\",\"action\":\"scan_stopped\"}");
      NimBLEDevice::getScan()->stop();
      break;
    case 't':
      Serial.println("{\"event\":\"test_alert\"}");
      fireIndicators();
      publishEvent("{\"event\":\"test_alert\",\"source\":\"serial\"}",
                   "Canary test alert");
      break;
    case 'h':
      Serial.println("{\"event\":\"help\",\"keys\":\"s=status d=dump v=verbose l=led-test t=test-alert x=kill-scan p=reprovision h=help\"}");
      break;
    default: break;
  }
}

// -------------------------------------------------------------- lifecycle

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);
#if PIN_ALERT_LED >= 0
  pinMode(PIN_ALERT_LED, OUTPUT);
  digitalWrite(PIN_ALERT_LED, LOW);
#endif
#if PIN_BUZZER >= 0
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);
#endif

  memset(g_track, 0, sizeof(g_track));
  g_queue = xQueueCreate(24, sizeof(Obs));

  Serial.println();
  Serial.printf("{\"event\":\"boot\",\"fw\":\"%s\",\"broad_vendors\":%d,"
                "\"threshold\":%d,\"rssi_gate\":%d}\n",
                FW_VERSION, ENABLE_BROAD_VENDORS,
                ALERT_SCORE_THRESHOLD, RSSI_ALERT_THRESHOLD);
  Serial.println("{\"event\":\"help\",\"keys\":\"s=status d=dump v=verbose l=led-test t=test-alert x=kill-scan p=reprovision h=help\"}");

#if WIFI_ENABLED
  checkFactoryReset();   // BOOT held at power-on wipes stored credentials
  cfgLoad();             // NVS, falling back to secrets.h defaults
  wifiEnsure();          // connects only if credentials are stored
#endif

#if WIFI_ENABLED
  g_improv.setDeviceInfo(ImprovTypes::ChipFamily::CF_ESP32, "SmartGlassCanary",
                         FW_VERSION, "Smart Glass Canary");
  g_improv.setCustomConnectWiFi(improvConnect);
  g_improv.onImprovConnected(improvOnConnected);
  g_improv.onImprovError(improvOnError);
#endif

  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&g_scanCb, false);
  scan->setActiveScan(true);        // ask for scan responses; names often live there
  scan->setInterval(160);           // 160 * 0.625ms = 100ms
  scan->setWindow(160);             // full duty cycle — listen constantly
  scan->setDuplicateFilter(false);  // we want repeat reports for RSSI tracking
  scan->setMaxResults(0);           // callback-only; do not accumulate results
  scan->start(0, false);            // 0 = scan forever
  g_lastAdvMs = millis();           // arm the stall watchdog

  Serial.println("{\"event\":\"scanning\"}");
}

void loop() {
  Obs o;
  while (xQueueReceive(g_queue, &o, 0) == pdTRUE) ingest(o);

  expireStale();
  serviceScanWatchdog();
  serviceIndicators();
  handleSerial();

#if WIFI_ENABLED
  wifiEnsure();
  // Serve the status page only while actually on the network. Driven here
  // rather than inside wifiEnsure() so it sits after webui.h is included.
  if (WiFi.status() == WL_CONNECTED) webStart(); else webStop();
  webService();
  timeEnsure();
  #if MQTT_ENABLED
    mqttEnsure();
    if (g_mqtt.connected()) g_mqtt.loop();
  #endif
#endif

  // Keep the BLE host task fed.
  delay(10);
}
