# voxstick — session notes (2026-05-07 / 2026-05-08)

A running log of what worked, what bricked us, and what's still TODO. Read
this before picking the project back up.

## What's done

**Firmware (`main/main.c`, `main/usb_descriptors.c`)**

- ESP-IDF 5.5.2 project, `framework=espidf` (PlatformIO + UAC component
  has a `usb_descriptors.c` double-register bug — we use raw `idf.py`).
- Composite USB descriptor: **UAC microphone (16 kHz / mono / 16-bit)
  + HID Keyboard** on a single device. Hosts see the device as
  `StickS3-Mic` (USB) and `voxstick PTT` (audio + HID).
- HID Keyboard: BtnA = F19 down/up, BtnB = F20 (BtnB unpopulated on this
  hardware revision). VoiceInk-friendly PTT keys.
- LCD ST7789P3 boot-stage indicator: white → yellow → orange/red →
  cyan → green. Magenta while BtnA held. Red on codec init failure.
- M5PM1 PMIC init does the **bare minimum** M5Unified does: configure
  GPIO3 as push-pull output, drive LOW (PA off so the speaker amp
  doesn't tick on every I2S start). No LDO writes — earlier attempts
  to write AXP2101-style register addresses (0x90, 0x92, …) to M5PM1
  put the codec into a state we couldn't reliably recover from in
  software.
- ES8311 codec init via `esp_codec_dev`. I2S channel is **enabled**
  before codec probe so MCLK is running on GPIO 18 — without that,
  ES8311 NACKs every I2C transaction.
- Soft-reboot-to-download-mode gesture: hold BtnA at boot →
  `RTC_CNTL_OPTION1.FORCE_DOWNLOAD_BOOT` → `esp_restart()` → ROM
  stays in download mode. Avoids the M5PM1's BOOT-pin latch which
  bricks the device through any reset.

**Mac side**

- Composite device enumerates correctly: UAC mic shows up as the
  default input device, HID interface attaches `AppleUserHIDDevice`.
- BLE scan from previous claude-buddy work proved the firmware-side
  HID emit path: BtnA press flips the LCD colour magenta (locally
  visible) and pushes a HID Keyboard report with F19 keycode 0x6E.

## What's blocking the end-to-end (PTT → VoiceInk → text)

**ES8311 codec is unreachable** (NACK on every I2C transaction at
`0x18`). Same hardware previously worked with the same firmware in
the same session — got broken when our PMIC init wrote AXP2101
register addresses (0x90/0x92/etc.) into M5PM1 (which has a different
register map). M5PM1 register 0x06 brute-forced to 0xFF (all LDO bits
on) does not bring the codec back, so the codec rail isn't on a bit
of 0x06.

Recovery path (verified once already this session): unplug USB,
double-click PWR if needed, wait 4–8 hours for the LiPo to fully
discharge, then replug. True POR resets M5PM1 to factory defaults and
the codec wakes up.

After recovery, the codec init is the **only** outstanding item.

## What we learned about the M5StickS3 (the hard way)

- **PMIC = M5PM1 at I2C 0x6E** (HANDOVER was correct; AXP2101's
  standard 0x34 doesn't ACK).
- **M5PM1 register map** (from M5Unified):
  - `0x06` LDO enable (bit assignment per-board, undocumented)
  - `0x10` GPIO direction (1=output)
  - `0x11` GPIO output level
  - `0x13` GPIO mode (1=open-drain, 0=push-pull)
  - `0x16` GPIO function (1=alt, 0=GPIO)
  - GPIO3 on M5StickS3 = speaker PA enable (drive LOW to mute)
- **Codec is on a rail outside register 0x06** — M5Unified's StickS3
  init doesn't write to 0x06 at all and the codec just works after
  POR. So the rail is on by default. Our wrong writes broke the
  default state.
- **IMU (BMI270) sits at 0x68** on the same I2C bus.
- **No BtnB on top** — only BtnA front + side PWR. Buddy HANDOVER's
  pinout for BtnB is from a different revision.
- **LCD dies when codec is broken** — possibly because the LCD
  backlight rail (or LCD VDD) is gated by the same M5PM1 state.
  After our bad PMIC writes, the screen went dark even though
  GPIO 38 was driven HIGH. This made debugging painful.

## Side-button gotchas

| Press | Effect |
|---|---|
| Short-press PWR | "Restart" — but it's a low-power resume on this hardware, **does not run app_main from scratch**. Our boot-time BtnA gesture won't fire. |
| Double-click PWR | Power off via PMIC. With USB unplugged, this fully kills the chip. With USB still plugged, only partial. |
| **Long-press PWR** | **Enters ROM download mode AND latches the BOOT pin.** This is the brick condition — the strap pin stays asserted across software resets, and only a true POR (battery drained completely with USB unplugged) clears it. **Use the boot-time BtnA gesture instead.** |

## Reliable flash workflow (post-fix)

1. **Hold BtnA** before triggering reset
2. **Cold-boot the device**: unplug USB → double-click PWR → keep BtnA
   held → plug USB. App boots, sees BtnA held, soft-reboots into
   download mode without touching the PMIC BOOT pin
3. `idf.py -p /dev/cu.usbmodem* flash`
4. esptool's hard-reset clears `FORCE_DOWNLOAD_BOOT` and the new
   firmware boots

For dev iteration when the chip is already in app mode (UAC owns USB,
no `/dev/cu.usbmodem*`), trigger step 1+2 to get a port back. Never
long-press PWR — it's a one-way trip to the brick state.

## What's next

### 2026-05-08 continuation

- Raw IDF build works when invoked through PlatformIO's bundled ESP-IDF
  package; plain `pio run` still hits the known duplicate
  `usb_descriptors.c.o` action bug.
- Clean production firmware was flashed once to `/dev/cu.usbmodem21101`.
- macOS now enumerates the running firmware correctly:
  - USB device: `StickS3-Mic` (`idVendor=0x303a`, `idProduct=0x8001`)
  - Audio input: `voxstick PTT`, 16 kHz, mono, default input
  - HID: `AppleUserHIDDevice`, keyboard report descriptor present
- Before the UAC callback fix, audio e2e was still not complete: System
  Settings input level stayed flat while the Mac speaker played a test phrase,
  and ffmpeg/AVFoundation hung opening `voxstick PTT`. This matched the
  firmware bug where `uac_input_cb()` returned `ESP_FAIL` when `codec_init()`
  failed, so the host never got audio packets.
- Fixed that bug in `main/main.c`: codec-not-ready now returns zero-filled
  audio buffers, and the LCD keeps/rediscovers RED as the final health colour
  when the codec is not ready.
- Added a no-button recovery path to production firmware:
  - Host sends the standard keyboard LED Output report value `0x1f` three
    times to the HID interface.
  - Firmware sets `RTC_CNTL_OPTION1.FORCE_DOWNLOAD_BOOT` and restarts into
    ROM download mode.
  - `tools/trigger-download.sh` builds/runs the macOS IOKit helper. Verified:
    `StickS3-Mic` disappears and `/dev/cu.usbmodem21101` reappears. On the
    updated production build, esptool can sync in that state (`USB mode:
    USB-OTG`, MAC `70:04:1d:dc:e9:d0`), so future reflashes do not need manual
    button entry.
- Added compile-time escape hatches:
  - `-DVOX_AUTO_DOWNLOAD_AFTER_SEC=N`
  - `-DVOX_CODEC_FAIL_DOWNLOAD_AFTER_SEC=N`
  - `VOX_DEBUG_NO_UAC` builds now default to a 180 s auto-download safety
    timer unless `-DVOX_DEBUG_NO_UAC_AUTO_DOWNLOAD_SEC=0` is explicitly set.
- Added diagnostic logs for LCD backlight GPIO level, LCD init result, and
  M5PM1 registers `0x06/0x10/0x11/0x13/0x16`.
- Flashed the fix, then used:
  `esptool.py --chip esp32s3 -p /dev/cu.usbmodem21101 --after watchdog_reset read_mac`
  to escape the ROM download state after flashing. `hard_reset` and monitor
  reset both kept returning to `boot:0x3 (DOWNLOAD(USB/UART0))`; watchdog
  reset booted the app and UAC re-enumerated.
- After the fix, ffmpeg records successfully:
  `build/e2e-voxstick.wav` is 5.0 s, 16 kHz, mono, 16-bit PCM.
  `astats` reports all-zero samples (`Peak level dB: -inf`,
  `RMS level dB: -inf`). So the host stream is now healthy, but the real mic
  path is still silent.
- 2026-05-08 late-night continuation:
  - User reported "side reset held about 2 s"; current board then enumerated
    back in normal app mode (`StickS3-Mic` / `voxstick PTT`), with no
    `/dev/cu.usbmodem*`, which is expected for UAC/HID app mode.
  - Re-ran audio e2e after that reset:
    `build/e2e-voxstick-after-reset.wav` is 5.0 s, 16 kHz, mono, 16-bit
    PCM, but still all zero (`nonzero=0/80000`, `Peak level dB: -inf`).
  - Added `VOX_DIAG_CODEC_FAIL_TONE`: when enabled, `uac_input_cb()`
    returns a 1 kHz square wave if `codec_init()` failed and `g_codec_ready`
    is false. This separates "codec init failed" from "codec init says ready
    but I2S/ES8311 samples are zero".
  - Built `build-diag-tone` with
    `-DVOX_DIAG_CODEC_FAIL_TONE=ON -DVOX_AUTO_DOWNLOAD_AFTER_SEC=180`;
    build passed (`voxstick.bin` size `0x51a40`).
  - Hardened no-button recovery for future firmware:
    - Existing HID LED Output report magic remains.
    - Added UAC volume magic (`2,98,2,98`) and mute magic (`1,0,1,0,1`)
      callbacks to request ROM download.
    - Added a device-level TinyUSB vendor control request:
      `bRequest=0x5d`, `wValue=0x5344`, `wIndex=0x4c44`; this should be the
      most reliable host-controlled path because it does not depend on the
      keyboard HID driver.
    - `tools/trigger-download.sh` now builds and tries three helpers:
      IOHID SetReport, raw USB vendor/HID control, and UAC control.
  - Production build also passed after those changes (`voxstick.bin` size
    `0x519e0`).
  - Current blocker: these new recovery paths are not flashed yet. On the
    old app currently running, `tools/trigger-download.sh` fails as expected:
    IOHID is blocked by macOS (`0xe00002c1`), raw HID has
    `LIBUSB_ERROR_ACCESS`/`LIBUSB_ERROR_PIPE`, and UAC SET_CUR stalls with
    `LIBUSB_ERROR_PIPE`. Need one more manual/physical ROM-download entry to
    flash either production or diagnostic firmware; after that, host-side
    vendor control should avoid button involvement.

Next physical step:

1. Get the board into ROM download one more time (manual side reset/boot
   entry is OK for this one flash), then flash the prepared diagnostic build:
   `idf.py -B build-diag-tone -p /dev/cu.usbmodem* flash`.
2. Boot app with watchdog reset if it sticks in ROM download:
   `esptool.py --chip esp32s3 -p /dev/cu.usbmodem* --before no_reset --after watchdog_reset read_mac`.
3. Re-run `bash tools/trigger-download.sh` on the newly flashed firmware.
   Expected: vendor control path makes `StickS3-Mic` disappear and returns
   `/dev/cu.usbmodem*` without button help.
4. Run the diagnostic audio recording:
   - 1 kHz/nonzero audio = `codec_init()` failed; focus on ES8311 I2C/PMIC.
   - Still all zero = `g_codec_ready` true; focus on I2S port/slot/MCLK/ES8311
     ADC configuration.
5. Resolved board recovery after the late debug attempt:
   - A `VOX_DEBUG_NO_UAC=ON` build without the new 180 s safety timer was
     flashed while testing serial diagnostics.
   - It enumerates as Espressif `303a:0009` / `/dev/cu.usbmodem21101`, but
     does not output logs and does not accept esptool sync.
   - 2026-05-08 follow-up: the user reported manually entering download mode,
     but the device still kept the same `303a:0009` CDC/JTAG descriptor and
     esptool still saw no ROM sync response. Verified with `idf.py flash`,
     `esptool --before no_reset`, `esptool --before usb_reset`, a custom
     "hold DTR/BOOT low" reset sequence, and OpenOCD with PID `0x0009`.
     DTR/RTS did not change the USB `sessionID`, so this port is still the
     old debug app CDC endpoint, not a usable ROM download session.
   - Tried monitor `Ctrl+T Ctrl+P`, pyserial DTR/RTS polarity sequences,
     1200-baud close, DFU_DETACH on interface 2, and CDC line-state control
     requests; none entered download mode.
   - 2026-05-08 final recovery: user held side reset for about 2 s; device
     changed to Espressif `303a:1001` / `USB JTAG/serial debug unit`, and
     `idf.py -p /dev/cu.usbmodem21101 flash` succeeded.
   - `hard_reset` left the chip in ROM download. Running
     `esptool.py --chip esp32s3 -p /dev/cu.usbmodem21101 --before no_reset --after watchdog_reset read_mac`
     booted the app, and `voxstick PTT` re-enumerated.
   - Verified future no-button recovery: `bash tools/trigger-download.sh`
     makes `StickS3-Mic` disappear and returns `/dev/cu.usbmodem21101`;
     `esptool --before no_reset read_mac` succeeds in that state. A watchdog
     reset boots the production app again.
6. Verify LCD final colour after the flashed fix:
   - GREEN = codec ready, retry ffmpeg/QuickTime/VoiceInk audio e2e
   - RED = codec still not ready, go back to true-POR/PMIC recovery path
7. Verify ES8311 ACKs at 0x18 in a debug/no-UAC build if RED persists.
8. If another flash lands in ROM download state, use watchdog reset as above.
9. Install [VoiceInk](https://github.com/Beingpax/VoiceInk), set PTT
   hotkey = F19, set input device = StickS3-Mic
10. Hold BtnA → speak → release → text appears at cursor

### 2026-05-09 continuation

- Fixed the new vendor-bulk recovery callback link issue. `usb_recovery.c`
  now exposes a normal strong helper symbol, and `main.c` references that
  helper so the archive member is pulled into the final ELF. Verified with
  `nm`:
  - production `build/voxstick.elf`: `T tud_vendor_rx_cb`
  - diagnostic `build-diag-tone/voxstick.elf`: `T tud_vendor_rx_cb`
- Flashed the diagnostic UAC build:
  `-DVOX_DIAG_CODEC_FAIL_TONE=ON -DVOX_AUTO_DOWNLOAD_AFTER_SEC=180`.
- Verified no-button recovery on the freshly flashed firmware:
  - app enumerated as `StickS3-Mic` / `voxstick PTT`
  - `bash tools/trigger-download.sh` sent the HID recovery report
  - device reappeared as `/dev/cu.usbmodem21101`
  - `esptool --before no_reset read_mac` synced in ROM download mode
    (`USB mode: USB-OTG`, MAC `70:04:1d:dc:e9:d0`)
  - watchdog reset booted the app again
- Recorded `build/e2e-voxstick-diag-after-recovery.wav` from AVFoundation:
  5.0 s, 16 kHz, mono, 16-bit PCM, 80000 samples.
  Stats: all 80000 samples nonzero, min `-4000`, max `4000`, RMS `4000`,
  zero crossings about 10000. First samples are the diagnostic square wave.
  Conclusion: USB UAC e2e is healthy; real mic silence is because
  `codec_init()` failed and `g_codec_ready == false`.
- Tried to continue with a `VOX_DEBUG_NO_UAC=ON` build to capture serial
  logs. Build and flash succeeded, and the board now enumerates as Espressif
  `303a:0009` / product `ESP32_S3` on `/dev/cu.usbmodem21101`.
- Stopped per user instruction because this hit the same no-progress loop as
  before: the `303a:0009` port produces no monitor/cat output, and an esptool
  `--before default_reset --after watchdog_reset read_mac` attempt failed with
  `No serial data received`. Current board state at stop time is still
  `303a:0009`, not `StickS3-Mic` and not confirmed ROM download.

Recommended next step from this state:

1. If the 180 s debug safety timer has worked, the board should change to
   `/dev/cu.usbmodem21101` in ROM download mode; flash `build-diag-tone` or
   production back immediately.
2. If it remains `303a:0009`, use the side-reset/ROM-entry sequence that
   previously changed it to `303a:1001`, then flash `build-diag-tone`.
3. Do not spend more time on the no-UAC serial-log path until the app console
   issue is understood; it repeats the same "port exists but no serial data"
   failure mode.

### VAD LCD indicator attempt

- Added a lightweight firmware-side VAD display in `main/main.c`:
  - `uac_input_cb()` computes smoothed average absolute PCM level.
  - a low-priority `vad_lcd_task` redraws a center circle every 100 ms.
  - idle/muted stays small; voice energy expands the circle; codec-not-ready
    shows a red circle.
- Production build passes after the change:
  `build/voxstick.bin` size `0x524b0`.
- Initially stopped per user instruction after one no-button recovery attempt failed:
  IOHID reported success but no download port appeared; raw USB vendor/HID
  and UAC fallbacks returned `LIBUSB_ERROR_PIPE` or `LIBUSB_ERROR_ACCESS`.
- A moment later the board did appear in ROM download as
  `/dev/cu.usbmodem21101` / `USB JTAG/serial debug unit`.
- Flashed the VAD LCD build successfully. Flash hard-reset left the device in
  download mode, so a single watchdog reset was used:
  `esptool.py --chip esp32s3 -p /dev/cu.usbmodem21101 --before no_reset --after watchdog_reset read_mac`.
- The board is now running the VAD LCD build as `StickS3-Mic`; macOS sees
  `voxstick PTT` as 16 kHz mono USB input, with no `/dev/cu.usbmodem*`.
- Verification recording `build/e2e-voxstick-vad-lcd.wav`: 3.0 s,
  16 kHz, mono, 16-bit PCM. Samples are still all zero, so USB UAC is intact
  but ES8311/codec remains the blocker for real VAD expansion.

### Screen dark + zero input follow-up

- User reported the StickS3 screen is still dark and macOS input level does
  not move when speaking.
- Confirmed current UAC app state from macOS:
  - `StickS3-Mic` is enumerated
  - `voxstick PTT` is default 16 kHz mono USB input
  - recording `build/e2e-current-user-dark-screen.wav` is still all zero
- Checked official M5Stack sources/docs:
  - StickS3 docs list LCD backlight as ESP32-S3 GPIO38 and ES8311 as I2C
    address `0x18`.
  - M5Unified StickS3 init only configures M5PM1 GPIO3 for speaker PA mute.
  - M5PM1 register docs define `0x06 bit2` as `LDO_EN`, the 3.3 V LDO.
- Updated firmware to explicitly enable M5PM1 `0x06 bit2` before LCD init and
  moved startup order to PMIC/LDO before LCD/codec. Built and flashed this
  LDO fix successfully (`build/voxstick.bin` size `0x52600`).
- After flashing the LDO fix, the board boots as `StickS3-Mic`, but recording
  `build/e2e-ldo-fix-check.wav` is still all zero. So LDO enable alone did
  not restore ES8311 audio.
- Made an additional local code change, not flashed yet: codec-not-ready LCD
  state is now full bright red with a white circle, and LCD backlight GPIO38
  drive strength is set to max. Build passes with the same app size.
- Stopped after one recovery attempt failed again: IOHID returned success but
  no `/dev/cu.usbmodem*` appeared, and USB vendor/HID/UAC fallbacks returned
  `LIBUSB_ERROR_PIPE` / `LIBUSB_ERROR_ACCESS`. Current board is still the
  flashed LDO-fix UAC app, not the latest bright-red LCD build.
- User then reported the board was ready. It was in ROM/USB-JTAG download as
  `/dev/cu.usbmodem21101`.
- Flashed the latest bright-red LCD build successfully. Flash hard-reset left
  the board in ROM download; one watchdog reset booted the app.
- Current board state after this flash:
  - USB app mode: `StickS3-Mic`
  - macOS audio: `voxstick PTT`, default input, 16 kHz mono
  - no `/dev/cu.usbmodem*`
  - recording `build/e2e-bright-red-screen-build.wav` is still all zero
    (`Peak level dB: -inf`, `RMS level dB: -inf`)
- User still needs to visually confirm whether the screen now shows the strong
  red/white codec-failure indicator. If it is still fully dark, LCD power or
  panel init/backlight is independently broken from the UAC path.
- User confirmed the screen is still fully black after the bright-red build.
  That means LCD/backlight/panel power or panel init is broken independently
  of the USB UAC path. Do not keep tuning VAD colors until LCD power/init is
  proven alive.

## Files of interest

- [PLAN.md](PLAN.md) — the original three-step roadmap
- [main/main.c](main/main.c) — firmware
- [main/usb_descriptors.c](main/usb_descriptors.c) — composite descriptor
- [main/tusb_config.h](main/tusb_config.h) — `CFG_TUD_HID=1` enable
- [tools/trigger-download.sh](tools/trigger-download.sh) — host-side HID
  recovery trigger for production firmware
- [sdkconfig.defaults](sdkconfig.defaults) — `USB_DEVICE_UAC_AS_PART=y`
- [README.md](README.md) — public-facing project description
