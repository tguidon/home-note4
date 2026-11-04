# Home Note4

Home Note4 is a personal dashboard for the monochrome ZECTRIX NOTE4 Developer Kit, PCB v1.0.
It runs on the ESP32-S3 and uses the 400 × 300 e-paper display.

The dashboard has three sections: **Home → F1 → NFL**.
Home shows weather for Wilton, Connecticut (06897), and an optional private Google Calendar agenda.
Dates and event times use Eastern time, including daylight-saving changes.

The device fetches data directly over Wi-Fi.
It needs no computer server, ZECTRIX account, WeChat account, or API key.
Firmware changes require a build and USB installation.
There is no wireless firmware update feature.

## Current firmware

Version **0.7.0** uses ESP-IDF **5.4.4** with **16 MB flash** and **8 MB octal PSRAM**.
The October 25, 2026 installation preserved saved Wi-Fi, the calendar link, and battery history.
Weather, calendar, F1, NFL, and USB navigation passed the installation checks.
All six host test suites passed with AddressSanitizer and UndefinedBehaviorSanitizer.

The owner added a private calendar through setup and saw its events on the device.
Physical button behavior after sleep, charger completion, and battery endurance still need observation on the device.
A successful build or display-driver call does not establish these results.

## Screens and controls

| Screen | Content |
| --- | --- |
| Home | Current weather, tomorrow's high and low temperatures, rain probability, and up to three calendar events |
| F1 | The next main race and the latest published session results, four drivers per page |
| NFL | Four games per page, with live games first, upcoming games next, and completed games last |

Home stays on one page.
Ended events disappear, and later events fill the available rows.
All-day and overnight events remain until they end.
The footer shows the last successful refresh time.

F1 keeps the main race at the top throughout its Eastern calendar day.
At midnight, the race heading advances to the next scheduled Grand Prix.
Session results show their own Grand Prix name and date.
Published qualifying results can differ from the final starting grid after penalties.
F1 results are not live timing.

| Button | Action |
| --- | --- |
| Side UP / DOWN | Change sections in the Home → F1 → NFL sequence |
| Front OK | Advance through results pages, or request a refresh on a single-page section |
| Hold front OK for about 1.5 seconds | Restart into Wi-Fi and calendar setup |
| Hold side DOWN for about 3 seconds | Shut down the device |

Manual refresh has a one-minute limit.
USB can keep the device powered after shutdown.

A tap produces a 120 ms LED pulse.
Navigation uses cached data and does not wait for network requests.
Rapid taps replace pending frames, so the display does not replay every intermediate page.
Full e-paper refreshes take time.
The firmware switches off panel power after each refresh.

## Configure Wi-Fi

1. Power the NOTE4 through USB.
2. Read the `HomeNote4-XXXX` network name and password on the display.
3. Join that network from a phone or computer.
4. If the phone reports no internet, keep the connection.
5. Open **http://192.168.4.1** in a browser.
6. Select **Set up a Wi-Fi network**.
7. Enter the name and password for your home **2.4 GHz** network.
8. Select **Save and restart**.
9. Reconnect the phone or computer to your home network.

The NOTE4 restarts, connects to Wi-Fi, sets its clock, and fetches dashboard data.
The setup network keeps the same device-specific name and saved password across setup sessions.
Each session uses a new form token.

The device supports WPA2/WPA3 personal networks with an 8–63 byte password.
It also supports open networks with a blank password.
It does not support enterprise Wi-Fi or networks that require a browser login.

If the connection fails after 30 seconds, the dashboard opens offline and retries.
The device keeps the saved credentials.

To change the saved Wi-Fi configuration:

1. Hold front OK for about 1.5 seconds.
2. Repeat the Wi-Fi procedure.

## Add a private Google Calendar

Before you connect to the NOTE4 setup network:

1. Open Google Calendar settings on a phone or computer with internet access.
2. Select the calendar.
3. Open **Integrate calendar**.
4. Copy the **Secret address in iCal format**.

