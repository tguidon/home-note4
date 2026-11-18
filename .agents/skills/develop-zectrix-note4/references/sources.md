# Source Map and Documentation Boundary

This is a transformed index of first-party material reviewed on 2026-09-04. Follow links for current details; do not assume the snapshot supersedes upstream.

## Entry page and first-level sources

| Source | What it covers |
|---|---|
| [NOTE4 Developer Kit development guide](https://wiki.zectrix.com/en/software/note4-development-guide) | Hardware identity, controls, pin map, schematic, ESP-IDF baseline, prebuilt image, component links, and support boundary |
| [Tindie product listing](https://www.tindie.com/products/zectrix/zectrix-note4-e-paper-devkit/) | Purchased developer-edition identity, included hardware, reference firmware, and batch caveat |
| [NOTE4 Developer Kit product page](https://zectrix.com/en/note4-developer-kit.html) | Product positioning, supported capabilities, and developer-edition boundary |
| [Official reference repository](https://github.com/itopinion/zectrix-note4-epd-demo) | MIT-licensed source, components, demo application, tools, and embedded assets |
| [Reference quick start](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/QUICK_START.md) | ESP-IDF setup, build/flash/monitor, first run, configuration, and common failures |
| [Hardware mapping](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/HARDWARE.md) | GPIOs, power behavior, and revision-porting checklist |
| [Reference release v1.0.0](https://github.com/itopinion/zectrix-note4-epd-demo/releases/tag/v1.0.0) | Merged binary, target, flash address, and checksum |
| [ZECTRIX firmware updater](https://zectrix.com/en/firmware-updater.html) | Web Serial installation, settings preservation, advanced offsets, serial monitor, and manual download mode |
| [NOTE4 v1.0 schematic](https://wiki.zectrix.com/sch_zectrix_note4_developer_kit_v1.0_en.pdf) | Board-level electrical connections |
| [ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) | MCU electrical and peripheral reference |
| [PCF8563 datasheet](https://www.nxp.com/docs/en/data-sheet/PCF8563.pdf) | RTC registers and behavior |
| [ES8311 product brief](https://www.everest-semi.com/pdf/ES8311%20PB.pdf) | Codec capabilities |
| [Espressif ES8311 component](https://components.espressif.com/components/espressif/es8311/versions/1.0.0?language=en) | Supported ESP-IDF codec component |

## One-level coding sources

These pages are linked by the entry page, repository README, or the closely related official guides and materially affect development:

| Source | What it covers |
|---|---|
| [Detailed ESP32-S3 e-paper guide](https://wiki.zectrix.com/en/guides/esp32-s3-e-paper-display-guide) | Refresh state, buffer formats, lifecycle, asset conversion, and display troubleshooting |
| [Open-source resources and roadmap](https://wiki.zectrix.com/en/software/opensource) | Public-source, licensing, commercial-firmware, and cloud boundaries |
| [EPD API](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/EPD_API.md) | Driver lifecycle, pixel formats, partial/full refresh, errors, and SPI ownership |
| [Hardware test criteria](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/TEST_CRITERIA.md) | Acceptance criteria and operator setup for seven peripherals |
| [UI flow](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/UI_FLOW.md) | Demo navigation, refresh cadence, display scenes, and shutdown order |
| [Board public API](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/components/zectrix_board/include/zectrix_board.h) | Board lifecycle, buttons, peripherals, power snapshot, LED/audio/latch controls |
| [Board pin configuration](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/components/zectrix_board/include/zectrix_board_config.h) | Compile-time pin and address constants |
| [EPD public API header](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/components/zectrix_epd/include/zectrix_epd.h) | Exact display types, constants, and functions |
| [EPD driver implementation](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/components/zectrix_epd/zectrix_epd.cc) | Controller readiness, shadow lifetime, timeout invalidation, and waveform implementation |
| [Project defaults](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/sdkconfig.defaults) | Flash, PSRAM, partition, and codec build settings |

The detailed display guide was prepared against commit `ca285c98ed0641f86780edb1f5ec77b0335fe649` and release `v1.0.0` dated 2026-08-07. Pin reproducible work to a tag or commit and re-check later revisions.

## Additional authoritative deployment references

- [ESP-IDF ESP32-S3 build, flash, and monitor](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/start-project.html)
- [ESP-IDF 5.4 Linux and macOS setup](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/get-started/linux-macos-setup.html)
- [ESP32-S3 boot-mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html)
- [ESP32-S3 USB Serial/JTAG console](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/usb-serial-jtag-console.html)
- [ESP32-S3 esptool basic commands](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/basic-commands.html)

## Known documentation gaps

- ZECTRIX recommends a complete 16 MB backup but publishes neither exact Note4 backup/restore steps nor a factory image.
- Hardware can vary by batch. Confirm model, PCB revision, panel type, and firmware identity before flashing.
- ZECTRIX does not link a public manufacturer SSD2683 datasheet. Use the official driver and schematic.
- The official path covers ESP-IDF 5.4+; it does not document Arduino or PlatformIO integration.
- The EPD API is documented and C-compatible; the broader C++ board adapter is source-level reference rather than a separately versioned public API.
- Full-refresh cadence depends on content, temperature, waveform, and acceptable artifacts.
- Battery percentage is an estimate; public prose does not publish a calibrated fuel-gauge model.
- Browser settings preservation is not guaranteed to match every custom partition layout.
- The public demo is not the consumer NOTE4 firmware or cloud service source.
- Button meanings beyond electrical mapping are application-defined.

When upstream sources disagree, prefer the source closest to the implementation: checked-out public headers and configuration for code, the schematic for electrical connections, release metadata for binaries/checksums, and the current ZECTRIX guide for hardware scope.
