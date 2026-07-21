# voxstick implementation plan

Three independently verifiable steps. Each step's output is usable on its own
even if the next step never lands.

## Step 1 — USB UAC bring-up

**Goal**: macOS `System Settings > Sound > Input` shows `StickS3-Mic`,
QuickTime records clean voice from the ES8311 mic.

- PlatformIO project with `framework = espidf`, `board = esp32-s3-devkitc-1`
- Pull `espressif/usb_device_uac` component via `idf_component.yml`
- I2S in **standard** (not PDM) mode against the ES8311 codec at:
  - MCLK=18 BCLK=17 WS=15 DIN=16 (StickS3 ES8311 mic-in path)
- ES8311 init via I2C0 (SDA=47 SCL=48) — drop in `esp_codec_dev` component or
  inline the register sequence used by M5Unified
- Mic only for v1. Skip speaker / power amp.
- 16 kHz / mono / 16-bit (Whisper-native; smaller MTU, less USB churn)

**Acceptance**
- `system_profiler SPAudioDataType | grep -A5 StickS3` shows the device
- 30 s continuous recording in QuickTime, no dropouts, voice intelligible

**Risks**
- ES8311 init order under PMIC AXP2101 — may need to power codec rail before
  I2C probe
- StickS3 PA defaults to ON → audible click; mute via M5PM1 register or leave
  the speaker amp disabled
- TinyUSB endpoint budget — UAC alone uses 2 EPs, fine here

## Step 2 — HID PTT + end-to-end dictation

**Goal**: hold BtnA → speak → release → transcribed text lands at the cursor
in whatever app is focused.

- USB descriptor → composite UAC + HID Consumer Control (reference
  [Alexaznavour/micemul](https://github.com/Alexaznavour/micemul))
- BtnA tap/double/long defaults → `Enter` / `Left Ctrl` / `Right Arrow`
- BtnB tap/double/long defaults → `Down Arrow` / `Up Arrow` / `Left Arrow`
- All six runtime button actions and the shake action are configurable over
  WebUSB; the 350 ms double-click window stays fixed and both buttons share the
  long-press threshold
- Presets include `Delete` and asynchronous `Backspace × N` batches; `N`
  defaults to 20 and is configurable from 2 to 100
- Shake defaults to 20 complete `Backspace` taps and dispatches its configured
  action outside the IMU task
- Effective microphone state `muted → live` for two continuous seconds → one
  `Left Ctrl` tap; muting again cancels the timer, while boot and USB reconnect
  establish a baseline without sending a key
- USB-mounted HID has strict priority; without a USB host, all seven configured
  actions fall back to the bonded `vibestick Keyboard` BLE HID device. Route
  snapshots keep complete taps and repeated batches on one transport
- BLE HID uses Security Mode 1 Level 2 and proactively initiates Just Works
  bonding; the encrypted bond persists in NimBLE NVS and reconnects after reboot
- BtnA at boot keeps ROM recovery; BtnA+BtnB clears BLE bonds without erasing
  the action configuration. The microphone and WebUSB remain USB-only
- LCD: minimum viable indicator — red dot in top-right while button is held
- Mac side: install [VoiceInk](https://github.com/Beingpax/VoiceInk),
  - input device = `StickS3-Mic`
  - PTT hotkey = F19
  - inject to focused app

**Acceptance**
- In Notes / Slack / terminal, hold BtnA, dictate, release → text appears
  within ~3 s
- Macro 30-min session has no dropped audio, no missed PTT events

**Risks**
- macOS first-time HID approval prompt — unavoidable (Ventura+ security)
- VoiceInk hotkey support for high F-keys — fallback via Karabiner remap
- Composite descriptor endpoint layout on ESP32-S3 native USB (6 EPs total)

## Step 3 — LCD UX + reverse channel

**Goal**: stick becomes a status panel — `recording → transcribing → preview`,
with optional double-tap-to-toggle-language and IMU wake.

- Current implementation uses the existing WinUSB/WebUSB vendor interface as
  the Mac → device reverse channel; no extra CDC endpoints are required
- VS Code Agent `Stop` hook sends an acknowledged completion command over that
  interface. The LCD flashes red/green for a configurable 0–10 rounds (default
  3), then restores its latest mascot state
- The hook uses an acknowledged device-level vendor control request, so it does
  not need to claim the bulk configuration interface. It is repository-scoped,
  silently skips unavailable USB devices, and lazily compiles a small libusb
  helper on macOS/Linux
- Future status messages can extend the same framed vendor protocol
- Mac daemon (Swift / Node, ~150 LOC):
  - either: tail VoiceInk's log / observe its state notifications and forward
  - or: implement the PTT loop directly (CGEventTap → AVAudioEngine →
    whisper.cpp → CGEventPost), giving fine-grained status control
- Firmware draws three states on the 240×135 LCD:
  - `recording` — red waveform / level meter
  - `transcribing` — spinner
  - `done "<text>"` — text bubble for 2 s
- Optional: add a dedicated language-switch gesture and re-dictate action
  cached PCM buffer, IMU wake

**Risks**
- VS Code Agent Hooks are currently a preview feature and may change format
- VoiceInk doesn't expose IPC for state — would force "implement own PTT"
  branch, which is more work but more controllable

## Out of scope (for now)

- WiFi audio / Bluetooth audio — ESP32-S3 has no Classic HFP or BLE Audio ISO
- On-device ASR — whisper-tiny on ESP32-S3 is borderline, not worth the
  complexity when Mac-side whisper.cpp is fast and free
- Cloud ASR — privacy regression vs. local whisper.cpp
