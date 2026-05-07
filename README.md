# scribestick

USB push-to-talk dictation stick for macOS, built on the M5Stack StickS3 (ESP32-S3).

Hold a button, talk, release — your transcription appears at the cursor in any
app. The stick presents itself to the host as a standard USB Audio Class
microphone plus an HID button, so it works with any whisper.cpp-based
voice-to-text tool (e.g. [VoiceInk](https://github.com/Beingpax/VoiceInk)) with
zero drivers and zero macOS-specific code on the device side.

## Why a hardware stick?

- **Real button**, not a global hotkey that fights with apps
- **Dedicated MEMS mic** + ES8311 codec, away from your laptop fan
- **Status display** — recording / transcribing / preview text on the 240×135 LCD
- Works on **Windows / Linux** too — UAC + HID is OS-agnostic

## Status

Early development. See [PLAN.md](PLAN.md) for the three-step roadmap.

| Step | Goal | Status |
|---|---|---|
| 1 | StickS3 enumerates as `StickS3-Mic` on macOS, QuickTime records cleanly | 🚧 in progress |
| 2 | BtnA = push-to-talk, end-to-end dictation via VoiceInk | ⏳ |
| 3 | LCD status UX + reverse channel from Mac | ⏳ |

## Hardware

- [M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3) — ESP32-S3-PICO-1, 8 MB flash, 8 MB OPI PSRAM, ES8311 codec + MEMS mic, 240×135 ST7789 LCD, 2 buttons

## License

[MIT](LICENSE)

## Credits

- USB UAC scaffold inspired by [atomic14/esp32-usb-uac-experiments](https://github.com/atomic14/esp32-usb-uac-experiments)
- Built on Espressif's [`usb_device_uac`](https://components.espressif.com/components/espressif/usb_device_uac) component
