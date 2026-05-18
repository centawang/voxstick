# VoxStick firmware v0.1.2

This directory is served by GitHub Pages for the browser-based installer.

| File | Purpose | SHA-256 |
|---|---|---|
| `voxstick-full.bin` | Merged ESP32-S3 image for one-shot flashing at `0x0` | `18ffb25084d2d9f25602a6a69e540aba9c9aa2f064c60a3bda3ac675b2795f34` |
| `manifest.json` | ESP Web Tools manifest consumed by `docs/install.html` | - |

This build mirrors the M5GFX StickS3 LCD bring-up more closely: M5PM1 GPIO2 is
driven high for the L3B LCD/MIC/SPK rail, the LCD SPI device uses 3-wire mode,
and the ready screen is a full green fill so display power is obvious.
