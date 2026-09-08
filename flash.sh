#!/usr/bin/env bash
#
# flash.sh - compile and upload the smart-glass-canary firmware to the ESP32.
#
# Usage:  ./flash.sh [sketch_dir]
#   sketch_dir  optional; defaults to the "firmware" subdir of this project.
#
# Verified hardware (2026-09-07):
#   Chip  : ESP32-D0WD-V3 rev v3.1, 4MB flash, 40MHz crystal
#   Bridge: CH340 (USB 1a86:7523) -- auto bootloader entry works, no need to
#           hold BOOT during upload.
#
set -euo pipefail

# Resolve the project directory from this script's own location, so the repo
# works wherever it is cloned.
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Serial port. Override for a different board or a non-Linux host:
#   PORT=/dev/ttyACM0 ./flash.sh
PORT="${PORT:-/dev/ttyUSB0}"

# arduino-cli: prefer one on PATH, fall back to the common install location.
ARDUINO_CLI="${ARDUINO_CLI:-$(command -v arduino-cli || echo "$HOME/bin/arduino-cli")}"
FQBN=esp32:esp32:esp32

# The default partition layout gives the app only 1.3MB, and the firmware already
# fills 87% of that with just BLE + serial. Enabling the webhook/MQTT outputs in
# config.h pushes it over. huge_app gives the app 3MB of the 4MB flash; the cost
# is no OTA slot, which this project does not use.
PARTITION=huge_app

# 921600 is reliable on this CH340 board. If uploads fail with checksum or
# "Packet content transfer stopped" errors, drop back to 115200 (safe fallback):
#   UPLOAD_BAUD=115200 ./flash.sh
UPLOAD_BAUD="${UPLOAD_BAUD:-921600}"

SKETCH_DIR="${1:-$PROJECT_DIR/firmware}"

if [ ! -x "$ARDUINO_CLI" ]; then
    echo "ERROR: arduino-cli not found at $ARDUINO_CLI" >&2
    exit 1
fi

if [ ! -d "$SKETCH_DIR" ]; then
    echo "ERROR: sketch directory does not exist: $SKETCH_DIR" >&2
    echo "       Pass one explicitly:  $0 /path/to/sketch" >&2
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
    echo "       If the ACL is gone:  sudo usermod -aG dialout \"$USER\"   (then log out and back in)" >&2
    exit 1
fi

echo "==> Compiling $SKETCH_DIR for $FQBN"
"$ARDUINO_CLI" compile \
    --fqbn "$FQBN" \
    --board-options "PartitionScheme=${PARTITION}" \
    "$SKETCH_DIR"

echo "==> Uploading to $PORT at ${UPLOAD_BAUD} baud"
"$ARDUINO_CLI" upload \
    --fqbn "$FQBN" \
    --port "$PORT" \
    --board-options "PartitionScheme=${PARTITION}" \
    --board-options "UploadSpeed=${UPLOAD_BAUD}" \
    "$SKETCH_DIR"

echo "==> Done. Open the serial monitor with:  $PROJECT_DIR/monitor.sh"
