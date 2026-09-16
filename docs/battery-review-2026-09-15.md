# Battery review — September 15, 2026

## Findings

The owner reports about a week of operation, reaching 0% before reconnecting. Firmware 0.5.1's last saved boot counter spans **197 h 12 min (8.22 days)**, September 7 at 12:18 EDT through September 15 at 17:30 EDT. This is boot uptime, not an exact unplug-to-cutoff measurement: the initial charging/unplug records were overwritten. The retained discharge samples independently confirm **127 h 20 min (5.31 days)** of continuous battery operation from September 10 at 10:10 EDT onward.

The export contains 512 consecutive records, sequences 468–979: 510 from the long discharge and two USB charging records. Every retained discharge reading is valid, on battery, and Wi-Fi-connected, with no charger faults. Sampling intervals were 900–902 seconds. The last battery sample was 2.573 V; charging resumed later, so the exact loss-of-power time is not known.

The original polynomial reached 0% near 3.448 V, **29.5 hours before the last recorded battery sample**. The observation that low percentages lasted unusually long is confirmed. Percentages were materially pessimistic for this device's discharge profile.

| Old display estimate | Recorded voltage | Time until last battery sample | New provisional estimate |
| --- | --- | --- | --- |
| 50% | 3.718 V | 127.3 h | ~65% |
| 31% | 3.606 V | 93.8 h | ~46% |
| 20% | 3.544 V | 60.3 h | ~29% |
| 10% | 3.496 V | 40.8 h | ~20% |
| 5% | 3.470 V | 33.8 h | ~17% |
| 0% | 3.448 V | 29.5 h | ~15% |

## Power and network evidence

The saved long-boot snapshot reports 517,787 seconds in sleep callbacks out of 709,947 seconds uptime (72.9%). These callbacks include sleep-entry/exit overhead and rejected attempts; this is neither exact successful sleep time nor measured current. The device issued 4,943 HTTP requests, four failing (0.081%), with 1.91 GB of response bodies and 3.83 hours of request elapsed time. It completed 3,778 panel waveforms with no failures, totaling 1.40 hours. Counters overlap and are not an energy breakdown. Requests/bytes are aggregate; they cannot attribute traffic to individual feeds.

The result exceeds the original 3–4 day target and is consistent with the owner's roughly week-long battery run. There is no evidence here that slower calendars, disabled peripherals, or more aggressive sleep is needed. Feed cadences, Wi-Fi policy, audio/NFC/RTC power, and display behavior are unchanged.

## Changes in 0.6.0

- A monotonic piecewise discharge curve replaces the display's low-voltage polynomial interpretation. Lower knots approximate remaining runtime in this observed workload, conservatively rounded. The unavailable upper range interpolates from 3.720 V / 65% to the existing 4.144 V / 100% anchor. This is a first-pass runtime proxy, not coulomb-counted capacity, and needs another run for validation. Display keeps the ~ prefix and smoothing. Charging still uses the previous voltage estimate and charger FULL signal; discharge calibration is never applied to charging voltage.
- 0% now means roughly the bottom reserve near 3.0 V, not guaranteed immediate shutdown. The prior run had about 1.75 hours below 3.0 V. This change controls the indicator only; hardware protection and shutdown behavior are unchanged.
- Regular battery logging changes from 15 to 30 minutes: 512 slots now cover roughly 10 days 16 hours instead of 5 days 8 hours, reduced by startup/transition records. It also halves regular battery/diagnostic write frequency. CSV layout and original raw percentage formula remain unchanged for historical comparison; millivolts are the basis for recalibration.
- A dedicated `discharge` power snapshot is updated only with battery-powered samples. Repeated USB inspection resets can no longer replace that snapshot with short charging boots. Its counters still describe the whole boot, including any earlier charging period.
- The previously approved single-page Home changes ship in the same release: tomorrow forecast, rolling upcoming agenda, refresh timestamp without a source footer.

## Validation and next run

ESP-IDF 5.4.4 build and all five host sanitizer suites passed. Cases include monotonic/bounded discharge mapping, the old false-zero voltage, charging/FULL/settling/invalid readings, raw-history wrap/restart, and Home rollover/forecast behavior. Physical unplugged behavior and the new curve need a subsequent battery run. Wait for FULL, use normally, and reconnect when low; deliberately exhausting the battery is not necessary for ordinary use. Export both logs before another firmware update. Initial factory backups remain available and private; restoration has not been tested.

## Sources and limits

- Local telemetry: `home-note4-battery-2026-09-15.csv`, `home-note4-power-2026-09-15.json` (saved outside Git in the Codex workspace outputs folder).
- [ZECTRIX specifications](https://zectrix.com/en/note4-developer-kit.html): 2,000+ mAh nominal battery; no current/capacity measurement was made during this run.
- [TI: State of Charge Estimation Using Smart Battery Charger](https://www.ti.com/lit/an/sluaaa1/sluaaa1.pdf): voltage-based estimates depend on operating conditions; a measured discharge under one workload does not establish true capacity across temperatures, loads and aging.
