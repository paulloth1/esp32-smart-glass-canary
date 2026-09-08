# Smart Glasses BLE Fingerprint Table

Research compiled 2026-09-07 for the ESP32 "canary" defensive privacy project.

## How to read this file

Every row carries a confidence tag:

| Tag | Meaning |
|---|---|
| **CONFIRMED** | Found in a primary/authoritative source (IEEE registry, Bluetooth SIG registry, vendor documentation). Safe to compile. |
| **REPORTED** | Secondary source — blog, forum, third-party app source code, press coverage. Plausible, not independently verified. |
| **UNVERIFIED** | Could not be substantiated, or the only source available is suspect. **Do not compile as a positive match.** |
| **NOT FOUND** | Actively searched, nothing public located. This is a real result, not a gap in effort. |

> **Read the "Detection reliability caveats" section at the end before writing any firmware.** Several of the strongest-looking signals in this table are unusable in the field for reasons that have nothing to do with whether the values are correct.

---

## 0. Authoritative source snapshot

Fetched and grepped locally during this research:

| Source | URL | Notes |
|---|---|---|
| IEEE MA-L registry (24-bit OUI) | https://standards-oui.ieee.org/oui/oui.csv | 40,090 records |
| IEEE MA-M registry (28-bit) | https://standards-oui.ieee.org/oui28/mam.csv | 6,580 records |
| IEEE MA-S registry (36-bit) | https://standards-oui.ieee.org/oui36/oui36.csv | 7,173 records |
| Bluetooth SIG member 16-bit UUIDs | https://bitbucket.org/bluetooth-SIG/public/raw/main/assigned_numbers/uuids/member_uuids.yaml | 713 records — **official SIG registry** |
| Bluetooth SIG company IDs (Nordic mirror) | https://raw.githubusercontent.com/NordicSemiconductor/bluetooth-numbers-database/master/v1/company_ids.json | |

Note: the Nordic mirror does **not** carry a `member_uuids.json`. The 16-bit member UUID assignments below come from the SIG's own Bitbucket, which is the upstream authority.

---

## 1. Company IDs (manufacturer-specific data, first 2 bytes little-endian)

Your pre-established list is confirmed correct against the Nordic mirror. Additions found during this research:

| Company ID | Company | Relevance | Confidence | Source |
|---|---|---|---|---|
| `0x058E` | Meta Platforms Technologies, LLC | Meta glasses | **CONFIRMED** | Nordic mirror (pre-established) |
| `0x01AB` | Meta Platforms, Inc. | Meta glasses | **CONFIRMED** | Nordic mirror (pre-established) |
| `0x03C2` | Snapchat Inc | Spectacles | **CONFIRMED** | Nordic mirror (pre-established) |
| **`0x0D53`** | **Luxottica Group S.p.A** | **New find — Ray-Ban/Oakley frames** | **CONFIRMED** | Nordic mirror, code 3411 |
| `0x0171` | Amazon.com Services LLC | Echo Frames | **CONFIRMED** | Nordic mirror (pre-established) |
| `0x060C` | Vuzix Corporation | Blade/Shield | **CONFIRMED** | Nordic mirror (pre-established) |
| `0x10F9` | Even Realities Ltd. | G1/G2 | **CONFIRMED** | Nordic mirror (pre-established) |
| `0x009E` | Bose Corporation | Bose Frames | **CONFIRMED** | Nordic mirror (pre-established) |
| `0x0BC6` | TCL COMMUNICATION EQUIPMENT CO.,LTD. | RayNeo | **CONFIRMED** | Nordic mirror (pre-established) |
| `0x05D6` | Zhuhai Jieli technology Co.,Ltd | Chipset vendor in several cheap camera glasses | **CONFIRMED** | Nordic mirror, code 1494 |

### `0x0D53` — important note

The `yj_nearbyglasses` project's `smart_glasses_identifiers.csv` labels `0x0D53` as **"Snap"**. **This is an error in that CSV.** `0x0D53` = 3411 = **Luxottica Group S.p.A**. The same project's actual source code names the constant correctly (`ESSILOR_COMPANY_ID` / `essilorCompanyID`), with the in-code comment *"EssilorLuxottica - needs more verification, but OAKLEY and some newer Meta models likely have that"*. Treat `0x0D53` as a **Luxottica/EssilorLuxottica** signal (i.e. Ray-Ban and Oakley frames), **not** Snap.
Source: https://github.com/yjeanrenaud/yj_nearbyglasses/blob/main/Android/src/main/java/ch/pocketpc/nearbyglasses/model/DetectionEvent.kt

