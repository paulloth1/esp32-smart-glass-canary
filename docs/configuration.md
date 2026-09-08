# Configuration

Everything tunable lives in `firmware/config.h`. Credentials live in `firmware/secrets.h`, which is gitignored.

## Configuration

Everything you would normally want to change is in `firmware/config.h`.
Credentials are the exception — those live in `firmware/secrets.h`, which is
gitignored (see "Build and flash" above).

| Setting | Default | What it does |
|---|---|---|
| `RSSI_ALERT_THRESHOLD` | `-75` | How close is "in range". `-75` ≈ same room; `-85` reaches into the next one and alerts far more often |
| `ALERT_SCORE_THRESHOLD` | `60` | Confidence needed to call something glasses |
| `CONFIRM_HITS` | `2` | Sightings required before alerting — suppresses one-off junk packets |
| `ENABLE_BROAD_VENDORS` | `1` | `0` restricts detection to the Meta family only |
| `DEVICE_TIMEOUT_MS` | `45000` | Silence after which a device is considered gone (fires an all-clear) |
| `REALERT_COOLDOWN_MS` | `300000` | Minimum gap between repeat alerts for the same device |
| `LOG_ALL_DEVICES` | `0` | Dump every BLE advertiser, not just matches |
| `HEARTBEAT_PERIOD_MS` | `30000` | Idle "still alive" wink interval. `0` keeps the LED dark until something is detected |
| `SCAN_STALL_TIMEOUT_MS` | `60000` | Silence after which the BLE scan is assumed dead and restarted — see below |
| `SCAN_RETRY_BACKOFF_MS` | `2000` | If restarting a stalled scan fails, how long before trying again |
| `NTP_TZ` | `CET-1CEST,M3.5.0,M10.5.0/3` | POSIX TZ string for event timestamps. Central European with EU DST rules; `UTC0` for UTC |
| `NTP_SERVER1` | `pool.ntp.org` | Primary time server |
| `NTP_SERVER2` | `time.nist.gov` | Fallback time server |

Wi-Fi behaviour and push outputs are configured in `config.h` too — but the
SSID, password and broker credentials themselves are in `secrets.h`.
`WEBHOOK_ENABLED` POSTs the event JSON to any URL (ntfy.sh, Home Assistant, a
Discord/Slack hook, your own listener). `MQTT_ENABLED` publishes events to
`canary/glasses/event` and keeps a retained `present`/`clear` value on
`canary/glasses/state`.

Fill in `secrets.h` before enabling either. The firmware refuses to call
`WiFi.begin()` while the SSID is still the `YOUR_SSID` placeholder, so a
half-configured board will not sit retrying a network that does not exist.

### The scan watchdog

A stalled BLE scan is this project's worst failure mode, because nothing about
it looks wrong. The LED keeps winking its heartbeat, MQTT keeps reporting
`clear`, the status line keeps counting uptime — and the device is deaf. You
would have no way to tell that from a genuinely quiet room.

So the firmware watches its own radio and restarts the scan on either of two
signals:

- **An explicit scan-end event.** NimBLE can end a scan by itself (controller
  error, duration lapse). That emits `{"event":"scan_end","reason":...}`.
- **Total silence for `SCAN_STALL_TIMEOUT_MS`** (default 60s). In any populated
  area advertisements arrive several times a second, so a full minute of
  *nothing at all* means the radio stopped, not that the neighbours went out.

Either path logs `{"event":"scan_restart","trigger":"scan_end"|"stall",...}`.

The status line (`s`) reports two counters for this: `scan_restarts` and
`adv_age_ms`, the time since the last advertisement of any kind. A non-zero
`scan_restarts` is worth noticing rather than ignoring — the watchdog did its
job, but it also means the radio died at least once, and a count that climbs
steadily points at a power, thermal or library problem underneath.

### Timestamps

Events carry wall-clock time so the serial history reads "glasses at 14:32"
rather than "at uptime 4211s". The detection and all-clear JSON events and the
status line all include a `"time"` field in ISO-8601 local time, using the zone
in `NTP_TZ`.

This needs Wi-Fi. Before NTP has synced — and always when `WIFI_ENABLED` is
`0` — the field reads `"unsynced"` rather than a plausible-looking wrong time.
That is deliberate: a wrong timestamp on a detection is worse than no
timestamp. A successful sync logs `{"event":"time_sync","time":"..."}` once.

## Serial console

Connect at 115200 and press a key:

| Key | |
|---|---|
| `s` | status — uptime, advertisements seen, alerts, free heap, Wi-Fi state, `scan_restarts`, `adv_age_ms`, clock |
| `d` | dump the current device table |
| `v` | toggle verbose mode (log every advertiser) |
| `l` | LED self-test — 5 slow blinks with pin readback |
| `t` | fire a test alert through every output |
| `h` | help |

Output is one JSON object per line, so you can pipe it straight into something:

```json
{"event":"glasses_detected","time":"2026-09-08T14:32:07+0200","addr":"a4:c1:38:...",
 "name":"Ray-Ban Meta","vendor":"Meta Ray-Ban","score":90,"rssi":-61,
 "best_rssi":-58,"why":"name+mfr","uptime_s":412}
```

`time` is `"unsynced"` until NTP lands, or whenever Wi-Fi is off.

## Tuning it to your environment

Run `v` for verbose mode somewhere busy and watch what actually shows up. Real
BLE environments are noisy and every home is different. If you get false alarms,
raise `ALERT_SCORE_THRESHOLD` or drop the offending rule's weight in
`signatures.h`. If you have the target glasses in hand, note the name and
manufacturer bytes they advertise and add a high-confidence rule for exactly
those — a signature you captured yourself beats any generic table.
