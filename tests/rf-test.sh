#!/usr/bin/env bash
#
# rf-test.sh - end-to-end RF test of the Smart Glass Canary.
#
# Uses a local BlueZ Bluetooth adapter to impersonate smart glasses over the
# air, and asserts that the canary reacts the way the scoring rules say it
# should -- including one case that must deliberately NOT alert.
#
# Requires: a Bluetooth adapter (bluetoothctl), the canary on /dev/ttyUSB0,
#           esptool (used only to reset the board between cases so the
#           re-alert cooldown cannot mask a result).
#
# Usage:  ./tests/rf-test.sh [--full]
#           --full  also run case E, the all-clear timeout (~90s extra)
#
set -uo pipefail

PORT="${PORT:-/dev/ttyUSB0}"
ESPTOOL="${ESPTOOL:-$HOME/.local/bin/esptool}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PASS=0; FAIL=0

need() { command -v "$1" >/dev/null || { echo "ERROR: $1 not found" >&2; exit 1; }; }
need bluetoothctl; need stty

[ -e "$PORT" ] || { echo "ERROR: $PORT missing - is the canary plugged in?" >&2; exit 1; }
if ! bluetoothctl list 2>/dev/null | grep -q Controller; then
    echo "ERROR: no Bluetooth controller. Plug in an adapter and check:" >&2
    echo "       systemctl is-active bluetooth ; bluetoothctl list" >&2
    exit 1
fi

reset_board() {
    [ -x "$ESPTOOL" ] && "$ESPTOOL" --port "$PORT" --after hard-reset --no-stub chip-id >/dev/null 2>&1
    sleep 3
}

# adapter's own BLE address, so we only assert on adverts that are ours
ADAPTER_MAC=$(bluetoothctl list 2>/dev/null | awk '/Controller/{print tolower($2); exit}')

# run_case <tag> <expect_alert:yes|no> <expected_why> <advertise-menu commands>
run_case() {
    local tag="$1" expect="$2" why="$3" cmds="$4"
    local log="$WORK/$tag.log"

    reset_board
    stty -F "$PORT" 115200 raw -echo
    timeout 26 cat "$PORT" > "$log" &
    local CAT=$!
    sleep 4
    printf 'v' > "$PORT"          # verbose, so we see the score of every advert
    sleep 1
    { printf 'menu advertise\n%s\nback\nadvertise on\n' "$cmds"; sleep 18; } \
        | timeout 22 bluetoothctl > "$WORK/bt_$tag.log" 2>&1 &
    local BT=$!
    wait $CAT
    kill $BT 2>/dev/null; wait 2>/dev/null

    # BlueZ reports "Advertising object registered" as soon as it accepts the
    # D-Bus object, which is NOT the same as the radio transmitting. When the
    # controller never activates the instance, nothing goes on air and the
    # canary correctly reports nothing -- a false negative that looks exactly
    # like a broken detector. Insist on seeing ActiveInstances go non-zero.
    if ! grep -q "ActiveInstances: 0x0[1-9]" "$WORK/bt_$tag.log"; then
        echo "── case $tag ── INVALID: the adapter never actually advertised"
        echo "   (BlueZ registered the advertisement but ActiveInstances stayed 0,"
        echo "    so this says nothing about the canary. Retry, or restart bluetooth.)"
        FAIL=$((FAIL+1))
        echo
        return
    fi

    local scored alerts
    scored=$(grep '"adv"' "$log" | grep -F "$ADAPTER_MAC" | tail -1)
    alerts=$(grep -c glasses_detected "$log")

    echo "── case $tag ── expect: ${expect} alert, why=\"$why\""
    echo "   scored: ${scored:-<our adapter never seen>}"
    echo "   alerts: $alerts"

    local ok=1
    [ -n "$scored" ] || ok=0
    echo "$scored" | grep -q "\"why\":\"$why\"" || ok=0
    if [ "$expect" = yes ]; then [ "$alerts" -ge 1 ] || ok=0; else [ "$alerts" -eq 0 ] || ok=0; fi

    if [ "$ok" = 1 ]; then echo "   PASS"; PASS=$((PASS+1)); else echo "   FAIL"; FAIL=$((FAIL+1)); fi
    echo
}

echo "Smart Glass Canary - RF test"
echo "adapter $ADAPTER_MAC -> canary on $PORT"
echo

# A: a decisive product name on its own. 70 >= 60, alerts.
run_case A yes name 'name Ray-Ban'

# B: Meta's company ID and nothing else. 55 < 60 -- this is the case that
#    proves weak signals stay sub-threshold instead of crying wolf.
run_case B no mfr 'manufacturer 0x058E 0x01 0x02'

# C: weak name "Meta" (35) corroborated by the company ID (55) = 90. Alerts.
run_case C yes name+mfr 'name Meta
manufacturer 0x058E 0x01 0x02'

# D: service UUID 0xFD5F, Meta Platforms Technologies. 70 on its own.
run_case D yes uuid 'uuids 0xFD5F'

if [ "${1:-}" = --full ]; then
    echo "── case E ── detect, then all-clear after DEVICE_TIMEOUT_MS"
    reset_board
    stty -F "$PORT" 115200 raw -echo
    timeout 90 cat "$PORT" > "$WORK/E.log" &
    CAT=$!
    sleep 4
    { printf 'menu advertise\nname Ray-Ban\nback\nadvertise on\n'; sleep 15; } \
        | timeout 19 bluetoothctl > "$WORK/bt_E.log" 2>&1
    wait $CAT
    grep -E 'glasses_detected|"clear"' "$WORK/E.log" | sed 's/^/   /'
    if grep -q glasses_detected "$WORK/E.log" && grep -q '"clear"' "$WORK/E.log"; then
        echo "   PASS"; PASS=$((PASS+1))
    else
        echo "   FAIL (expected a detect followed by an all-clear)"; FAIL=$((FAIL+1))
    fi
    echo
fi

echo "══ $PASS passed, $FAIL failed ══"
[ "$FAIL" -eq 0 ]
