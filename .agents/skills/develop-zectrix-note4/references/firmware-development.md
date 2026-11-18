# Firmware Development

## Supported baseline

The official project is a standalone C/C++ ESP-IDF application for ESP32-S3. It requires ESP-IDF 5.4 or later, builds board components as C++17, and downloads `espressif/esp_codec_dev ~1.5` through the ESP Component Registry.

Start from the [official reference repository](https://github.com/itopinion/zectrix-note4-epd-demo) when possible. It is MIT-licensed and intentionally independent of the commercial consumer firmware and cloud service.

```bash
git clone https://github.com/itopinion/zectrix-note4-epd-demo.git
cd zectrix-note4-epd-demo
idf.py set-target esp32s3
idf.py build
```

`idf.py set-target` generates `sdkconfig`. If upstream `sdkconfig.defaults` changes after configuration, reconcile or regenerate the local configuration rather than assuming new defaults were applied.

## Install or activate ESP-IDF on macOS

If `idf.py --version` is unavailable, follow Espressif's [ESP32-S3 macOS setup](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/get-started/linux-macos-setup.html). Use the ESP-IDF Installation Manager or install a project-compatible 5.4+ release manually. For reproducible work on the reference v1.0.0 tag, prefer a 5.4.x release unless the checked-out project says otherwise.

Activate the installed environment in each development shell. An Installation Manager setup provides a version-specific activation script; a manual checkout uses:

```bash
. /absolute/path/to/esp-idf/export.sh
idf.py --version
command -v idf.py
command -v esptool || python -m esptool version
```

The activated environment supplies the matching compiler, Python packages, IDF Monitor, and esptool. Keep ESP-IDF and the application checkout in paths without spaces.

## Required project configuration

The reference defaults select:

- 16 MB flash.
- Custom `partitions.csv`.
- 8 MB octal PSRAM at 80 MHz, available through the allocator.
- C++ exceptions disabled.
- ES8311 codec support.

The reference partition table contains NVS at `0x9000` (24 KiB), PHY initialization data at `0xf000` (4 KiB), and one factory application at `0x10000` (3 MiB). Do not assume this layout matches factory consumer firmware.

## Repository layout

| Path | Purpose |
|---|---|
| `components/zectrix_epd/` | Public SSD2683 display driver |
| `components/zectrix_board/` | Pins, power, buttons, I2C, RTC, NFC, audio, battery, and charging |
| `components/zectrix_demo_ui/` | Canvas, bitmap font, and demo UI |
| `components/zectrix_self_test/` | Hardware acceptance tests |
| `main/app_main.cc` | Application integration and lifecycle example |
| `main/assets/` | Embedded display binaries |
| `tools/` | Image, animation, and font conversion tools |

For a new project, either fork the reference or copy the components actually needed. Add component names to the consuming component's `REQUIRES`. Preserve their license files and third-party notices.

## Board adapter

`ZectrixBoard::Init()` sets up the power and GPIO path, shared I2C bus, battery ADC, buttons, RTC, NFC, and charging state. Its public surface includes:

- `WaitButton()` and `DrainButtons()` for debounced click and long-press events.
- `rtc()` and `nfc()` for initialized device adapters.
- `PrepareAudio()` for lazy audio setup.
- `ReadPowerSnapshot()` for battery millivolts, estimated percentage, and charge state.
- `SetPowerLed()`, `SetAudioPower()`, and `CutBatteryPower()` for board power behavior.

Keep board pins centralized in `components/zectrix_board/include/zectrix_board_config.h`. For another hardware revision, update that file, review EPD configuration, confirm flash/PSRAM/partitions, and rerun every physical test.

## RTC

The `RtcPcf8563` adapter supports time read/write, alarm configuration, a seconds countdown timer, interrupt enable/flag handling, and an interrupt callback. Use the shared I2C bus supplied by `ZectrixBoard`.

Be explicit about whether `tm` values represent local time or UTC in application code. The reference class names its parameters as local time but does not supply timezone policy.

## NFC

The `ZectrixNfc` adapter supports:

- Power and field detection callbacks.
- 16-byte block reads and writes.
- User-data and command-area access.
- Seven-byte UID reads.
- NDEF read/write plus text and URI helpers.

The published adapter maps user blocks `0x01` through `0x37` and exposes `0x04` as the start of its command area. Check bounds through the public helpers instead of issuing arbitrary raw writes. Back up user data before a temporary test write and verify restoration.

## Audio

`PrepareAudio()` returns the board's codec abstraction. Check `valid()` after codec initialization. The public interface supports volume, input gain, input/output enable, 16-bit PCM input/output vectors, and sample-rate/channel getters.

Enable the audio rail and amplifier only while needed. Do not infer echo cancellation, resampling, encoding, or network streaming from the low-level codec interface.

## Wi-Fi and application behavior

Wi-Fi and Bluetooth use normal ESP-IDF ESP32-S3 facilities; the Note4 reference repo adds a Wi-Fi RF self-test rather than a device-specific networking API. Keep credentials out of source control and use the project's configuration or provisioning strategy.

The demo UI is an example, not a required application architecture. Its inactivity, button, and menu behavior may be replaced while retaining the underlying hardware mappings.

Sources: [reference README](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/README.md), [quick start](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/QUICK_START.md), [hardware map](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/HARDWARE.md), and the repository's public component headers.
