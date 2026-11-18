---
name: develop-zectrix-note4
description: Build, modify, deploy, and troubleshoot ESP-IDF firmware for the monochrome ZECTRIX NOTE4 Developer Kit PCB v1.0. Use for SSD2683 display code, image assets, buttons, power, battery and charging, RTC, NFC, audio, Wi-Fi, flashing, recovery, or hardware tests. Do not use for NOTE4C or undocumented consumer firmware and cloud services.
---

# Develop for the Zectrix NOTE4

Support the monochrome ZECTRIX NOTE4 Developer Kit as an ESP32-S3 embedded target. Use the local references for a fast baseline and inspect the current project or upstream source when exact APIs may have changed.

## Establish the hardware boundary

Before any flash operation, confirm all of these facts:

- The device is the black-and-white NOTE4 Developer Kit, not NOTE4C.
- The PCB is version 1.0 unless the project provides a verified mapping for another revision.
- The target is ESP32-S3 with 16 MB flash and 8 MB octal PSRAM.
- The selected serial port belongs to this device.

Do not treat the reference demo as the consumer NOTE4 firmware. A custom firmware flash replaces the installed firmware. Keep a verified full-flash backup before the first custom flash or any partition-layout change.

## Inspect the project

Read repository instructions and the current Git status. Identify whether the project is:

- A clone or fork of `itopinion/zectrix-note4-epd-demo`.
- A separate ESP-IDF 5.4+ project that imports selected Note4 components.
- An Arduino or PlatformIO experiment. These frameworks are not covered by the official Note4 development material; state that boundary and adapt only when the user explicitly requests it.

Treat the project's checked-in component headers, `sdkconfig.defaults`, partition table, and board configuration as authoritative for that checkout. Do not overwrite unrelated user changes.

## Load task guidance

| Task | Read |
|---|---|
| Identify the board, pins, controls, schematic, voltage, or power behavior | [hardware.md](references/hardware.md) |
| Set up ESP-IDF, understand the reference project, or write board/peripheral code | [firmware-development.md](references/firmware-development.md) |
| Draw to the SSD2683 display, use partial refresh, or convert image assets | [display-and-assets.md](references/display-and-assets.md) |
| Back up, build, flash, monitor, use the browser updater, or enter download mode | [deployment-and-recovery.md](references/deployment-and-recovery.md) |
| Diagnose failures or validate real hardware | [validation-and-troubleshooting.md](references/validation-and-troubleshooting.md) |
| Build a connected dashboard, keep navigation responsive, handle large feeds, or record battery history | [connected-apps.md](references/connected-apps.md) |
| Check upstream pages, provenance, versions, or documentation gaps | [sources.md](references/sources.md) |

Read more than one reference when the task crosses boundaries. Do not load unrelated references.

## Preserve device invariants

- Assert the GPIO17 battery power latch early when the firmware must stay alive on battery.
- Use 3.3 V logic only. Never apply 5 V directly to a GPIO.
- Keep the e-paper rail off when idle. Power the panel through its driver lifecycle.
- Establish a successful full 1 bpp frame once per boot or new driver handle before partial updates.
- After 4 bpp output, re-establish a full 1 bpp base before partial updates.
- After a partial-refresh timeout, power the panel off and on before the full 1 bpp rebase.
- Periodically replace partial updates with a full 1 bpp refresh to control ghosting.
- Serialize shared I2C access among the RTC, NFC, and audio control path.
- Keep USB-powered deep sleep distinct from battery-latch shutdown; USB can keep the rail energized.
- Preserve NFC user data when tests or experiments temporarily write an NDEF record.

## Implement and verify

Prefer the official reference components over rewriting the SSD2683 waveform or board support from memory. Keep hardware-specific pins behind a board configuration boundary.

For code changes:

1. Build with the project's supported ESP-IDF version and target `esp32s3`.
2. Review configuration for 16 MB flash, octal PSRAM, and the intended partition layout.
3. Run host-side or compile-time checks first.
4. Flash only when the user requested deployment and the hardware identity, port, backup, and image layout are known.
5. Monitor at the project baud rate; the reference project uses 115200.
6. Validate affected peripherals on real hardware using the relevant acceptance criteria.

Do not claim hardware success from a compile alone. Separate verified results from steps the user still needs to run on the device.

## Use current sources deliberately

The bundled references summarize first-party material reviewed on 2026-09-04, supplemented by explicitly labeled application observations from 2026-09-05. When internet access is available, verify current release tags, checksums, supported ESP-IDF versions, browser-updater behavior, and APIs before giving version-sensitive instructions. Prefer ZECTRIX, the official reference repository, component manufacturers, and Espressif documentation.

## Finish

Report:

- The hardware profile and framework used.
- Files changed and features implemented.
- Build, flash, monitor, and physical-test results separately.
- Any unverified hardware assumptions or remaining device steps.
- Whether the factory backup and recovery path are available.
