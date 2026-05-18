# VoxStick firmware v0.1.4

This directory is served by GitHub Pages for the browser-based installer.

| File | Purpose | SHA-256 |
|---|---|---|
| `voxstick-full.bin` | Merged ESP32-S3 image for one-shot flashing at `0x0` | `158df526e8889937436c4b1b52e7d3654bbd72271df9e1fb7b7a9317bd3b4f16` |
| `manifest.json` | ESP Web Tools manifest consumed by `docs/install.html` | - |

This release refines the low-power LCD mute state: the muted mic icon is drawn
in light gray and the red slash is thinner, so the screen reads as a muted
microphone instead of just a bright red line.
