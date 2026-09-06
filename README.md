# Home Note4

An English dashboard for the monochrome ZECTRIX NOTE4, with weather for Wilton, Connecticut (06897), the date, an optional private Google Calendar agenda, US Open tennis, Formula 1, and NFL scores. It runs directly on the ESP32-S3. No WeChat, ZECTRIX account, API key, or Mac server is required.

## Current status

The firmware builds with ESP-IDF **5.4.4** for **ESP32-S3, 16 MB flash, 8 MB octal PSRAM**. Host tests pass against captured API responses and failure cases. Screens have been rendered from the actual firmware drawing code and checked at 400 × 300.

**Installed on September 5, 2026.** The purchased NOTE4 DevKit documentation, factory board identifier, and USB chip information match the v1.0 N16R8 profile. Two full factory reads match byte for byte. Flash verification, dashboard boot, 8 MB PSRAM testing, RTC/NFC initialization, display-driver calls, and setup AP startup passed. The owner confirmed that the dashboard works after Wi-Fi setup. This confirms the normal setup and display path; invalid-password recovery, prolonged reconnection tests, and battery endurance remain unverified. Local installation records are in the ignored `dist/` and `backups/` directories.

**Version 0.2.0 device verification:** two consecutive US Open fetches each parsed 40 matches; NFL loaded 16 upcoming games. Eleven full-frame updates completed successfully in 1,292–1,318 ms. Rapid navigation and paging were exercised through the local USB commands that use the button action handler. Wi-Fi reconnected and all six previously exported battery records survived; the log now contains 15 records. Visual inspection of this updated screen and physical button timing remain owner checks. The factory backup remains available.


**Version 0.3.0 device verification:** the F1 feed loaded all 22 Monza qualifying positions with the Italian Grand Prix on September 6 at 09:00 EDT as its anchor. All six classification pages and wraparound were exercised through USB. Thirteen display updates returned ESP_OK in 1,295–1,320 ms. Weather, 16 NFL games, and 40 US Open matches also loaded. All 15 previously exported battery readings survived; 17 are now available. Host sanitizer tests cover all seven session types, missing/malformed feeds, race-day retention, Eastern midnight rollover, and cold starts. Screens were checked using previews from the firmware renderer; physical inspection of this F1 update remains an owner check.

**Version 0.4.0 device verification:** the application-only flash passed hash verification and booted as 0.4.0 on the same ESP32-S3 with 8 MB PSRAM. Saved Wi-Fi reconnected; weather, F1 (22 drivers), NFL (16 games), and US Open (40 matches) loaded. Nine display updates returned ESP_OK in 1,294–1,318 ms; USB navigation returned to the new home screen. All 20 pre-update battery records survived; 23 were exported afterward. Calendar/settings sanitizer tests and a local browser save test passed with invented data. The owner's secret link has not been entered, so a real private calendar fetch and the new setup form on the physical device remain owner checks. The factory backup remains available; restoration has not been tested.

## How it runs

The firmware is the application. C++ source, the hardware drivers, and ESP-IDF are compiled into native ESP32-S3 machine code. USB flashing writes this binary into the device's nonvolatile flash memory. At startup, the bootloader loads the application and starts its FreeRTOS tasks.

The main task handles buttons and renders cached pages. A dedicated display worker refreshes the panel without blocking input. A background task fetches JSON from the weather and sports APIs and iCalendar data directly from Google over HTTPS. It validates each response and shares a snapshot with the display task. The device stores Wi-Fi settings separately in NVS, so no password is compiled into the application. During setup, the NOTE4 itself serves the small web page used to enter those settings.

Once configured, it operates without a computer. Code changes require a new build and USB flash; this version does not implement wireless firmware updates.

## Pages and controls

- Home: Wilton weather above your calendar. The agenda shows every event overlapping today, including all-day and overnight events. When today is empty, it shows upcoming events within 90 days. Three events fit per page; OK pages through the list. Weather stays visible on each agenda page. Times use Eastern time. Past appointments stay visible for the rest of their day and are marked Ended.
- US Open: today's singles and doubles, three matches per page. Live matches come first, then scheduled matches and results. Set scores appear beside each player/team; an asterisk marks the winner. MS/WS mean men's/women's singles; MD/WD mean men's/women's doubles; XD means mixed doubles.
- F1: the main Grand Prix name and start time stay at the top of every page. The latest published practice, sprint qualifying, sprint race, qualifying, or race classification appears below, four drivers per page. OK pages through every driver. Positions and times come from the provider; qualifying order can differ from the final starting grid after penalties.
- NFL: four games per page, with live games first, upcoming games next, and completed games last. Scheduled games show local dates and times, not 0–0 scores.
- The date follows Eastern time, including daylight-saving changes.
- Battery percentage appears beside ONLINE/OFFLINE on every dashboard page. It uses the official board driver's voltage-based estimate and updates with each screen redraw. `--%` means the battery reading is unavailable; charging and load can affect the estimate.

