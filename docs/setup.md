# Setup and provisioning

How a device gets its Wi-Fi and finds a broker, and how to get back to setup when things change.

## Setting it up without a toolchain

Wi-Fi settings arrive over **Improv Wi-Fi** on the USB serial link — the same
connection used to flash the device — so a non-technical user never installs a
toolchain or edits a file.

1. Plug the canary into a computer with a USB **data** cable (charge-only
   cables power the board but carry no data — this cost us an hour).
2. Open the [installer page](https://paulloth1.github.io/esp32-smart-glass-canary/)
   in Chrome or Edge and click Install. WebSerial is Chromium-only, so Firefox,
   Safari and phones cannot flash.
3. The same page then asks for your Wi-Fi name and password and sends them
   down the serial link.
4. The device saves them to NVS, connects, and reports its own address back.

Flashing **erases stored Wi-Fi credentials**: the distributed image spans
`0x0`–`0x13a990`, which covers the NVS region at `0x9000`. So every update also
means re-provisioning — a few seconds in the same browser flow, and it does
guarantee the settings match the firmware that is running.

Settings live in NVS, not in the binary. That is the point: **a firmware image
built this way contains no credentials**, so the same file can be handed to
anyone. `secrets.h` still works and acts as the default when NVS is empty, so
the build-and-flash workflow above is unchanged for you.

There is deliberately **no setup access point**. A SoftAP would mean
broadcasting an open network anyone in range could configure, contending for
the single 2.4GHz radio the BLE scanner needs — a full-duty scan starves
association badly enough that clients see the network but cannot join it — and
carrying a web server and DNS responder for a job the serial link already does.
Since reconfiguring means plugging into a computer anyway, it is also a natural
moment to reflash to the current version.

### Getting back to setup

Credentials outlive the network they were for. Two ways to clear them:

| Route | How |
|---|---|
| Hardware | Hold **BOOT** while powering on, keep holding ~3s |
| Serial | Press `p` in `./monitor.sh` |

Both wipe stored settings and leave the device waiting for Improv. It keeps
detecting glasses and driving the LED and buzzer the whole time — only the
network side is idle.

### Finding a broker without configuring one

Improv carries only Wi-Fi credentials, so a device flashed from the installer
has no broker address. When `MQTT_HOST` is empty the firmware discovers a
broker advertised on the LAN as `_mqtt._tcp`, retrying every 60 seconds. A
configured host always wins.

Mosquitto does not advertise itself — that needs a small Avahi service file on
the broker host, and the caveats (first responder wins, mDNS is
unauthenticated) are in [docs/signage-integration.md](docs/signage-integration.md).

### Security trade-offs, stated plainly

- **NVS is not encrypted.** Someone with physical access to the board can read
  the stored Wi-Fi password out of flash. Normal for consumer IoT, and worth
  knowing rather than discovering.
- **Provisioning requires physical access** to the USB port, which is a
  meaningfully smaller attack surface than an open access point.
