# voxstick

> **[简体中文 README](README.zh.md)** — 中文版本带微信输入法配置详细教程

Hybrid USB/BLE push-to-talk dictation stick for macOS / Windows, built on the
M5Stack StickS3 (ESP32-S3).

The stick is a single composite USB device that hosts see as **both** a
16 kHz microphone *and* a HID keyboard. By default, tap the front button for
Enter, double-tap for Left Ctrl, or hold to move right. Lay it flat with
the screen facing up and the IMU mutes the mic automatically. Flat auto-mute
and all seven gesture actions are configurable from the browser. When no USB
host is mounted, the same actions automatically fall back to the bonded
`vibestick Keyboard` BLE HID device. USB always has priority; the microphone
and browser configuration remain USB-only. No drivers or companion app are
required on macOS / Windows / Linux.

## What it looks like

| StickS3 hardware | Upright = live mic | Flat, screen up = muted |
|---|---|---|
| <img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_02.webp" alt="M5Stack StickS3 front product photo" width="180"> | <img src="docs/assets/voxstick-upright-live.png" alt="StickS3 upright with VoxStick live microphone LCD" width="180"> | <img src="docs/assets/voxstick-flat-muted.png" alt="StickS3 lying flat, screen up, with VoxStick muted microphone LCD" width="180"> |

Product photos are referenced from the official
[M5Stack StickS3 documentation](https://docs.m5stack.com/en/core/StickS3).
The web installer uses drawn product images with VoxStick's LCD states placed
inside the real StickS3 screen.

## Recommended setup

For Chinese-with-mixed-English voice input we use **WeChat 输入法**
(WeType): it has the best handling for code-mixed speech plus
LLM-based correction we've seen. See [README.zh.md](README.zh.md) for
the step-by-step. Any whisper.cpp-based macOS dictation tool will also
work — VoiceInk, MacWhisper Pro, or macOS native Dictation.

## Why a hardware stick?

- **Real keyboard event**, not a global hotkey faked through Accessibility
- **Dedicated MEMS mic** + ES8311 codec, away from your laptop fan
- **Low-power status display** — dim backlight + one of five animated dogs on
   the 240×135 LCD while live; muted mode shows the doghouse
- **Copilot completion reminder** — this repository's VS Code agent flashes
   the LCD red and green when a task ends
- **IMU privacy mute** — face-up or face-down on a desk = mic muted

## Hardware

[M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3) — ESP32-S3
+ ES8311 codec + MEMS mic + ST7789 LCD + BMI270 IMU + USB-C, ~$25.

## Install prebuilt firmware

The easiest path is the browser installer:

1. **Computer:** open <https://openbrt.github.io/voxstick/install-en.html> in
   desktop Chrome or Microsoft Edge.
2. **Hardware:** connect the M5Stack StickS3 to the computer over USB-C with a
   data-capable cable.
3. **Hardware:** hold the StickS3 side reset/PWR button for about 2 seconds.
   Release it when the internal green LED blinks; the StickS3 is now in
   download mode.
4. **This page:** click **Connect and flash**, choose the StickS3 serial port
   in the browser picker, and approve the install.
5. **Hardware:** after flashing, unplug USB, double-click the side PWR button
   to fully power off the StickS3, then plug USB back in.
6. **Computer OS:** after the firmware boots, select `StickS3-Mic` in system
   sound input settings.
7. **Keyboard check:** tap BtnA to send `Enter`, or double-tap it to send one
   `Left Ctrl` press and release with the default configuration.
8. **BLE fallback:** connect `vibestick Keyboard` in the OS Bluetooth settings.
   The firmware starts encrypted Just Works bonding automatically and reconnects
   on later boots. Unplugging USB then routes the same gestures to BLE.
9. **Optional config:** while USB is connected, open
   <https://openbrt.github.io/voxstick/config.html> to change all seven
   actions, flat auto-mute, its orientation threshold and transition hold
   time, the shared long-press threshold, LCD dog style, and Copilot completion
   flash count.

The installer is powered by
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) and writes the
merged `voxstick-full.bin` image at flash offset `0x0`. The static installer
files live in [`docs/install-en.html`](docs/install-en.html) and
[`docs/install.html`](docs/install.html). The WebUSB config page lives in
[`docs/config.html`](docs/config.html), with the firmware manifest in
[`docs/firmware/v0.1.6/manifest.json`](docs/firmware/v0.1.6/manifest.json).

If GitHub Pages is not enabled yet, publish this repository from the `docs/`
folder (`Settings > Pages > Deploy from a branch > main / docs`) and the URL
above will become live.

Command-line fallback:

```sh
curl -LO https://github.com/openbrt/voxstick/releases/download/v0.1.6/voxstick-full.bin
esptool.py --chip esp32s3 -p /dev/cu.usbmodem* write_flash 0x0 voxstick-full.bin
```