| Button | Action |
| --- | --- |
| Side UP / DOWN | Switch sections: Home → US Open → F1 → NFL |
| Front OK | Next agenda/results/match page; request a refresh on single-page sections, limited to once per minute |
| Hold front OK about 1.5 seconds | Restart into Wi-Fi and calendar setup |
| Hold side DOWN about 3 seconds | Shut down; USB can keep the power rail energized |

Navigation acknowledges taps with a 120 ms LED pulse. There is no three-second artificial redraw delay. The display worker has one pending frame: rapid taps replace obsolete frames rather than replaying every intermediate screen. Page changes use cached RAM data and never wait for HTTP. Empty pages show a loading state; cached pages keep their contents while feeds update. The e-paper waveform still takes time, so a page change cannot be instantaneous like an LCD. Full 1bpp refreshes keep text clean; panel power is removed after each refresh.

The tennis query follows the New York calendar day, including a still-live match that crosses midnight. After the tournament's published end time, its page becomes **US Open is over**, with Weather/F1/NFL still available on Up/Down. A valid empty schedule is distinct from a failed request. Cached dates and stale live states are labelled explicitly. The feed's event dates drive the transition; the published 2026 end date is also a fallback after an off-season reboot. Future event dates come from the feed, not a guessed annual schedule.

The F1 race anchor follows the New York calendar day. It shows the main race throughout race day, even after the start or finish. At midnight Eastern, it advances to the next scheduled Grand Prix. The latest results keep their own Grand Prix name and session date below that anchor, so last weekend's results are not confused with the next race. This uses actual race timestamps, including Saturday races and daylight-saving changes. A new session result returns the F1 section to its first page.

F1 waits for published classifications and keeps the prior session while newer results are missing. It checks at most eight recent sessions during a cold-start lookup. Failed requests preserve cached data with a stale indicator. The provider's alpha endpoint has no completion/finality flag or guaranteed publication delay: these are published results, not live timing or a certified final grid. Schedules and results are validated separately. The next season is checked after the last scheduled race; before the first session of a new year, the prior season is checked for results. An unavailable future calendar shows “Next race schedule pending.” The date-based anchor also advances from cached schedule data while offline.

## Wi-Fi setup after installation

1. Power the NOTE4 over USB.
2. Read the temporary `HomeNote4-XXXX` network name and its random password on the display.
3. Join that network from a phone or computer. Stay connected if it reports no internet.
4. Open **http://192.168.4.1** in a browser.
5. Select **Set up a Wi-Fi network**, enter your home **2.4 GHz** network name and password, then choose **Save and restart**.
6. Reconnect the phone or computer to your home network. The NOTE4 restarts, joins Wi-Fi, sets its clock, and loads the dashboard.

Setup accepts WPA2/WPA3 personal networks with an 8–63 byte password, or open networks with a blank password. Enterprise Wi-Fi and networks that require a browser login are not supported. If connection fails after 30 seconds, the setup screen returns with a new temporary password. Holding OK later also returns to setup.

Network details and the optional calendar link are saved in the device's local `home_note` NVS namespace. A versioned settings value keeps them together; existing Wi-Fi settings migrate when settings are next saved. They are not embedded in the firmware or logged. This build does not enable flash encryption. Keep flash backups private because they may contain credentials. The setup server runs only in AP setup mode, behind its random WPA2 password; a per-session form token protects saving.

Internet access to NTP and the data providers is required. HTTPS certificate validation remains enabled. If the clock cannot synchronize, data requests wait. If a provider fails, the last successful data stays in RAM with a stale indicator and update timestamp; it is not retained across a reboot.

## Add a private Google Calendar

