# Smart Glass Canary

An ESP32 that warns you when camera-equipped smart glasses — Meta Ray-Ban,
Oakley Meta, Snap Spectacles and similar — come into Bluetooth range. It
signals with an onboard LED, an optional buzzer, and MQTT.

**[Install it from your browser →](https://paulloth1.github.io/esp32-smart-glass-canary/)**
Chrome or Edge, no toolchain. Wi-Fi is set up over the same USB connection.

## What it can and cannot do

Worth reading before you trust it, because the failure mode is silence.

**It reliably catches** glasses powering on, pairing, or sitting between phone
connections.

**It goes blind** once glasses are connected to their owner's phone — they stop
advertising entirely, to every BLE scanner, not just this one. That is the
normal state for a device someone is wearing, so the canary can be quiet
during an actual recording session.

So: **a detection is good evidence. Silence is weak evidence** — not proof that
nobody nearby is recording. It also tells you glasses are *present*, never
whether a camera is *running*. This is an awareness tool, not a guarantee.

## How it works

Detection is scored, not binary. Advertised name, Bluetooth SIG manufacturer ID,
public-MAC OUI and service UUID each add points; a device alerts on crossing a
threshold. Weak signals stay deliberately below it, so `"Meta"` alone scores 35
and stays silent while `"Meta"` plus a Meta company ID scores 90 and alerts.

Vendor signatures are graded by confidence in
[docs/fingerprints.md](docs/fingerprints.md). One widely-circulated but
fabricated Ray-Ban payload is documented there and deliberately excluded — a
bogus rule matches nothing forever while looking like coverage.

## Documentation

| | |
|---|---|
| [Setup and provisioning](docs/setup.md) | Wi-Fi over serial, broker discovery, getting back to setup |
| [Configuration](docs/configuration.md) | Every setting, the serial console, tuning for your environment |
| [How detection works](docs/detection.md) | Scoring, signal weights, editing the rules |
| [Vendor fingerprints](docs/fingerprints.md) | Per-vendor signatures, graded by confidence |
| [MQTT and signage](docs/signage-integration.md) | Topics, payloads, subscriber examples |
| [Limitations in full](docs/limitations.md) | Every failure mode, stated plainly |
| [Testing](docs/testing.md) | What was verified on hardware, and what wasn't |
| [Building from source](docs/building.md) | Hardware, toolchain, flashing locally |

## Hardware

An ESP32 dev board and a USB cable. A piezo buzzer on GPIO25 is optional; the
onboard LED on GPIO2 needs no wiring.

## Ethics

A BLE scan sees every advertiser nearby, not just eyewear. Logging your own
space is a different thing from profiling other people's devices, and this tool
makes no distinction — you do.

Recording indicators on these products are separate from the radio, so presence
never implies a camera is on.

MIT licensed.
