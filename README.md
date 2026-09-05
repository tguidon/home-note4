# Home Note4

An English dashboard for the monochrome ZECTRIX NOTE4, with weather for Wilton, Connecticut (06897), the date, Formula 1, and NFL scores. It runs directly on the ESP32-S3. No WeChat, ZECTRIX account, API key, or Mac server is required.

## Current status

The firmware builds with ESP-IDF **5.4.4** for **ESP32-S3, 16 MB flash, 8 MB octal PSRAM**. Host tests pass against captured API responses and failure cases. Screens have been rendered from the actual firmware drawing code and checked at 400 × 300.

**Installed on September 5, 2026.** The purchased NOTE4 DevKit documentation, factory board identifier, and USB chip information match the v1.0 N16R8 profile. Two full factory reads match byte for byte. Flash verification, dashboard boot, 8 MB PSRAM testing, RTC/NFC initialization, display-driver calls, and setup AP startup passed. The owner confirmed that the dashboard works after Wi-Fi setup. This confirms the normal setup and display path; invalid-password recovery, prolonged reconnection tests, and battery endurance remain unverified. Local installation records are in the ignored `dist/` and `backups/` directories.

## How it runs

The firmware is the application. C++ source, the hardware drivers, and ESP-IDF are compiled into native ESP32-S3 machine code. USB flashing writes this binary into the device's nonvolatile flash memory. At startup, the bootloader loads the application and starts its FreeRTOS tasks.

The main task handles buttons and draws the display. A background task fetches JSON directly from the weather and sports APIs over HTTPS. It validates each response and shares a snapshot with the display task. The device stores Wi-Fi settings separately in NVS, so no password is compiled into the application. During setup, the NOTE4 itself serves the small web page used to enter those settings.

Once configured, it operates without a computer. Code changes require a new build and USB flash; this version does not implement wireless firmware updates.

## Pages and controls

- Weather: temperature, conditions, feels-like temperature, daily high and low, wind, and rain probability for Wilton.
- F1: the next race in Eastern time, plus the latest race's top three finishers. This is a schedule/results feed, not live timing.
- NFL: four games per page, with live games first, upcoming games next, and completed games last. Scheduled games show local dates and times, not 0–0 scores.
- The date follows Eastern time, including daylight-saving changes.

| Button | Action |
| --- | --- |
| Side UP / DOWN | Previous / next page |
| Front OK | Request a refresh, limited to once per minute |
| Hold front OK about 1.5 seconds | Restart into Wi-Fi setup |
| Hold side DOWN about 3 seconds | Shut down; USB can keep the power rail energized |

## Wi-Fi setup after installation

1. Power the NOTE4 over USB.
2. Read the temporary `HomeNote4-XXXX` network name and its random password on the display.
3. Join that network from a phone or computer. Stay connected if it reports no internet.
4. Open **http://192.168.4.1** in a browser.
5. Enter your home **2.4 GHz** network name and password, then choose **Save and connect**.
6. Reconnect the phone or computer to your home network. The NOTE4 restarts, joins Wi-Fi, sets its clock, and loads the dashboard.

Setup accepts WPA2/WPA3 personal networks with an 8–63 byte password, or open networks with a blank password. Enterprise Wi-Fi and networks that require a browser login are not supported. If connection fails after 30 seconds, the setup screen returns with a new temporary password. Holding OK later also returns to setup.

Network details are saved in the device's local `home_note` NVS namespace. They are not embedded in the firmware or logged. This build does not enable flash encryption. Keep flash backups private because they may contain credentials. The setup server runs only in AP setup mode, behind its random WPA2 password; a per-session form token protects saving.

Internet access to NTP and the data providers is required. HTTPS certificate validation remains enabled. If the clock cannot synchronize, data requests wait. If a provider fails, the last successful data stays in RAM with a stale indicator and update timestamp; it is not retained across a reboot.

## Data sources

