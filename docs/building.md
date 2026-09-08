# Building from source

For changing the firmware. To just use the device, the browser installer needs none of this.

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