### Company IDs actively searched and NOT FOUND

Grepped the full company ID database (`xreal`, `nreal`, `rokid`, `brilliant`, `solos`, `engo`, `halliday`, `inmo`):

| Vendor | Result |
|---|---|
| Xreal / Nreal | **NOT FOUND** — no SIG company ID |
| Rokid | **NOT FOUND** |
| Brilliant Labs | **NOT FOUND** (only "Brilliant Home Technology, Inc." `0x0820`, unrelated smart-home vendor — **do not use**) |
| Solos | **NOT FOUND** |
| Engo | **NOT FOUND** |
| Halliday | **NOT FOUND** |
| INMO | **NOT FOUND** |

These vendors do not own a company ID, so their manufacturer data will carry either their silicon vendor's ID (commonly Nordic `0x0059`, Realtek, Jieli `0x05D6`) or no manufacturer data at all. **Company-ID matching cannot detect them.**

---

## 2. 16-bit member service UUIDs (Bluetooth SIG registry)

All **CONFIRMED** from https://bitbucket.org/bluetooth-SIG/public/raw/main/assigned_numbers/uuids/member_uuids.yaml

| UUID | Assigned to | Relevance |
|---|---|---|
| **`0xFD5F`** | **Meta Platforms Technologies, LLC** | Highest-value Meta signal |
| `0xFEB7` | Meta Platforms, Inc. | |
| `0xFEB8` | Meta Platforms, Inc. | |
| `0xFE45` | Snapchat Inc | Spectacles |
| `0xFD41` | Amazon Lab126 | Echo Frames hardware org |
| `0xFE00`, `0xFE03`, `0xFE15` | Amazon.com Services, Inc. | |
| `0xFCDC` | Amazon.com Services, LLC | |
| `0xFC8F`, `0xFDD2`, `0xFE21`, `0xFEBE` | Bose Corporation | `0xFDD2` = Bose AR (see §9) |

No member UUID is registered to Vuzix, Luxottica, Even Realities, Xreal, Rokid, TCL, or Microoled. **NOT FOUND.**

Expanded 128-bit form for `0xFD5F`: `0000fd5f-0000-1000-8000-00805f9b34fb`

---

## 3. Meta — Ray-Ban (Gen 1/2), Oakley Meta HSTN, Meta Vanguard, Ray-Ban Display

| Signal | Value | Confidence | Source |
|---|---|---|---|
| Company ID | `0x058E` Meta Platforms Technologies | **CONFIRMED** | SIG registry |
| Company ID | `0x01AB` Meta Platforms, Inc. | **CONFIRMED** | SIG registry |
| Company ID | `0x0D53` Luxottica Group | **CONFIRMED** (as an ID) / **REPORTED** (as appearing on these glasses) | SIG registry; attribution to glasses per yj_nearbyglasses in-code comment |
| Service UUID | `0xFD5F` | **CONFIRMED** (assignment) / **REPORTED** (observed in Ray-Ban adverts) | SIG registry; https://github.com/NullPxl/banrays |
| Manufacturer data sample | `020102102716e4` | **REPORTED** | https://github.com/NullPxl/banrays |
| Manufacturer data ASCII `META_RB_GLASS` | `4D 45 54 41 5F 52 42 5F 47 4C 41 53 53` | **UNVERIFIED — DO NOT COMPILE** | see below |
| BT Classic name | `Ray-Ban Meta…` in phone Bluetooth list | **REPORTED** | https://support.bemyeyes.com/hc/en-us/articles/29893014835729-Meta-AI-Glasses-FAQ |
| BT Classic name (call audio picker) | `RB Meta` | **REPORTED** | as above |
| BT Classic name (Gen 1, 2021) | `Ray-Ban Stories` | **REPORTED** | https://en.wikipedia.org/wiki/Ray-Ban_Meta |
| GATT services | `0x180A` Device Information, `0x180F` Battery — standard only, rest undocumented | **REPORTED** | https://github.com/lingster/meta-rayban-bluetooth |
| Address type | Random / rotating — OUI matching stated not useful | **REPORTED** | https://github.com/NullPxl/banrays |
| Advertises while paired+connected? | **No** — see caveats §12 | **REPORTED** | https://cybernews.com/security/android-app-detects-nearby-meta-snap-smart-glasses/ |