1. Copy the calendar's **Secret address in iCal format** while your phone still has internet access. In Google Calendar settings, select the calendar, then **Integrate calendar**. Google may require its desktop website to expose this setting. Do not use the public address or enable public sharing. [Google's instructions](https://support.google.com/calendar/answer/37648?hl=en).
2. Hold the NOTE4's front OK button for about 1.5 seconds to enter setup.
3. On your phone, join the temporary `HomeNote4-XXXX` Wi-Fi using the password shown on the NOTE4. Stay connected despite the no-internet notice.
4. Open **http://192.168.4.1**. Keep **Keep saved Wi-Fi** selected.
5. Under Calendar, choose **Add or replace a calendar**, paste the secret link, and tap **Save and restart**.
6. Return your phone to home Wi-Fi. The NOTE4 reconnects, synchronizes its clock, and fetches your events.

The calendar stays private and read-only. Anyone with the secret link can read it. Enter the link directly into the device's setup page; keep it out of chat, Git, screenshots, and logs. The page contains no third-party scripts or assets and never sends the saved link back to the browser. The link travels to the local device over its password-protected setup Wi-Fi and is used only for a certificate-verified HTTPS request to `calendar.google.com`. It is stored in NVS, not the firmware image. Physical flash access can read it because this device does not enable flash encryption.

To change calendars or remove the link, repeat setup and select the relevant Calendar option. Removing it clears the RAM agenda on restart and does not change Google events. **Keep current calendar setting** preserves the link when you change Wi-Fi. Saving a new link checks its private Google URL format; the device fetches and validates the feed after restarting. A revoked or inaccessible link shows an unavailable state and retries.

Calendar requests run every five minutes and after local midnight. Feed failures keep the last valid RAM agenda with a stale indicator. Events are expanded for 90 days; an expired offline cache asks you to reconnect. Event data is not written to flash or retained after a reboot. Google controls when an edited event appears in its feed, so polling does not guarantee immediate updates.

The bounded iCalendar parser supports ordinary daily, weekly, monthly, and yearly rules; intervals and counts; UNTIL, BYDAY, BYMONTH, BYMONTHDAY, BYSETPOS, and WKST; all-day events; EXDATE/RDATE; and moved/cancelled single instances. It handles UTC, floating Eastern times, and embedded VTIMEZONE observances with annual rules or explicit transition dates. Tests cover US and European DST changes. Recurrence follows [RFC 5545](https://www.rfc-editor.org/rfc/rfc5545.html).

This is not a complete RFC implementation. Sub-daily rules, EXRULE, RANGE=THISANDFUTURE, unknown time zones, and other unsupported event forms produce a visible warning rather than an empty-calendar claim. Large calendars are bounded at 3 MiB, 2,000 event components, 1,024 displayed occurrences, and a shared recurrence work budget. Structural or download-limit failures preserve the previous agenda; unsupported events are omitted with a warning. Check Google Calendar if the warning appears. Tests use invented appointments and a fake secret URL; no personal calendar is checked into this repository.

## Data sources

| Content | Source | Normal refresh |
| --- | --- | --- |
| Calendar | Your private Google Calendar iCal feed | 5 minutes, plus each new local day |
| Weather | [Open-Meteo](https://open-meteo.com/en/docs), CC BY 4.0, free noncommercial API | 15 minutes |
| F1 | [Jolpica F1 alpha schedules/results API](https://api.jolpi.ca/docs/) | Results: 5 minutes; schedule: 1 hour; also checks each new local day |
| US Open | [ESPN public tennis scoreboard](https://site.api.espn.com/apis/site/v2/sports/tennis/atp/scoreboard) (includes singles and doubles for both men and women) | 5 minutes during the event; 6 hours off-season, plus each new local day |
| NFL | [ESPN public scoreboard endpoint](https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard) | 5 minutes |
| Clock | `pool.ntp.org`, `time.google.com` | SNTP |

ESPN's endpoint is publicly accessible but undocumented; it is not an open-source or guaranteed service. The ESPN adapters are in `main/dashboard.cc`, `main/tennis.cc`, and `main/feeds.cc`. ESPN rejects the prior custom User-Agent; requests now use a tested standard-library compatibility header. The tennis endpoint returns the full tournament despite its date parameter, so a bounded token parser reads one match at a time rather than allocating a full JSON tree. A reusable download buffer is allocated at startup to keep a contiguous parser allocation available in PSRAM. Tennis responses are capped at 3 MiB and 200,000 tokens; calendar responses also have a 3 MiB limit; other feeds retain the 768 KiB limit. Requests have time and size limits. Empty schedules and unavailable results have explicit display states. Attribution appears on each page. Test fixtures capture provider data on September 5, 2026; they are test samples, not the running dashboard's data source.

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

## Power and hardware scope

This is a USB desk dashboard. It uses Wi-Fi modem power saving and powers off the e-paper rail between full 1bpp refreshes, but it does not schedule automatic deep sleep. Battery endurance is unmeasured. No OTA update path is configured; subsequent firmware updates use USB. Audio and NFC are not application features in this version.

The board and display drivers retain the official upstream mapping and waveform. The application asserts the board power latch before networking, serializes panel access on a dedicated display task, and uses a separate data-fetch task with protected snapshots. Full refreshes avoid partial-refresh state and ghosting complications across entirely different pages.

## Code map

- `main/app_main.cc`: startup, screen updates, buttons, and shutdown.
- `main/network.cc`, `main/setup.html`: Wi-Fi storage, local setup, SNTP, and bounded HTTPS.
- `main/feeds.cc`: source URLs, polling, and snapshot synchronization.
- `main/dashboard.cc`: response validation, score ordering, and timezone conversion.
- `main/render.cc`: the 400 × 300 screen layout and section/page mapping.
- `main/calendar.cc`, `main/calendar_rules.cc`: bounded iCalendar parsing, recurrence expansion, timezone conversion, and agenda selection.
- `main/settings.cc`: private URL validation and setup changes that preserve existing settings.
- `main/tennis.cc`: bounded tournament parsing and event lifecycle.
- `main/formula_one.cc`: session classifications, schedule validation, race-day anchors, and bounded result lookup.
- `main/display_worker.cc`: asynchronous panel access and pending-frame replacement.
- `main/battery_history.cc`: bounded persistent battery history.
- `tests/`: data, failure, timezone, credential, and rendering checks. Host tests use AddressSanitizer and UndefinedBehaviorSanitizer.

Based on [itopinion/zectrix-note4-epd-demo](https://github.com/itopinion/zectrix-note4-epd-demo). Its MIT license and third-party notices remain in the repository. The original README is preserved in `docs/UPSTREAM_README.md`.


## Local battery history

The device saves a battery reading at startup (after charger settling), every 15 minutes while running, and before button-requested shutdown. A bounded NVS ring holds the latest 512 readings: about five days and eight hours at the regular interval, less with frequent restarts. It overwrites the oldest readings and survives power loss and app-only firmware updates. It needs no partition changes and never erases Wi-Fi settings to recover storage. Logging stops after a storage error until the next boot.

Each reading contains sequence number, random boot ID, Unix timestamp (zero before clock sync), uptime in seconds, averaged/calibrated voltage in millivolts, estimated percentage, and battery-valid, external-power, charging, full, fault, no-battery, and Wi-Fi-connected flags. Timestamps and boot IDs help separate interrupted discharge sessions. The log contains no SSID, password, location, or fetched feed data. It is never uploaded. NVS uses wear levelling; the 15-minute cadence limits flash writes without requiring extra screen refreshes.

To export locally, close any serial monitor, activate the ESP-IDF environment, and run:

```sh
python tools/export-battery.py --port /dev/cu.usbmodem2101 --output battery-2026-09-05.csv
```

Use a fresh output filename each time. The script validates row counts and sequence continuity and saves only battery CSV records. Opening USB can restart the board on this Mac; previously saved readings remain available. A serial terminal can also send `battery` followed by Enter at 115200 baud. Export works in both the dashboard and Wi-Fi setup modes.

The readings will help estimate runtime and tune the voltage curve after normal unplugged use. Charging and load affect voltage, so this remains an estimate rather than a measured state of charge. A sudden low-battery power loss may occur up to one sampling interval after the last saved reading. There is no measured runtime for this firmware yet; its always-awake processor and connected Wi-Fi do not match the factory's one-sync-per-day standby test.


## Local USB diagnostics

At 115200 baud, send `next`, `previous`, or `more` followed by Enter to exercise the same navigation actions as the buttons. `refresh` requests a rate-limited background refresh; `battery` exports the saved battery log. These commands only work locally over USB. The display task logs each completed frame's duration and result. Opening the USB port can restart this board on macOS.
