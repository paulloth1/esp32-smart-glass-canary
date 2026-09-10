#pragma once

// Credentials live in secrets.h, which is gitignored. Copy
// secrets.h.example to secrets.h and fill it in.
#include "secrets.h"
/*
 * Smart Glass Canary — user configuration
 *
 * Everything you are likely to change lives in this file.
 * Credentials are NOT here -- they live in secrets.h, which is gitignored.
 *
 * Note WEBHOOK_URL below is site-specific. It is not a secret, but it does
 * describe your LAN, so blank it before making this repo public.
 */

// --------------------------------------------------------------- watchdog

// If no BLE advertisement arrives for this long, assume scanning has died and
// restart it. In any populated area adverts arrive constantly -- several per
// second -- so a full minute of total silence means the radio stopped, not
// that the room went quiet. This exists because a stalled scan is otherwise
// invisible: the LED keeps winking and MQTT keeps saying "clear" while the
// device notices nothing at all.
#define SCAN_STALL_TIMEOUT_MS 60000UL

// If restarting the scan fails (the controller can be busy or resyncing after
// a host reset, which is exactly when a restart is most likely to be needed),
// retry after this long rather than waiting out another full stall timeout.
#define SCAN_RETRY_BACKOFF_MS  2000UL

// ------------------------------------------------------------------- time

// Timezone for event timestamps, POSIX TZ format. Default is Central European
// (Berlin) with EU daylight-saving rules. UTC would be "UTC0".
#define NTP_TZ                 "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER1            "pool.ntp.org"
#define NTP_SERVER2            "time.nist.gov"

// ---------------------------------------------------------------- detection

// Alert only when a candidate is at least this strong (dBm).
// -75 is roughly "same room". -85 catches the next room over and rings more.
#define RSSI_ALERT_THRESHOLD   -75

// A device must match on this many separate scan sightings before it alerts.
// Guards against one-off garbage packets. 2 is a good balance.
#define CONFIRM_HITS            2

// Confidence score a device must reach to be treated as smart glasses.
// See signatures.h for how individual signals are weighted.
#define ALERT_SCORE_THRESHOLD  60

// Include the wider vendor list (Snap, Amazon, Xreal, Rokid, Vuzix, ...)
// alongside the Meta/Ray-Ban rules. 0 = Meta family only, far fewer false hits.
#define ENABLE_BROAD_VENDORS    1

// Drop a device from the tracking table after this long without a sighting.
// Leaving the "present" state fires an all-clear.
#define DEVICE_TIMEOUT_MS      45000UL

// Minimum gap between repeat alerts for the same device, so a pair of glasses
// parked next to you does not alert every scan cycle.
#define REALERT_COOLDOWN_MS   300000UL

// Log every BLE advertiser seen, not just matches. Extremely chatty — turn on
// only when you want to profile your environment or tune the rules.
#define LOG_ALL_DEVICES         0

// ------------------------------------------------------------------- pins

// Onboard LED. GPIO2 on nearly every classic ESP32 DevKit board.
#define PIN_STATUS_LED          2

// Idle heartbeat: a brief wink every this many milliseconds, meaning "alive
// and scanning, nothing detected". The board's own power LED already shows
// it has power, so this only needs to be occasional. Set to 0 to keep the
// LED dark when idle, so that ANY blink means a detection.
#define HEARTBEAT_PERIOD_MS 30000UL
#define HEARTBEAT_WINK_MS      40UL

// External alert LED. Set to -1 if you are not wiring one.
#define PIN_ALERT_LED          -1

// Piezo buzzer. Set to -1 to stay silent. Avoid the strapping pins
// (0, 2, 5, 12, 15); 13, 25, 26, 27, 32 and 33 are all safe choices.
//
// GPIO13 is preferred over the otherwise-equivalent GPIO14: 14 emits a short
// PWM pulse while the chip boots, which a buzzer turns into a chirp on every
// reset. 13 is quiet until the sketch drives it.
#define PIN_BUZZER             13
// 4000Hz measured by ear as the loudest step of the `f` sweep on the fitted
// piezo. Resonance is sharp and part-specific -- re-run the sweep if you swap
// the buzzer rather than assuming this number carries over.
#define BUZZER_FREQ_HZ       4000
#define BUZZER_BEEPS            3

// ----------------------------------------------------------- provisioning

// Settings arrive over Improv Wi-Fi on the USB serial link -- the same
// connection the browser used to flash the device -- and are kept in NVS. That
// means a distributable firmware image contains no secrets at all, and there is
// no setup access point to broadcast or defend.


// Hold the BOOT button (GPIO0) down while powering on for this long to erase
// stored credentials. This is the recovery path when the Wi-Fi password
// changes or the device changes hands; re-provision over serial afterwards.
#define PIN_FACTORY_RESET       0
#define FACTORY_RESET_HOLD_MS 3000UL

// -------------------------------------------------------------------- wifi

// Leave this at 0 until you have filled in real credentials below AND
// enabled a push output. With placeholder credentials the board retries a
// non-existent network forever, logging "sta is connecting, cannot set
// config" every 30s for no benefit. The firmware also refuses to call
// WiFi.begin() while WIFI_SSID is still the placeholder, as a backstop.
#define WIFI_ENABLED            1
#define WIFI_CONNECT_TIMEOUT_MS 15000UL


// ----------------------------------------------------------------- webhook

// POSTs a JSON body to this URL on each alert. Works with ntfy.sh, Home
// Assistant webhooks, Discord/Slack incoming hooks, or anything of your own.
// Plain HTTP keeps the binary small; HTTPS also works but costs ~40KB flash.
#define WEBHOOK_ENABLED         0
#define WEBHOOK_URL            "http://192.168.1.10:8123/api/webhook/glass_canary"

// ------------------------------------------------------------------- mqtt

#define MQTT_ENABLED            1
// MQTT_HOST, MQTT_USER and MQTT_PASS live in secrets.h -- they describe your
// deployment, not the firmware's behaviour. The firmware skips MQTT entirely
// while MQTT_HOST is empty, so it will not spin retrying a broker that is
// not there.
#define MQTT_PORT              1883
#define MQTT_CLIENT_ID         "glass-canary"
// Payload on the event topic is a display-ready line, e.g.
//   "Glasses detected: Meta Ray-Ban (-49 dBm)"
//   "All clear: Meta Ray-Ban left (seen 11s)"
#define MQTT_TOPIC_EVENT       "canary/glasses/event"
#define MQTT_TOPIC_STATE       "canary/glasses/state"   // retained: "clear" / "present"

// Availability, retained. The canary publishes "online" when it connects and
// registers "offline" as its MQTT Last Will, so the broker announces the
// canary's death even if it loses power or crashes. Without this a dead canary
// is indistinguishable from a quiet one -- the signage would keep showing a
// stale state forever. Subscribe to this if you display anything at all.
#define MQTT_TOPIC_AVAIL       "canary/glasses/status"  // retained: "online" / "offline"
