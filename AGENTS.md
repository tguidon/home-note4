# Home Note4 project preferences

- Keep the temporary setup Wi-Fi network name and password consistent across setup sessions so the owner’s phone can reconnect easily. The owner requested this on September 5, 2026. Prefer reusing the last setup password when available.
- Firmware 0.5.0 persists a per-device setup password in NVS and reuses it; the form token stays random per session. The previous RAM-only password cannot be recovered after restart. Do not put passwords in source, logs, or documentation.
- The owner confirmed on September 5, 2026 that adding the private Google Calendar through the setup page works and the calendar appears on the device.

- September 7, 2026: first battery optimization target is 3–4 days, to be verified by another discharge run. Keep audio, NFC, RTC and their existing supply rails enabled; do not add unused-peripheral power gating. Preserve existing e-paper rail handling and explicit user-requested shutdown.
- The first power update is code-first. Do not claim that a build establishes hardware sleep/wake or battery endurance.

- September 11, 2026: Home stays one page with current/tomorrow weather and the nearest ongoing/upcoming calendar events. Remove ended events and fill remaining space with later days; remove the home data-source footer but keep last refresh. Develop this update on a separate branch and leave the running device alone during its battery trial.

- September 15, 2026: battery trial complete; owner authorizes battery improvements, squash merge to main, and deployment including Home changes. Raw CSV percentages keep the original board formula; the on-screen discharge estimate now uses the provisional device curve. Preserve the exported run before flashing. Keep peripheral rails and existing feed cadences unchanged.

- October 25, 2026: remove the entire US Open scene and its requests. Keep Home → F1 → NFL. Charging estimates use the same provisional voltage curve as battery estimates, capped at 99%; only the charger completion signal shows `100% FULL`. Preserve raw CSV percentages.
