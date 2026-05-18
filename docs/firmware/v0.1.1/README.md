# VoxStick firmware v0.1.1

This directory is served by GitHub Pages for the browser-based installer.

| File | Purpose | SHA-256 |
|---|---|---|
| `voxstick-full.bin` | Merged ESP32-S3 image for one-shot flashing at `0x0` | `4c6c9b238ba7a336d70332b24fd8ba0923b89871d56e2fafa1f4a7ffe6500674` |
| `manifest.json` | ESP Web Tools manifest consumed by `docs/install.html` | - |

This build fixes the M5PM1 L3B rail control polarity for StickS3. L3B powers
the LCD/MIC/SPK rail; on StickS3 PYG2 must be driven low to enable it.
