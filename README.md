# Smart Glass Canary

An ESP32 that listens for camera-equipped smart glasses — Meta Ray-Ban, Oakley
Meta, Snap Spectacles and similar — and raises an alert when a likely pair comes
into Bluetooth range.

It is a **receive-mostly** device. It listens to the advertising packets these
devices broadcast publicly and continuously, and never connects, pairs, or
attempts to access anything. Functionally it is a smoke detector for a specific
kind of radio traffic.

One caveat on "passive": the firmware uses **active scanning**, which does
transmit — it sends a scan request to advertisers to solicit their scan
response, because that is where device names frequently live, and the name is
the single strongest detection signal. The practical consequences are that the
canary is itself visible to other BLE scanners, and that it draws more current
than a purely listening device. If you want a genuinely silent receiver,
`scan->setActiveScan(false)` in `setup()` will do it, at a real cost in
detection rate.

## Hardware

| | |
|---|---|
| Board | ESP32 dev board (CH340 USB-serial), `/dev/ttyUSB0` |
| Status LED | GPIO2 — onboard, no wiring needed |
| Buzzer | GPIO25 → piezo → GND (optional) |
| Alert LED | disabled by default; set `PIN_ALERT_LED` to use one |

For a passive piezo buzzer, wire it straight between GPIO25 and GND. An active
buzzer works too but will sound at its own pitch and ignore `BUZZER_FREQ_HZ`.
If you would rather stay silent, set `PIN_BUZZER` to `-1`.

## Build and flash

Before the first build, create your credentials file:

```bash
cp firmware/secrets.h.example firmware/secrets.h
$EDITOR firmware/secrets.h      # Wi-Fi SSID/password, MQTT user/password
```

`config.h` includes `secrets.h`, so the build will not compile without it.
Then:

```bash
./flash.sh      # compile + upload
./monitor.sh    # watch the serial output at 115200
```

`firmware/secrets.h` is listed in `.gitignore` and **must never be committed**.
`firmware/secrets.h.example` is the committed template — it holds placeholders
only, and is the file to update if you ever add a new credential.

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

## How detection works

Detection is **scored, not binary**. Each signal in an advertisement adds
points, and a device alerts once it crosses the threshold:

| Signal | Weight | Notes |
|---|---|---|
| SIG member service UUID | 35–70 | `0xFD5F` (Meta Platforms Technologies) is the single strongest signal available |
| Distinctive advertised name | 30–70 | `Ray-Ban`, `Spectacles`, `Xreal` decisive; `Meta`, `Frame` deliberately weak |
| Manufacturer company ID | 20–65 | includes `0x0D53` EssilorLuxottica, who make the Ray-Ban/Oakley frames |
| Public-MAC vendor OUI | 40–50 | public addresses only — see caveats below |

The weights matter. A bare `"Meta"` in a device name scores 35 — deliberately
*under* the threshold, because plenty of innocent things contain that string. It
only alerts when something corroborates it, such as a Meta Platforms company ID
in the same packet. Conversely `"Ray-Ban"` alone is decisive.

Company IDs and member UUIDs come from the Bluetooth SIG assigned-numbers
database, so the values themselves are exact. What is heuristic is the inference from vendor to product:
Meta and Amazon ship that same ID in phones, speakers and VR headsets, which is
why those entries score mid-range rather than conclusive.

OUI matching has a subtlety worth knowing before you add entries. Nreal/Xreal,
RayNeo, Solos and Halliday own only 28-bit **MA-M** blocks, and the 24-bit
prefix of each is shared with roughly fifteen unrelated companies. The firmware
therefore stores a nibble alongside those prefixes and matches the full 28 bits;
matching 24 would be almost entirely false positives. Meta, Snap, Vuzix and
Luxottica own full 24-bit MA-L blocks and need no such check.

Edit `firmware/signatures.h` to add rules or re-weight existing ones. Per-vendor
provenance and confidence for each signal is recorded in `docs/fingerprints.md`,
which grades every value as confirmed, reported or unverified.

One entry is deliberately **absent**. A Ray-Ban manufacturer-data payload
labelled `META_RB_GLASS` circulates widely and looks authoritative. It is an
illustrative example from a project README, not a capture: its declared length
disagrees with its actual length, it cites Google's Eddystone UUID rather than
Meta's `0xFD5F`, it marks an audio device as `BR/EDR Not Supported`, and its MAC
is a placeholder sequence. Compiling it in would have produced a rule that
matches nothing, forever, while looking like coverage.

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

