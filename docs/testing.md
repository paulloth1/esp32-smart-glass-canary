# Testing

How this was verified on real hardware, and what remains unverified.

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
