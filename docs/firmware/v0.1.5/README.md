# VoxStick firmware v0.1.5

This directory is served by GitHub Pages for the browser-based installer.

| File | Purpose | SHA-256 |
|---|---|---|
| `voxstick-full.bin` | Merged ESP32-S3 image for one-shot flashing at `0x0` | `12f917fb1a8a858e652d2fbc626c872c416acd2b6ec12d343738dcdc814d8af1` |
| `manifest.json` | ESP Web Tools manifest consumed by `docs/install.html` | - |

This binary mirrors the `voxstick-full.bin` asset from the
[`v0.1.5` GitHub release](https://github.com/openbrt/voxstick/releases/tag/v0.1.5).
It updates the BtnA tap shortcut to `Ctrl+F12`, matching the WeType setup docs
for both macOS and Windows. Long-press BtnA still sends `Enter`.
