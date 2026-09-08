# How detection works

Why detection is scored rather than binary, and what each signal is worth.

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
