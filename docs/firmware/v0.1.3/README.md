# VoxStick firmware v0.1.3

This directory is served by GitHub Pages for the browser-based installer.

| File | Purpose | SHA-256 |
|---|---|---|
| `voxstick-full.bin` | Merged ESP32-S3 image for one-shot flashing at `0x0` | `954851e1e12a0cf15079ca2dd842a176d911570c7b74f63415f10d63cc202cf5` |
| `manifest.json` | ESP Web Tools manifest consumed by `docs/install.html` | - |

This release changes the StickS3 LCD from a bright full-screen ready fill to a
low-power status display: dim PWM backlight, black background, open/muted mic
icon, and a tiny audio level meter. IMU flat-detect mute is reflected in the
same muted icon as host-side UAC mute.
