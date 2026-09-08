# What this can and cannot do

The short version is in the [README](../README.md). This is the full picture, including the failure modes that are easy to miss.

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