### ⚠️ `META_RB_GLASS` is almost certainly fabricated — do not use it

This string circulates as a Ray-Ban Meta manufacturer-data payload. It traces to a single sample advertising frame in the `yj_nearbyglasses` README, introduced with the words *"this is what BLE advertising frames look like"* — i.e. presented as an **illustration**, not a capture. I checked its internal consistency and it fails on four counts:

1. **Declared length is wrong.** The frame says `Length: 0x1A` (26). The actual AD structure is 1 type byte + 2 company ID bytes + 13 payload bytes = **16 = 0x10**. A real sniffer would never print this.
2. **Wrong service UUID.** The frame lists `0xFEAA`, which is **Google LLC** (Eddystone beacon format) — confirmed in the SIG registry. Meta's own assignment is `0xFD5F`.
3. **`BR/EDR Not Supported` flag** is set, on a device whose entire product function includes A2DP/HFP audio playback and calls.
4. **Placeholder-looking MAC** `C4:7C:8D:1E:2B:3F` (`1E:2B:3F` is a textbook sequence).

Compiling `META_RB_GLASS` into firmware would produce a **permanent silent false negative**. Excluded.

### Ray-Ban Display / Oakley Meta HSTN / Meta Vanguard

No model-specific BLE fingerprints published. **NOT FOUND.** These are recent SKUs; expect them to share the `0x058E` / `0xFD5F` Meta signals, but no byte pattern distinguishing model is documented. Do not guess discriminator bytes.

---

## 4. Snap Spectacles (all gens)

| Signal | Value | Confidence | Source |
|---|---|---|---|
| Company ID | `0x03C2` Snapchat Inc | **CONFIRMED** | SIG registry |
| Member service UUID | `0xFE45` Snapchat Inc | **CONFIRMED** | SIG registry |
| Advertised/Classic name | `Spectacles` (default) | **REPORTED** | https://support.spectacles.com/hc/en-us/articles/360000407246-Pairing-Your-Spectacles |
| Name is **user-customisable** | Users may rename at pairing; the chosen name is what appears in the Bluetooth list | **REPORTED** | as above |
| Address type | Not documented | **NOT FOUND** | |
| OUI | `98:A4:0E` — "Snap, Inc." | **CONFIRMED** (registration) / **UNVERIFIED** (that glasses use it over the air) | IEEE MA-L |

**Name matching on `Spectacles` is unreliable** because the name is user-editable. Company ID `0x03C2` is the durable signal.

---

## 5. Amazon Echo Frames (all gens)

| Signal | Value | Confidence | Source |
|---|---|---|---|
| Company ID | `0x0171` Amazon.com Services LLC | **CONFIRMED** | SIG registry |
| Member UUIDs | `0xFD41` (Amazon Lab126), `0xFE00`, `0xFE03`, `0xFE15`, `0xFCDC` | **CONFIRMED** (assignment) / **UNVERIFIED** (which one Echo Frames advertise) | SIG registry |
| Advertised BLE name | Not documented; Amazon docs only say "select the name of your Echo Frames" | **NOT FOUND** | https://www.amazon.com/gp/help/customer/display.html?nodeId=G2BV97LR4N8Y6ZG5 |
| Bluetooth Classic | **Yes** — explicitly supported as a Bluetooth headset (HFP/A2DP) | **CONFIRMED** | https://www.amazon.com/gp/help/customer/display.html?nodeId=GSYNNGDQXM7QF7N2 |
| Address type | Not documented | **NOT FOUND** | |

**Note:** Echo Frames have **no camera**. For a camera-warning canary they are arguably out of scope, or should be a distinct lower-severity class. Also, `0x0171` is used across Amazon's entire device fleet (Echo speakers, Fire TV, Kindle, tags) — matching it alone will produce heavy false positives in any home or public space.

---

## 6. Xreal (Air / Air 2 / One) — **not BLE-detectable**

