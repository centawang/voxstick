#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="${TMPDIR:-/tmp}"
hid_bin="$tmp/voxstick-hid-download"
usb_bin="$tmp/voxstick-usb-download"
uac_bin="$tmp/voxstick-uac-download"

clang -Wall -Wextra -O2 \
  "$root/tools/hid_download_trigger.c" \
  -framework IOKit -framework CoreFoundation \
  -o "$hid_bin"

clang -Wall -Wextra -O2 \
  "$root/tools/usb_download_trigger.c" \
  $(pkg-config --cflags --libs libusb-1.0) \
  -o "$usb_bin"

clang -Wall -Wextra -O2 \
  "$root/tools/uac_download_trigger.c" \
  $(pkg-config --cflags --libs libusb-1.0) \
  -o "$uac_bin"

wait_for_download_port() {
  for _ in {1..40}; do
    if compgen -G "/dev/cu.usbmodem*" >/dev/null; then
      ls /dev/cu.usbmodem*
      return 0
    fi
    sleep 0.25
  done
  return 1
}

try_trigger() {
  local label="$1"
  local bin="$2"

  if "$bin"; then
    if wait_for_download_port; then
      echo "$label trigger entered ROM download"
      return 0
    fi
    echo "$label trigger returned success, but no /dev/cu.usbmodem* appeared" >&2
  fi
  return 1
}

try_trigger "IOHID" "$hid_bin" && exit 0
try_trigger "USB vendor/HID" "$usb_bin" && exit 0
try_trigger "UAC" "$uac_bin" && exit 0

echo "failed to enter ROM download from host controls" >&2
exit 1
