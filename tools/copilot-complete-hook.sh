#!/usr/bin/env bash

# VS Code Agent Stop hooks pass event JSON on stdin. The completion reminder
# does not need session content, so discard it without interpreting user data.
cat >/dev/null 2>&1 || true

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source_file="$root/tools/copilot_complete_trigger.c"
binary="${TMPDIR:-/tmp}/voxstick-copilot-complete"

if ! command -v clang >/dev/null 2>&1 ||
   ! command -v pkg-config >/dev/null 2>&1 ||
   ! pkg-config --exists libusb-1.0; then
    exit 0
fi

if [[ ! -x "$binary" || "$source_file" -nt "$binary" ]]; then
    flags="$(pkg-config --cflags --libs libusb-1.0 2>/dev/null)" || exit 0
    # pkg-config emits compiler arguments that intentionally need word splitting.
    # shellcheck disable=SC2086
    clang -Wall -Wextra -O2 "$source_file" $flags -o "$binary" \
        >/dev/null 2>&1 || exit 0
fi

"$binary" >/dev/null 2>&1 || true
exit 0