| Signal | Value | Confidence | Source |
|---|---|---|---|
| SIG company ID | none registered | **NOT FOUND** | company_ids.json grep |
| IEEE MA-M block | `FC:D2:B6:A` — NREAL TECHNOLOGY LIMITED (28-bit) | **CONFIRMED** | IEEE MA-M registry |
| Transport | **USB** — driver uses `hidapi` / `rusb`, no Bluetooth code path | **CONFIRMED** | https://github.com/badicsalex/ar-drivers-rs (`src/nreal_air.rs`, `src/nreal_light.rs`) |
| USB VID | `0x3318` (Nreal) | **CONFIRMED** | ar-drivers-rs `src/nreal_air.rs` |
| USB PIDs | Air `0x0424`, Air 2 `0x0428`, Air 2 Pro `0x0432`, Air 2 Ultra `0x0426` | **CONFIRMED** | as above |

**Conclusion: Xreal Air/Air 2/One are USB-C tethered display glasses.** The open-source driver stack talks to them purely over USB HID; there is no Bluetooth transport. They also have **no user-facing camera** on Air/Air 2/One. **A BLE canary cannot detect these, and should not claim to.**

### ⚠️ The `FC:D2:B6` OUI is shared — do not match on 24 bits

Nreal holds `FC:D2:B6:A`, a **28-bit MA-M block**. The 24-bit prefix `FC:D2:B6` is administered by the IEEE and split among 16 unrelated companies, including Univer S.p.A., Soma GmbH, Cirque Audio, and CG Power. Matching only `FC:D2:B6` gives **15/16 false positives**. If you use it at all, match the full 28 bits (first nibble of byte 4 == `A`).

---

## 7. Rokid (Max / Glasses)

| Signal | Value | Confidence | Source |
|---|---|---|---|
| SIG company ID | none registered | **NOT FOUND** | company_ids.json grep |
| IEEE OUI | none registered under "Rokid" in MA-L/MA-M/MA-S | **NOT FOUND** | all 3 IEEE registries |
| BLE scan filter UUID | `00009100-0000-1000-8000-00805f9b34fb` (16-bit `0x9100`) | **REPORTED** | https://marcinmiazga.com/rokid-cxrm-upgrade |
| Architecture | BLE used for **discovery only**; Bluetooth **Classic** socket carries the data | **REPORTED** | as above |
| Advertised name | not documented | **NOT FOUND** | |

`0x9100` is **not** a SIG-assigned 16-bit UUID — it is a vendor-chosen value in unassigned space. That makes it distinctive (low collision odds in practice) but entirely undocumented by the SIG, so it could change between firmware versions or product lines. Treat as REPORTED, single-source.

Rokid **Max** is a tethered display device; **Rokid Glasses** (2025) is the camera-equipped model. The UUID above comes from CXR-M development notes and may not apply to all models.

---

## 8. RayNeo (X2 / Air), Vuzix (Blade / Shield)

| Vendor | Signal | Value | Confidence | Source |
|---|---|---|---|---|
| RayNeo | Company ID | `0x0BC6` TCL Communication Equipment | **CONFIRMED** (assignment) / **UNVERIFIED** (used by RayNeo glasses) | SIG registry |
| RayNeo | IEEE MA-M | `00:6A:5E:5` — Rayneo (wuxi) ltd (28-bit) | **CONFIRMED** | IEEE MA-M registry |
| RayNeo | Pairing name | `rayneo X2` | **REPORTED** | https://www.rayneo.com/pages/faq-x2 |
| Vuzix | Company ID | `0x060C` Vuzix Corporation | **CONFIRMED** | SIG registry |
| Vuzix | IEEE OUI | `98:DA:92` Vuzix Corporation; `60:99:D1` "Vuzix / Lenovo" | **CONFIRMED** | IEEE MA-L |
| Vuzix | Advertised name / service UUIDs | no public documentation located | **NOT FOUND** | searched vendor docs + SDK |

`00:6A:5E` is again a **shared 24-bit MA-M prefix** (Hilti, Alstom, Annapurna Labs and 12 others). Match the full 28 bits.

Vuzix `98:DA:92` and `60:99:D1` are full MA-L blocks and are genuinely Vuzix-exclusive — but see §12 on whether an OUI is ever visible.

---

## 9. Even Realities (G1 / G2), Brilliant Labs (Frame / Halo), Bose Frames