## Testing

`tests/rf-test.sh` impersonates smart glasses over the air using a local BlueZ
adapter and asserts the canary reacts as the rules say it should. It resets the
board between cases so the re-alert cooldown cannot mask a result.

```bash
./tests/rf-test.sh          # cases A-D, ~2 minutes
./tests/rf-test.sh --full   # adds the all-clear timeout case, ~90s more
```

All cases verified on hardware against a TP-Link UB500 adapter:

| Case | Advertised | Score | Expected | Result |
|---|---|---|---|---|
| A | local name `Ray-Ban` | 70 | alert | detected, `why:"name"` |
| B | company ID `0x058E` only | 55 | **no alert** | scored 55, zero alerts |
| C | name `Meta` + company ID `0x058E` | 90 | alert | detected, `why:"name+mfr"` |
| D | service UUID `0xFD5F` | 70 | alert | detected, `why:"uuid"` |
| E | `Ray-Ban`, then silence | — | all-clear | detect, then `clear` after the timeout |

Case B is the important one. It advertises a genuine Meta company ID and
deliberately produces **no alert**, because 55 is below the threshold of 60.
Case C then adds a weak name to the same packet and does alert at 90. Together
they demonstrate the thing the scoring design exists for: corroboration, rather
than a hair trigger on any single Meta-flavoured byte.

A useful accident during case D: BlueZ labels `0xFD5F` in its own logs as
`Oculus VR, LLC`, independently corroborating that UUID as a Meta/Reality Labs
assignment from a completely different database than the one the rules were
built from.

### What the tests do not cover

- **That the LED physically lights.** The `l` command reads the pin back after
  driving it, which proves the firmware and the pad, not that photons came out.
  Confirm that with your eyes once.
- **The RSSI gate.** Exercising `RSSI_ALERT_THRESHOLD` means physically moving a
  transmitter out of range; the test rig sits on the desk at roughly -40 dBm.
- **Real glasses.** Everything above is a BlueZ adapter imitating the *signals*
  smart glasses emit. It validates the detection logic thoroughly and says
  nothing about what a real pair broadcasts in the state you care about. See the
  connected-and-invisible problem under "What this can and cannot do".

## What this can and cannot do

Worth being straight about, because the failure mode is a **silent false
negative** — a canary that says nothing while glasses are present looks
identical to one working perfectly.

**Works well when** the glasses are advertising openly: powered on and
unpaired, in pairing mode, or between phone connections. That covers a
meaningful share of real-world situations.

**Degrades or fails when:**

- **The glasses are already paired and connected to their owner's phone.** This
  is the central limitation, and it is worse than "degraded": two independent
  hands-on sources report that Meta glasses stop advertising **entirely** once
  connected to their phone, becoming invisible to every BLE scanner, not just
  this one. The practical consequence is stark — the canary reliably sees
  glasses while they power on or pair, and goes blind during the actual
  recording session. That is device behaviour, not a firmware bug, and no
  amount of signature tuning fixes it.
- **BLE address randomisation.** Meta glasses advertise from a random static
  address, and most other modern devices rotate a resolvable private address
  every 15 minutes or so. The firmware only applies OUI matching to public
  addresses, because a random address's top bytes are meaningless. Neither of
  the two real-world detectors surveyed during this build uses OUI matching at
  all — treat the OUI table as a bonus, not a primary signal, and do not expect
  to track a specific pair of glasses over time.

- **Some products are not BLE-detectable at all.** Xreal Air/Air 2/One are
  driven purely over USB HID and expose no Bluetooth control path (they also
  have no camera). The `xreal` name rule is kept for the wider product line,
  but those particular models will never appear.
- **Unknown or updated products.** Detection depends on a signature table.
  A firmware update that changes an advertised name is enough to make a
  previously detected device invisible.
- **RSSI is a poor distance estimate.** It swings with orientation, bodies and
  walls. `-75` is a rough proxy for "same room", not a measurement.

So: treat a **detection** as good evidence, and treat **silence as weak
evidence** — it is not proof that no one nearby is recording. This is an
awareness tool, not a guarantee.

Two further notes. Recording indicators on these products are separate from the
radio, so this tells you glasses are *present*, never whether a camera is
*running*. And a BLE scan sees every advertiser nearby, not just eyewear —
verbose mode in particular will show you your neighbours' devices. What you do
with that is on you; logging your own space is very different from profiling
other people's.
