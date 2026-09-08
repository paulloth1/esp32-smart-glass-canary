# Signage integration

What the canary publishes, and what your display needs to subscribe to.

**Broker:** `BROKER_IP:1883` — MQTT 3.1.1, QoS 0. Substitute your own broker
address; the canary reads it from `MQTT_HOST` in `firmware/secrets.h`.
**Canary client id:** `glass-canary`.

## Topics

| Topic | Retained | Payload |
|---|---|---|
| `canary/glasses/event` | no | A display-ready line of plain UTF-8 text |
| `canary/glasses/state` | **yes** | `present` or `clear` |
| `canary/glasses/status` | **yes** | `online` or `offline` |

### `canary/glasses/event`
Fires on each detection and each all-clear. Render it verbatim — no parsing
needed. Exactly these shapes:

```
Glasses detected: Meta Ray-Ban (-49 dBm)
All clear: Meta Ray-Ban left (seen 11s)
Canary test alert
```

The vendor string comes from the signature tables, so it can also read
`Meta Platforms Technologies`, `Snap Spectacles`, `Meta (unspecified)` and so
on. Treat it as free text and give it room — budget for ~60 characters.

### `canary/glasses/state`
Retained, so a display that restarts immediately learns the current situation
without waiting for the next event. Use this to drive a persistent indicator
(a banner, a colour, an icon). Use `event` for the transient message.

### `canary/glasses/status`
Retained availability, backed by an MQTT **Last Will**. The canary publishes
`online` when it connects, and the broker publishes `offline` on its behalf if
the device drops off ungracefully — power loss, crash, out of Wi-Fi range.

**Subscribe to this one.** Without it a dead canary looks exactly like a quiet
canary: `state` would sit at its last value forever and the display would
imply "all clear" when in truth nothing is watching. If `status` is `offline`,
show that instead of the state — an unknown is not an all-clear.

Timing depends on how the canary died:

- **Crash or reboot** — the device reclaims its client id, the broker closes the
  stale connection and publishes the will immediately. Verified: `offline` then
  `online` arrived in the same second after a hard reset.
- **Power loss or out of range** — nothing reconnects, so the broker waits for
  the keepalive to lapse first. Expect roughly 15–25s before `offline` appears.
  This path follows from the same mechanism but was not directly exercised in
  testing; to confirm it yourself, unplug the canary and watch the topic.

Either way, do not treat a missing `offline` in the first few seconds as proof
the canary is alive.

## Minimal subscriber

Python, with `paho-mqtt` (`pip install paho-mqtt`):

```python
import paho.mqtt.client as mqtt

BROKER = "BROKER_IP"

def on_connect(c, _u, _f, _rc):
    c.subscribe([("canary/glasses/event",  0),
                 ("canary/glasses/state",  0),
                 ("canary/glasses/status", 0)])

def on_message(_c, _u, msg):
    topic, text = msg.topic.rsplit("/", 1)[1], msg.payload.decode()
    if topic == "status" and text == "offline":
        show_banner("CANARY OFFLINE - not watching", colour="grey")
    elif topic == "state":
        show_banner("GLASSES NEARBY" if text == "present" else "clear",
                    colour="red" if text == "present" else "green")
    elif topic == "event":
        show_message(text)          # render verbatim

c = mqtt.Client()
c.on_connect, c.on_message = on_connect, on_message
c.connect(BROKER, 1883, 60)
c.loop_forever()
```

Shell, to eyeball the traffic:

```bash
mosquitto_sub -h BROKER_IP -t 'canary/glasses/#' -v
```

Or with no dependencies at all, using the watcher in this repo:

```bash
./tests/mqtt-watch.py                       # defaults to the broker above
./tests/mqtt-watch.py BROKER_IP 'canary/#'
```

## If the signage is a browser page

Plain MQTT over TCP is not reachable from JavaScript — you need the broker's
**WebSocket listener**, which is not currently enabled (port 9001 was closed
when scanned). Add to `mosquitto.conf` on `.115`:

```
listener 9001
protocol websockets
allow_anonymous true
```

Then use MQTT.js against `ws://BROKER_IP:9001`.


## Letting the canary find your broker

A device flashed from the browser installer has no broker address: Improv
carries only Wi-Fi credentials, and the device page is read-only. So when
`MQTT_HOST` is empty the firmware looks for a broker advertised on the LAN as
`_mqtt._tcp` and uses the first one that answers, retrying every 60 seconds
until it finds one.

**Mosquitto does not advertise itself.** Nothing happens until you tell Avahi
to announce it. On the broker host, create
`/etc/avahi/services/mqtt.service`:

```xml
<?xml version="1.0" standalone='no'?>
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
  <name replace-wildcards="yes">MQTT on %h</name>
  <service>
    <type>_mqtt._tcp</type>
    <port>1883</port>
  </service>
</service-group>
```

Then `sudo systemctl restart avahi-daemon`. Check it from another machine with:

```bash
avahi-browse -tr _mqtt._tcp
```

A configured `MQTT_HOST` always wins; discovery only fills the gap when none is
set. The address is held in RAM rather than saved, so a broker that moves is
picked up again on the next boot instead of leaving a stale address behind.

### Worth knowing

- **First responder wins.** With more than one broker advertising, the choice is
  arbitrary — there is no signal to rank them by. Set `MQTT_HOST` explicitly if
  that matters.
- **Anything on the LAN can claim to be the broker.** mDNS is unauthenticated,
  so a device that trusts discovery will publish detection events to whoever
  answers first. On a home network that is usually fine; on a shared or
  untrusted one, configure the broker explicitly instead.
- Verified end to end: with `MQTT_HOST` blank, an advertised broker was found
  and connected to without any configuration on the device.

## Things worth knowing

- **QoS 0** — events are fire-and-forget and can be lost on a flaky link. The
  retained `state` and `status` topics are the reliable source of truth; treat
  `event` as a nice-to-have notification rather than an audit log.
- **No authentication.** The broker accepts anonymous connections, so anything
  on the LAN can publish to these topics too. Fine for a home network; worth
  revisiting if the signage ever faces anywhere less trusted.
- **A stalled scan now self-heals.** The firmware watches its own BLE radio and
  restarts the scan if it ends unexpectedly or hears nothing at all for a
  minute. Previously a dead scan could leave `state` sitting at `clear` and the
  event topic silent indefinitely, with no outward sign. A long quiet stretch is
  therefore more trustworthy than it used to be — but it is still not proof of
  absence, for the reason below.
- **A detection is evidence; silence is not.** Meta glasses stop advertising
  once connected to a phone, so `clear` means "nothing detected", never
  "nobody is recording". If the signage phrases it as "all clear", that
  overstates what the device actually knows. See the README's limitations.