| Vendor | Signal | Value | Confidence | Source |
|---|---|---|---|---|
| Even Realities | Company ID | `0x10F9` Even Realities Ltd. | **CONFIRMED** | SIG registry |
| Even Realities | IEEE OUI | none in any IEEE registry | **NOT FOUND** | all 3 registries |
| Even Realities | Transport | Nordic UART Service; each arm is a **separate BLE connection** (L and R) | **REPORTED** | https://github.com/AGiXT/mobile/blob/main/Even%20Realities%20G1%20BLE%20Protocol.txt |
| Even Realities | Characteristic | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` (Nordic UART TX) | **REPORTED** | as above |
| Even Realities | Serial format | `S110LAAL103842` — `S100`=Round, `S110`=Square; `AA`=Grey, `BB`=Brown, `CC`=Green | **REPORTED** | as above |
| Even Realities | Advertised name | left/right arm naming convention widely referenced but **exact string not confirmed** | **UNVERIFIED** | |
| Even Realities | Keepalive | disconnects after 32 s without heartbeat (send every 28–30 s) | **REPORTED** | as above |
| Brilliant Labs | Service UUID | `7A230001-5475-A6A4-654C-8431F6AD49C4` | **CONFIRMED** | https://docs.brilliant.xyz/frame/frame-sdk-bluetooth-specs/ |
| Brilliant Labs | TX characteristic | `7A230002-5475-A6A4-654C-8431F6AD49C4` | **CONFIRMED** | as above |
| Brilliant Labs | RX characteristic | `7A230003-5475-A6A4-654C-8431F6AD49C4` | **CONFIRMED** | as above |
| Brilliant Labs | Bootloader name | `Frame Update` | **CONFIRMED** | as above |
| Brilliant Labs | Normal advertised name | not stated in vendor docs | **NOT FOUND** | as above |
| Brilliant Labs | Bonding | "Frame uses BLE bonding and must pair with a host device before any communication can take place" | **CONFIRMED** | as above |
| Bose | Company ID | `0x009E` Bose Corporation | **CONFIRMED** | SIG registry |
| Bose | Bose AR service UUID | `0000fdd2-0000-1000-8000-00805f9b34fb` (`0xFDD2`) | **CONFIRMED** (SIG assignment) / **REPORTED** (as the Bose AR service) | SIG registry; https://zakaton.github.io/Bose-Frames-Web-SDK/default.html |
| Bose | Dual-radio design | BLE for motion sensor, **Bluetooth Classic** separately for audio — must connect twice | **REPORTED** | as above |
| Bose | IEEE OUIs | `C8:7B:23`, `2C:41:A1`, `08:DF:1F`, `AC:BF:71`, `04:52:C7`, `4C:87:5D`, `48:22:1D`, `78:2B:64`, `28:11:A5`, `E4:58:BC`, `68:F2:1F`, `BC:87:FA`, `60:AB:D2`, `00:0C:8A` | **CONFIRMED** | IEEE MA-L |

**Brilliant Labs Frame is the single best-documented device in this table** — the service UUID is a proprietary 128-bit value published by the vendor, which makes it both highly specific and safe to match. Brilliant Labs **Halo**: **NOT FOUND**, no published BLE specs.

**Bose Frames have no camera** and the product line is discontinued. `0x009E` will also match every Bose headphone in range — very high false-positive rate.

---

## 10. Solos, Engo, Halliday, INMO

| Vendor | Company ID | IEEE | Names / UUIDs |
|---|---|---|---|
| Solos | **NOT FOUND** | `E8:78:29:E` — Solos Technology Limited (MA-M, 28-bit) — **CONFIRMED** | **NOT FOUND** |
| Engo | **NOT FOUND** | **NOT FOUND** (Engo/ActiveLook — Microoled holds company ID `0x08F2`) | **NOT FOUND** |
| Halliday | **NOT FOUND** | `38:05:25:D` — Halliday Holdings PTE. LTD. (MA-M, 28-bit) — **CONFIRMED** | **NOT FOUND** |
| INMO | **NOT FOUND** | **NOT FOUND** | **NOT FOUND** |

Both MA-M prefixes above are **shared 24-bit blocks** (`E8:78:29` also holds FairPhone, METZ CONNECT, Annapurna Labs; `38:05:25` also holds Visitech, Robotize, Annapurna Labs). Match 28 bits or not at all.

Engo eyewear uses the **ActiveLook** platform from **Microoled** (company ID `0x08F2`, already in your list) — that is the most likely manufacturer ID to appear, but I found no capture confirming it. **UNVERIFIED.**

---

## 11. IEEE OUI reference (full MA-L blocks, 24-bit — vendor-exclusive)

All **CONFIRMED** from https://standards-oui.ieee.org/oui/oui.csv

**Meta Platforms, Inc.** (13 blocks):
`48:05:60`, `CC:A1:74`, `C0:DD:8A`, `D0:B3:C2`, `88:25:08`, `94:F9:29`, `D4:D6:59`, `78:C4:FA`, `B4:17:A8`, `50:99:03`, `80:F3:EF`, `84:57:F7`, `F4:4E:35`

**Facebook Inc:** `48:57:DD`, `A4:0E:2B`
**Oculus VR, LLC:** `2C:26:17`
**Snap, Inc.:** `98:A4:0E`
**Vuzix:** `98:DA:92`, `60:99:D1` (Vuzix / Lenovo)
**Luxottica Group S.P.A.:** `98:59:49` · **Luxottica Tristar (Dongguan) Optical:** `80:AA:1C`, `38:47:12`
**Magic Leap, Inc.:** `60:4B:AA`
**Amazon Technologies Inc.:** 100+ blocks — too many to be a useful discriminator; see §12.

Note there is **no** MA-L block registered to "Meta Platforms Technologies" — the Reality Labs hardware org inherits the Facebook/Oculus blocks above.

---

## 12. Detection reliability caveats

This is the section that matters most. **Read it before trusting anything above.**

### 12.1 The paired-and-connected blind spot — the single biggest limitation

**Meta Ray-Ban glasses stop emitting BLE advertising packets entirely once they are connected to their phone.** Reporting on the `Nearby Glasses` app states that Ray-Ban Meta Gen 2 "stops broadcasting advertisement packets entirely after connecting to a phone, making the device invisible not only to Nearby Glasses but to general-purpose Bluetooth scanners as well."
Source: https://cybernews.com/security/android-app-detects-nearby-meta-snap-smart-glasses/

The `banrays` author reports the same from hands-on testing: *"I have only been able to detect BLE traffic during 1) pairing 2) powering-on"* and cannot detect the glasses *"during usage when they're communicating with the paired phone."*
Source: https://github.com/NullPxl/banrays

**Implication: the canary detects glasses in exactly the states where they are least threatening** — powering on, in pairing mode, or freshly out of the case — and goes blind during **the actual recording session**, which is the threat you built it for. This is not a bug you can fix in firmware. It is a property of how the glasses behave.

Partial mitigations, none complete:
- Advertise-on-wake is sometimes visible when glasses come out of the case while already powered on (the `banrays` author reports this happens, *"but not consistently"*).
- Bluetooth **Classic** inquiry scanning is a separate channel and is not subject to the BLE advertising stop. Most of these devices (Meta, Echo Frames, Bose, Rokid, RayNeo) run Classic A2DP/HFP. However, Classic devices are only discoverable when explicitly in discoverable mode, which is likewise mostly during pairing. ESP32 (classic-capable variants only — **not** ESP32-C3/C6/H2) can do this; the ESP32-S3 has no Classic radio.
- Detecting the **phone side** or Wi-Fi Direct/AP link used for photo transfer is a different research direction not covered here.

### 12.2 Address randomisation makes OUI matching largely useless

The `banrays` author states Meta glasses use randomised addresses and that *"even though IEEE assigns certain MAC address prefixes (OUI...), the randomization means this doesn't appear to be useful for detection."* The `yj_nearbyglasses` README concurs: *"Because BLE uses randomised MAC and the OSSID are not stable, nor the UUID of the service announcements, you can't just scan for the bluetooth beacons."* Its sample frame shows an address typed **`(Random Static)`**.

Under the Bluetooth Core Spec, a device using a Resolvable Private Address (RPA) sets the top two bits of the address to `01` and rotates it (typically every 15 minutes). **A random address contains no OUI at all** — the "vendor prefix" is random bits. Consequently:

- **The entire OUI table in §11 is reference material, not a detection primitive.** It is useful for identifying a device you have already captured with a public address, and for Bluetooth Classic (which always uses the public BD_ADDR). It is close to useless for passive BLE scanning of modern glasses.
- Notably, **neither of the two real-world glasses detectors I examined does any OUI matching at all.** `yj_nearbyglasses` matches only company IDs and name substrings. That is a deliberate choice by people who tested against real hardware.
- If you compile OUIs anyway, treat any hit as high-confidence-but-rare, and never treat an OUI miss as evidence of absence.

### 12.3 Shared 24-bit prefixes in the MA-M registry

Xreal, Solos, Halliday and RayNeo hold **28-bit MA-M blocks**, not full OUIs. Their 24-bit prefixes are shared with 15 other companies each. Matching 24 bits gives a ~94% false-positive rate on any hit. Always compare the full 28 bits.

### 12.4 Company ID false positives

- `0x0171` (Amazon) covers Echo speakers, Fire TV, Kindles, Tile-style trackers — **not** just glasses.
- `0x009E` (Bose) covers every Bose headphone and speaker.
- `0x05D6` (Zhuhai Jieli) is a **chipset** vendor found in vast numbers of generic BT audio products.
- `0x0BC6` (TCL) covers TCL phones and TVs.
- `0x058E` / `0x01AB` (Meta) also cover Quest headsets and Meta controllers.

Company ID alone is a *vendor* signal, not a *glasses* signal. Consider requiring a company ID **plus** a corroborating signal (service UUID, RSSI proximity threshold, name substring) before raising an alarm, and expose a sensitivity setting.

### 12.5 Name-based matching is fragile

- Snap Spectacles names are **user-editable** at pairing.
- The `Local Name` AD field is frequently carried in `SCAN_RSP` rather than `ADV_IND`, so a **passive** scanner will never see it. The ESP32 must run an **active** scan (which transmits `SCAN_REQ` — note this makes your canary itself radio-visible, a counter-surveillance consideration in its own right).
- Names typically disappear once the device is connected, per §12.1.

### 12.6 Coverage honesty

Of the 13 vendor families requested:
- **Well-fingerprinted:** Meta, Snap, Brilliant Labs Frame, Bose, Even Realities, Vuzix (IDs only).
- **Partially:** Rokid (one single-source UUID), RayNeo, Amazon.
- **Effectively undetectable via BLE:** Xreal Air/Air 2/One (USB-only, no BLE radio in the control path), INMO, Engo, Solos, Halliday (no IDs, no published sniffs).
- **No public data at all:** Meta Ray-Ban Display / Oakley Meta HSTN / Meta Vanguard model discriminators, Brilliant Labs Halo.

### 12.7 One value was excluded as fabricated

`META_RB_GLASS` (`4D 45 54 41 5F 52 42 5F 47 4C 41 53 53`) circulates online as a Ray-Ban Meta manufacturer-data payload. Analysis in §3 shows the sole source frame is internally inconsistent (wrong AD length, Google's `0xFEAA` beacon UUID instead of Meta's `0xFD5F`, `BR/EDR Not Supported` on an audio device) and is labelled by its author as an illustration. **It is not in this table as a match target.** If you have seen this string elsewhere and were planning to use it — don't.

### 12.8 Verify before you trust

Every REPORTED row should be confirmed against real hardware with `nRF Connect` (mobile) or `bluetoothctl` / `btmon` on Linux before it gates an alert. The CONFIRMED registry rows (company IDs, member UUIDs, Brilliant Labs' vendor-published UUIDs) are safe as *values*; what remains unproven for most is *whether that value actually appears over the air on that product*.

---

## Appendix: recommended firmware match priority

Ordered by specificity (most specific first), based on the above:

1. `7A230001-5475-A6A4-654C-8431F6AD49C4` → Brilliant Labs Frame (vendor-published, unambiguous)
2. Service UUID `0xFD5F` → Meta
3. Service UUID `0xFE45` → Snap · `0xFDD2` → Bose AR
4. Company ID `0x058E` / `0x01AB` → Meta · `0x03C2` → Snap · `0x0D53` → Luxottica · `0x10F9` → Even Realities · `0x060C` → Vuzix
5. UUID `0x9100` → Rokid (single-source, verify)
6. Name substrings `rayban` / `ray-ban` / `ray ban` / `spectacles` / `rayneo` / `frame update` (active scan required)
7. Company ID `0x0171` / `0x009E` / `0x05D6` / `0x0BC6` → **low confidence, high false positive, gate behind a sensitivity setting**
8. OUI matching → reference only; see §12.2
