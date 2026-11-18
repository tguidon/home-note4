# Deployment and Recovery

Flashing changes device state and replaces installed firmware. Confirm the monochrome NOTE4 hardware, serial port, image type, offsets, and recovery plan before writing.

## Find the serial port

Use a USB data cable, preferably connected directly to the computer.

- macOS: `/dev/cu.usbmodem*`
- Linux: `/dev/ttyACM*` or `/dev/ttyUSB*`
- Windows: the matching `COM` port

When more than one candidate exists, compare the port list before and after connecting the Note4. Do not guess.

## Back up factory flash

ZECTRIX says to back up the complete 16 MB factory flash before installing third-party firmware or changing the partition layout, but it does not publish a Note4-specific backup/restore command or factory image.

When the connected device is confirmed to use an ordinary readable 16 MB flash, a generic, non-destructive esptool read is:

```bash
esptool --chip esp32s3 -p PORT read-flash 0 ALL note4-factory-backup.bin
```

This command comes from Espressif tooling, not a ZECTRIX recovery procedure. Run it from an activated ESP-IDF environment. If no `esptool` executable is on `PATH`, use `python -m esptool` with the same arguments. Check `read-flash --help` for the installed version. Confirm the result is 16,777,216 bytes, record a SHA-256 checksum, and store it outside the build directory.

For the first custom installation, a second independent full read and byte-for-byte comparison provide stronger evidence than checking file size alone. This was verified on the `home-note4` device on 2026-09-05. Two matching reads do not prove restoration works. Keep backups private because they can include credentials and other NVS data.

Do not infer encryption state, eFuse state, restore offsets, or factory-image compatibility. Restoring is a separate destructive write. If recovery is required and the batch-specific process is unknown, stop and direct the user to ZECTRIX support.

Sources: [ZECTRIX development guide](https://wiki.zectrix.com/en/software/note4-development-guide), [Espressif ESP32-S3 read-flash documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/basic-commands.html#read-flash-contents).

## Build, flash, and monitor a project

If `idf.py --version` fails on macOS, install and activate a compatible environment using the [ESP-IDF 5.4 macOS setup](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/get-started/linux-macos-setup.html) and the activation notes in [firmware-development.md](firmware-development.md).

From the ESP-IDF project root:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

The reference project requires ESP-IDF 5.4+ and uses 115200 baud for monitoring. Exit IDF Monitor with `Ctrl+]`. `idf.py flash` writes the complete project layout; do not flash only an application binary unless its offset and compatibility are known.

Sources: [ZECTRIX quick start](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/QUICK_START.md), [ESP-IDF build and flash guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/start-project.html).

## Preserve settings during an application update

For an already installed custom app, prefer an application-only update when the partition layout and bootloader remain compatible. Compare the device partition table with the generated table, confirm the intended app partition and image size, and inspect the build's flash arguments. With OTA layouts, determine the correct slot and selection procedure rather than copying the single-factory-app example.

The `home-note4` update used a verified factory app partition at `0x10000`, size `0x300000`, with NVS at `0x9000`, size `0x6000`. Application-only flashes preserved Wi-Fi settings and battery history. Those offsets are evidence for that layout, not defaults for unknown firmware.

Use the project's `idf.py -p PORT app-flash` target when its generated arguments match the verified layout, or pass the verified app binary and offset to esptool. Never write an app-only binary at `0x0`. Avoid a merged image or whole-chip erase for a routine app update; a merged image can overwrite the settings region. A changed NVS schema also needs compatibility or migration even when flashing preserves its bytes.

Record the app version, commit, binary checksum, offset, and flash verification result. After boot, verify both the new behavior and retained settings. Check the installed esptool help: the tested 4.12.0 environment used names such as `write_flash` and `read_flash`, while newer documentation uses hyphenated names.

## macOS native USB monitor reset behavior

On the tested device, opening the serial port could restart the board or leave it awaiting a reset. For pyserial helpers, set DTR and RTS before opening:

```python
device = serial.Serial(port=None, baudrate=115200, timeout=0.2)
device.dtr = False
device.rts = False
device.port = verified_port
device.open()
```

If a restart is intended and the console remains silent, the tested esptool 4.12.0 workaround was `from esptool.reset import HardReset` followed by `HardReset(device, uses_usb=True)()`. This is a version-specific Python API, not a universal monitor requirement. Inspect the installed implementation before reuse. Close other serial clients, use a bounded read deadline, and remember that resetting restarts the application and affects battery observations.

## Enter ROM download mode

Most hosts connect without buttons. If detection fails:

1. Keep the USB data cable connected.
2. Hold the front circular BOOT / Voice button (GPIO0).
3. While holding it, press the recessed RESET pinhole once.
4. Release BOOT.
5. Reconnect the flashing tool and select the newly detected ESP32-S3 port.

This enters the ESP32-S3 ROM downloader. A normal restart returns to regular boot.

## Flash a merged release image

The official v1.0.0 monochrome demo release provides one merged binary written at address `0x0`:

| Field | Value |
|---|---|
| Filename | `zectrix-note4-epd-demo-v1.0.0.bin` |
| Address | `0x0` |
| SHA-256 | `a3649af0692987bd21ec815b224166c36bd91aa266657c7628a393cad1318072` |

Verify current release metadata and the downloaded checksum before flashing. This image is not compatible with NOTE4C and is not consumer firmware.

Source: [official v1.0.0 release](https://github.com/itopinion/zectrix-note4-epd-demo/releases/tag/v1.0.0).

## Browser updater

The [ZECTRIX firmware updater](https://zectrix.com/en/firmware-updater.html) uses Web Serial in desktop Chrome or Edge. Safari and Firefox are not supported. Its guided path:

1. Connect the USB device.
2. Choose a local `.bin` file.
3. Choose whether to preserve device settings; preserving is the default.
4. Review device, file, write address, and settings.
5. Install without disconnecting USB or sleeping the computer, then restart.

The default single-image address shown by the updater is `0x0000`. Advanced mode accepts multiple files and offsets. Use advanced mode only with a known firmware layout. Treat settings preservation cautiously with custom partition tables.

## Recovery order

If a newly flashed project does not boot:

1. Capture the serial log at 115200.
2. Verify the target is `esp32s3`, flash size is 16 MB, PSRAM is octal, and the whole project layout was flashed.
3. Enter ROM download mode manually.
4. Reflash a verified known-good project or compatible official merged image.
5. If no verified backup or batch-specific factory image exists, stop. Do not promise restoration of the original firmware.

Avoid erasing the entire chip as a generic first step because it destroys settings and recovery evidence.