On Windows, replace the port with something like `COM5`.

Note: browser flashing may not reliably auto-reboot the StickS3 because the
board is battery-backed behind the M5PM1 PMIC. The official StickS3 button
operations are long press = download mode, double press = power off, and single
press = power on.

## Build

```sh
. $IDF_PATH/export.sh
idf.py build flash
```

ESP-IDF 5.5 + components fetched automatically by IDF Component
Manager (`espressif/usb_device_uac`, `espressif/esp_codec_dev`,
`espressif2022/bmi270`) plus ESP-IDF's built-in NimBLE/`esp_hid` components.

For chip recovery (no buttons), see [`tools/trigger-download.sh`](tools/trigger-download.sh).

## Button gestures

| Gesture | Default HID output | Use |
|---|---|---|
| Tap BtnA | `Enter` | Confirm or send in the focused app |
| Double-tap BtnA | `Left Ctrl` | Send one Left Ctrl press and release |
| Hold BtnA (≥ 600 ms) | `Right Arrow` | Move the cursor or current selection right once |
| Tap BtnB | `Down Arrow` | Move the cursor or current selection down once |
| Double-tap BtnB | `Up Arrow` | Move the cursor or current selection up once |
| Hold BtnB (≥ 600 ms) | `Left Arrow` | Move the cursor or current selection left once |
| Shake the stick | `Backspace` × 20 | Configurable action; delete up to 20 preceding characters by default |
| Microphone muted → live for 2 seconds | `Left Ctrl` | Send once only if it stays live continuously |
| Hold BtnA at boot | (none) | Reboot to ROM download mode for safe re-flash |
| Hold BtnA + BtnB at boot | (none) | Clear BLE bonds without resetting action mappings |

When USB is enumerated, all configurable actions are sent only through USB HID.
Without a USB host (including battery operation or a charge-only adapter), they
are sent only through BLE. A BLE connection may remain open while USB is active,
but it receives no duplicate reports. The microphone and its two-second unmute
gesture are USB-only because ESP32-S3 does not support standard Bluetooth audio.

Use the [WebUSB config page](https://openbrt.github.io/voxstick/config.html)
to change all six runtime button actions, the shake action, and the shared
long-press threshold without rebuilding firmware. Flat auto-mute and its
orientation threshold and transition hold time are configurable on the same
page. Choose pixel dog (the default), Shiba, Corgi, Labrador, or Border Collie;
the [dog preview](https://openbrt.github.io/voxstick/dog-preview.html) shows all
five styles. Existing devices migrate to the pixel dog without changing saved
actions or orientation settings. The 350 ms double-click window and boot-time
recovery gestures remain fixed. Saved mappings apply to both USB and BLE,
though the page itself requires USB. Built-in action presets include `Delete`
and `Backspace × N`; `N` defaults to 20 and can be set from 2 to 100.
Boot and USB reconnect establish the microphone-state baseline and do not send
Left Ctrl. Muting again within the two-second window cancels the pending key.

## Copilot completion reminder

This repository includes a VS Code Agent `Stop` hook in
[`.github/hooks/voxstick-complete.json`](.github/hooks/voxstick-complete.json)
that notifies a USB-connected voxstick when an agent task ends. The LCD
alternates full-screen red and green, then restores the latest mascot or
doghouse state. The default is three rounds; the WebUSB config page accepts
`0` through `10`, where `0` disables the reminder.

The hook is repository-scoped and uses the USB data connection, not BLE. If the
device is absent, busy, connected through a charge-only cable, or running old
firmware, it exits silently without affecting Copilot. Its native helper is
compiled lazily and requires `clang`, `pkg-config`, and `libusb-1.0`. On macOS,
install the host packages with `brew install pkg-config libusb`; Linux users can
install the equivalent distribution packages. The checked-in Windows hook is
currently a quiet no-op. VS Code Agent Hooks are a preview feature and their
configuration format may change.

## License

[MIT](LICENSE)

## Credits

- USB UAC scaffold inspired by [atomic14/esp32-usb-uac-experiments](https://github.com/atomic14/esp32-usb-uac-experiments)
- M5StickS3 PMIC + codec init pattern lifted from
  [m5stack/M5Unified](https://github.com/m5stack/M5Unified) and
  [m5stack/M5GFX](https://github.com/m5stack/M5GFX)
- Built on Espressif's
  [`usb_device_uac`](https://components.espressif.com/components/espressif/usb_device_uac),
  [`esp_codec_dev`](https://components.espressif.com/components/espressif/esp_codec_dev),
  and [`espressif2022/bmi270`](https://components.espressif.com/components/espressif2022/bmi270)
  components

See [SESSION-NOTES.md](SESSION-NOTES.md) for the bring-up postmortem
(PMIC L3B LDO, ES8311 `no_dac_ref` bug, M5PM1 BOOT-pin brick recovery,
and other rough edges).
