#!/usr/bin/env bash
#
# monitor.sh - open a serial monitor on the ESP32 at 115200 baud.
#
# Usage:  ./monitor.sh [baudrate]
#
# Quit with Ctrl-C.
#
set -euo pipefail

PORT="${PORT:-/dev/ttyUSB0}"
ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli || echo "$HOME/bin/arduino-cli")}"
BAUD="${1:-115200}"

if [ ! -x "$ARDUINO_CLI" ]; then
    echo "ERROR: arduino-cli not found at $ARDUINO_CLI" >&2
    exit 1
fi

if [ ! -e "$PORT" ]; then
    echo "ERROR: serial port $PORT is missing." >&2
    echo "       Is the board plugged in? Check with:  ls -l /dev/ttyUSB*  and  lsusb | grep 1a86" >&2
    exit 1
fi

if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
    echo "ERROR: no read/write access to $PORT." >&2
    echo "       Check the ACL:  getfacl $PORT" >&2
    echo "       If the ACL is gone:  sudo usermod -aG dialout paul   (then log out and back in)" >&2
    exit 1
fi

echo "==> Monitoring $PORT at ${BAUD} baud (Ctrl-C to quit)"
exec "$ARDUINO_CLI" monitor -p "$PORT" -c "baudrate=${BAUD}"
