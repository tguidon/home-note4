# Battery indicator review — October 25, 2026

The owner reported that the display stayed around 70% after at least 30 minutes on several USB chargers. The retained records do not confirm a completed charge during that period. They also expose limits in the current voltage estimate and display diagnostics.

## Evidence

Before updating, exported 512 battery records (sequences 2742–3253) and four power snapshots to the ignored `backups/2026-10-25/` directory. Raw data remains local.

- The retained history spans October 18–25. Of 512 readings, 65 have external power and none has the charger FULL or fault flag.
- The October 20 charging window reaches 3.927 V, about 79% under the old charging formula. Detected external power repeatedly changes during that window. The records cannot distinguish a connection/charger problem from signal-sampling behavior.
- The previous confirmed FULL session, on September 15, reached about 4.175 V roughly two hours after its initial 3.674 V sample. Thirty minutes plugged in does not establish completion.
- The final October 25 battery-powered sample is 2.748 V. A later power-on charging sample is 2.703 V. The power snapshot's 31.17 days of boot uptime includes intervening charging; it is not a continuous discharge endurance measurement.
- The long-boot power snapshot records 20,168 display attempts and zero driver failures. Old firmware records neither the last drawn battery label nor the time of that frame. It cannot establish exactly what the owner saw, and zero driver errors does not prove optical output.

## Changes in 0.7.0

1. Charging previously used the original board percentage formula while discharge used the September provisional curve. The same voltage could therefore display different percentages. Both modes now use the same voltage curve. Charging remains an approximate voltage proxy, capped at 99%; the hardware completion signal displays `100% FULL`, independent of the voltage estimate.
2. The display worker previously suppressed an identical frame after a failed draw. It now permits that frame to be submitted again on the next redraw. This is a confirmed code defect, but the retained logs do not show that it caused the reported symptom.
3. Power diagnostics record the latest display attempt and the last successful frame's time, battery voltage and label. The metadata travels with its frame, including when pending frames are replaced. Old power snapshots remain readable; absent metadata is explicit. Logs still contain no calendar content or credentials.
4. The complete US Open scene, parser, requests, scheduling and fixtures are removed. Navigation is Home → F1 → NFL.

The discharge curve, raw CSV formula, charger-state decoder, remaining feed cadences and peripheral power supplies are unchanged. No new endurance or measured-capacity claim follows from these changes.

## Validation

ESP-IDF 5.4.4 build passed for ESP32-S3 with 16 MB flash and 8 MB octal PSRAM. All six host suites passed with address and undefined-behavior sanitizers. Coverage includes charging/full/settling labels, three-section navigation, retrying a failed identical frame, legacy power snapshot migration, USB preservation of discharge records, and last-successful-frame metadata. Actual-renderer previews were inspected for Home, F1, NFL and the new battery labels.

The application-only flash at `0x10000` passed hash verification after the device partition table matched the build. Firmware 0.7.0 booted with 8 MB PSRAM and RTC/NFC initialized. Saved Wi-Fi, private calendar, weather, F1 and NFL loaded. USB navigation exercised forward and reverse wraparound and returned to Home. Ten display updates completed without driver failures during the navigation check. A later boot completed all six initial data requests without failure. The pre-existing duplicate GPIO ISR-service diagnostic is handled by the board adapter; it did not prevent initialization.

All 508 overlapping battery rows matched the pre-update export exactly; four oldest records rotated out normally as new readings were appended. The complete original 512-row export remains saved. The legacy discharge snapshot preserved all counters and correctly reported missing frame metadata. New frame metadata was verified on the device. The two matching factory backups remain available; restoration is untested. Physical button feel and the charging-to-FULL transition were not observed during this USB check.

## Follow-up observation

Leave the device connected until `100% FULL` appears, rather than treating an elapsed half hour as full. If the estimate stalls again, export battery and power diagnostics before restarting or flashing. The new frame metadata can distinguish an old screen from a current low estimate. A later full-to-low discharge run can refine the provisional curve. Physical charger completion and battery endurance still require observation on the device.