The desktop Google Calendar website exposes this setting.
[Google's instructions](https://support.google.com/calendar/answer/37648?hl=en) describe calendar synchronization.
The secret address gives read-only access without public calendar sharing.

To save the calendar link:

1. Hold front OK for about 1.5 seconds.
2. Join the `HomeNote4-XXXX` network with the password on the display.
3. If the phone reports no internet, keep the connection.
4. Open **http://192.168.4.1**.
5. Keep **Keep saved Wi-Fi** selected.
6. Under **Calendar**, select **Add or replace a calendar**.
7. Paste the secret address.
8. Select **Save and restart**.
9. Reconnect the phone or computer to your home network.

The NOTE4 connects directly to Google and fetches events.
The calendar is optional.
The setup page can keep, replace, or remove the saved calendar link.
Removal clears the device agenda after restart and leaves Google events unchanged.

### Calendar limits

The parser expands events over a 90-day window.
It supports all-day events, common recurrence rules, and changes to individual events.
It handles UTC, Eastern time, and embedded time-zone definitions with supported transitions.
Host tests cover US and European daylight-saving changes.

The parser does not implement all of RFC 5545.
Unsupported recurrence rules or time zones produce a warning, and the device omits those events.
Downloads have a 3 MiB limit.
The parser also limits event components to 2,000 and expanded occurrences to 1,024.
A download or structural error preserves the previous agenda.
Google controls how soon event changes appear in its feed.

If the device shows a calendar warning, use Google Calendar to see the complete agenda.

## Privacy and offline behavior

The device saves Wi-Fi credentials and the calendar link in local nonvolatile storage (NVS).
The firmware image contains neither secret.
The setup page contains no third-party scripts and does not return the saved calendar link to the browser.
The setup network uses WPA2 protection.
The device uses HTTPS certificate validation for data requests.

This firmware does not enable flash encryption.
A flash backup can contain Wi-Fi credentials and the private calendar link.
Anyone with the calendar link can read its events.

- Enter the calendar link directly into the local setup page.
- Keep credentials and calendar links out of Git, chat, screenshots, and logs.
- Keep flash backups private.

The clock must synchronize before data requests start.
If a provider request fails, the device keeps the last successful data in RAM and shows a stale indicator.
Dashboard data does not survive a restart.
Saved credentials, battery history, and power diagnostics do survive a restart.

## Data sources and refresh intervals

| Content | Source | Normal refresh |
| --- | --- | --- |
| Calendar | Your private Google Calendar iCal feed | Every 5 minutes and after local midnight |
| Weather | [Open-Meteo](https://open-meteo.com/en/docs) | Every 15 minutes |
| F1 | [Jolpica F1 alpha API](https://api.jolpi.ca/docs/) | Daily on quiet days, hourly before sessions, every 5 minutes near sessions or pending results |
| NFL | [ESPN scoreboard endpoint](https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard) | Daily on quiet days, hourly before games, every 5 minutes near or during games |
| Clock | `pool.ntp.org`, `time.google.com` | SNTP synchronization |

Failed requests use increasing retry delays, up to one hour.
F1 keeps prior session results until the provider publishes newer results.
The Jolpica alpha API does not guarantee publication time or final classifications.
The ESPN endpoint is public but undocumented and has no service guarantee.
Test fixtures contain sample responses, and the device fetches its own data.

Weather coordinates, units, and source URLs are in [main/feeds.cc](main/feeds.cc).
The timezone definition is in [main/dashboard.h](main/dashboard.h).
These values require a firmware change.
The setup page configures Wi-Fi and the calendar link.

## Build and host tests

1. Install [ESP-IDF 5.4.4](https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/get-started/linux-macos-setup.html).
2. Activate the ESP-IDF environment.
3. From the repository root, run these commands in order:

```sh
idf.py set-target esp32s3
idf.py build
bash tools/test-host.sh
idf.py merge-bin -o home-note4-merged.bin
```

The host script runs six suites with AddressSanitizer and UndefinedBehaviorSanitizer.
The suites cover feed parsing, calendar recurrence, navigation, battery history, refresh policy, and power diagnostics.
The script also creates screen previews from the firmware drawing code in `build-host/previews/`.

The ignored `tools/activate-local.sh` helper contains paths for the original development Mac.
It is not part of a fresh clone.

On that Mac, run:

```sh
source tools/activate-local.sh
idf.py build
bash tools/test-host.sh
```

## USB installation

The supported hardware is the **monochrome NOTE4 Developer Kit PCB v1.0**, with an ESP32-S3 N16R8.
These images do not target the NOTE4C or other board revisions.

| Image | Contents | Write offset |
| --- | --- | --- |
| `build/home_note4.bin` | Application only | `0x10000` |
| `build/home-note4-merged.bin` | Bootloader, partition table, and application | `0x0` |

**CAUTION: Do not write the application-only image at `0x0`.**
This write overwrites the bootloader.

**CAUTION: Export battery history and power diagnostics before a firmware update.**
The merged image overwrites NVS, including saved credentials and diagnostic history.
An application-only update preserves NVS with the existing partition layout.

Before the first installation:

1. Make sure that the board revision, memory, and USB port match the supported hardware.
2. Read the device security state.
3. Save a full **16,777,216-byte** flash backup.
4. Read the full flash a second time.
5. Make sure that both backup hashes match.
6. Make sure that the recovery procedure matches the device security state and hardware batch.

The original development device has two matching factory backups in the ignored `backups/` directory.
Factory restoration remains untested.
A matching backup alone does not establish recovery.

To install the firmware:

1. Make sure that the device partition table matches the image layout.
2. Write the selected image at its listed offset with esptool syntax for the installed version.
3. Capture a 115200-baud serial log.
4. Make sure that startup detects 8 MB PSRAM and initializes the board.
5. Make sure that Wi-Fi setup, clock synchronization, and all three dashboard sections operate.
6. Make sure that all three buttons operate after an idle period.
7. Make sure that Wi-Fi reconnects after a connection failure.
8. With USB disconnected, make sure that the DOWN hold shuts down battery power.

Do not use an erase-all command.

## Battery and power

The firmware uses frequency scaling, automatic light sleep, and Wi-Fi modem power saving.
USB, setup, and display refreshes prevent automatic sleep during those operations.
Audio, NFC, and RTC supplies remain enabled.
Panel power switches off between refreshes.
There is no automatic deep-sleep schedule or scheduled Wi-Fi shutdown.

The battery label is an estimate from a provisional voltage curve.
Charging uses the same curve, with a 99% limit.
Only the charger completion signal produces `100% FULL`.
Charging voltage can inflate the estimate.

After USB disconnection, `SETTLING` appears for 30 seconds before the smoothed estimate.
`--%` means that no valid estimate is available.
Percentage changes appear on the next normal redraw.
Power-state changes request a redraw.
The raw CSV percentage keeps the original board formula for comparison across firmware versions.

The initial endurance target was 3–4 days.
The September trial retained 5.31 uninterrupted days of battery samples within 8.22 days of boot uptime.
Earlier records rotated out, so the exact discharge duration is unknown.
The October boot uptime included charging and does not establish continuous battery endurance.
Evidence and limits appear in the [September battery review](docs/battery-review-2026-09-15.md) and [October charging review](docs/battery-review-2026-10-25.md).

## Export local diagnostics

The device saves a battery sample every 30 minutes, at startup, on power transitions, and before button shutdown.
The history holds 512 readings and replaces the oldest readings as new ones arrive.
Regular samples alone cover about ten days and sixteen hours.
Restarts and power transitions shorten that period.
The history contains no credentials or calendar data, and the device never uploads it.

Power diagnostics contain reset information, sleep attempts, network counters, and display counters.
They also record the time, voltage, and battery label of the last successful frame.
Sleep counters do not measure electrical current.
A successful display-driver return does not prove visible output.

To export diagnostics:

1. Close any serial monitor.
2. Activate the ESP-IDF environment.
3. Replace each example port with the current device port.
4. Use a new output filename for each export.
5. Run the battery exporter:

```sh
python tools/export-battery.py --port /dev/cu.usbmodem2101 --output battery-2026-09-05.csv
```

6. Run the power exporter:

```sh
python tools/export-power.py --port /dev/cu.usbmodem101 --output power-2026-09-07.local.json
```

The exporters save battery CSV or diagnostic JSON without raw application logs.
Opening the USB port can restart this board on macOS.
Saved history survives that restart.

### USB commands

At 115200 baud, the local serial console accepts these commands followed by Enter:

| Command | Action |
| --- | --- |
| `next` / `previous` | Change sections through the button action handler |
| `more` | Apply the front OK action |
| `refresh` | Request a background refresh with the one-minute limit |
| `battery` | Export saved battery readings |
| `power` | Export power diagnostics |

## Code map

| Files | Responsibility |
| --- | --- |
| `main/app_main.cc`, `main/app_events.cc` | Startup, buttons, navigation, and shutdown |
| `main/network.cc`, `main/setup.html`, `main/settings.cc` | Wi-Fi, local setup, saved configuration, clock synchronization, and HTTPS |
| `main/feeds.cc`, `main/refresh_policy.cc` | Data requests, refresh intervals, and shared snapshots |
| `main/dashboard.cc`, `main/formula_one.cc` | Weather and sports parsing, result order, and race dates |
| `main/calendar.cc`, `main/calendar_rules.cc` | iCalendar parsing, recurrence, time zones, and agenda selection |
| `main/render.cc`, `main/display_worker.cc` | Screen layouts and panel refreshes |
| `main/battery_history.cc`, `main/battery_estimate.h`, `main/battery_indicator.h` | Battery history and the on-screen estimate |
| `main/power_manager.cc` | Sleep policy and power diagnostics |
| `components/zectrix_board/`, `components/zectrix_epd/` | Board and display drivers |
| `tests/` | Host suites, provider fixtures, and screen previews |

## Project origin

Home Note4 started from [itopinion/zectrix-note4-epd-demo](https://github.com/itopinion/zectrix-note4-epd-demo).
It keeps the upstream board mapping and display waveform.
The active application is the Home Note4 dashboard.
Some component names and reference documents still describe the original demo.

The [upstream README](docs/UPSTREAM_README.md) preserves that project description.
The MIT license and [third-party notices](THIRD_PARTY_NOTICES.md) remain in the repository.