| Content | Source | Normal refresh |
| --- | --- | --- |
| Weather | [Open-Meteo](https://open-meteo.com/en/docs), CC BY 4.0, free noncommercial API | 15 minutes |
| F1 | [Jolpica F1 / Ergast-compatible API](https://api.jolpi.ca/ergast/) | 60 minutes |
| NFL | [ESPN public scoreboard endpoint](https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard) | 5 minutes |
| Clock | `pool.ntp.org`, `time.google.com` | SNTP |

ESPN's endpoint is publicly accessible but undocumented; it is not an open-source or guaranteed service. The ESPN adapter is isolated in `main/dashboard.cc` and `main/feeds.cc`. Requests have time and size limits. Empty schedules and unavailable results have explicit display states. Attribution appears on each page. Test fixtures capture provider data on September 5, 2026; they are test samples, not the running dashboard's data source.

Weather coordinates, units, and endpoints are in `main/feeds.cc`. Eastern timezone rules are in `main/dashboard.h`.

## Build

Install and activate [ESP-IDF 5.4.4](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/get-started/linux-macos-setup.html), then run:

```sh
idf.py set-target esp32s3
idf.py build
bash tools/test-host.sh
idf.py merge-bin -o home-note4-merged.bin
```

On the original development Mac, the ignored `tools/activate-local.sh` helper activates its local toolchain. On another computer, activate your own ESP-IDF installation as described above.

```sh
source tools/activate-local.sh
idf.py build
bash tools/test-host.sh
```

`build/home_note4.bin` is an **application-only** image at offset `0x10000`. It must not be written to `0x0`.

`build/home-note4-merged.bin` includes the bootloader, partition table, and application. Its write offset is `0x0`. The merged image fills gaps, including the NVS region, with erased bytes, so installation replaces settings. The 3 MiB application partition has ample room for the approximately 1.1 MiB application.

## Before first installation

Confirm the black-and-white NOTE4 PCB v1.0 mapping, ESP32-S3 N16R8 memory, and the correct USB port. Do not use these images on NOTE4C or an unverified board revision.

Read the device's security state and full flash before any write. Preserve a **16,777,216-byte** factory backup, then perform a second read and compare hashes. Do not infer that a backup guarantees factory recovery: the device's security configuration and the batch-specific recovery procedure must also be checked. The original development device has two matching 16 MiB backups of factory firmware 3.6.2 under the ignored `backups/` directory. Their checksum and original partition layout are recorded alongside them. Factory restoration has not been tested.

After those checks, install the verified merged image using the matched serial port and esptool's syntax for the installed version. Do not use an erase-all command. Capture a 115200-baud serial log and validate:

- USB boot and 8 MB PSRAM detection.
- Readable English display and all three buttons.
- Setup from a phone, invalid-password recovery, and successful Wi-Fi connection.
- Clock sync, real weather, F1, and all NFL pages.
- Wi-Fi reconnection and clear stale-data behavior.
- Panel power off between refreshes and battery shutdown with USB disconnected.

## Scope of this first version

This is a USB desk dashboard. It uses Wi-Fi modem power saving and powers off the e-paper rail between full 1bpp refreshes, but it does not schedule automatic deep sleep. Battery endurance is unmeasured. No OTA update path is configured; subsequent firmware updates use USB. Audio and NFC are not application features in this version.

The board and display drivers retain the official upstream mapping and waveform. The application asserts the board power latch before networking, serializes display access on the main task, and uses a separate data-fetch task with protected snapshots. Full refreshes avoid partial-refresh state and ghosting complications during the first hardware tests.

## Code map

- `main/app_main.cc`: startup, screen updates, buttons, and shutdown.
- `main/network.cc`, `main/setup.html`: Wi-Fi storage, local setup, SNTP, and bounded HTTPS.
- `main/feeds.cc`: source URLs, polling, and snapshot synchronization.
- `main/dashboard.cc`: response validation, score ordering, and timezone conversion.
- `main/render.cc`: the 400 × 300 screen layout.
- `tests/`: data, failure, timezone, credential, and rendering checks. Host tests use AddressSanitizer and UndefinedBehaviorSanitizer.

Based on [itopinion/zectrix-note4-epd-demo](https://github.com/itopinion/zectrix-note4-epd-demo). Its MIT license and third-party notices remain in the repository. The original README is preserved in `docs/UPSTREAM_README.md`.
