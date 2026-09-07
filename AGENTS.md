# Home Note4 project preferences

- Keep the temporary setup Wi-Fi network name and password consistent across setup sessions so the owner’s phone can reconnect easily. The owner requested this on September 5, 2026. Prefer reusing the last setup password when available.
- Firmware 0.5.0 persists a per-device setup password in NVS and reuses it; the form token stays random per session. The previous RAM-only password cannot be recovered after restart. Do not put passwords in source, logs, or documentation.
- The owner confirmed on September 5, 2026 that adding the private Google Calendar through the setup page works and the calendar appears on the device.

- September 7, 2026: first battery optimization target is 3–4 days, to be verified by another discharge run. Keep audio, NFC, RTC and their existing supply rails enabled; do not add unused-peripheral power gating. Preserve existing e-paper rail handling and explicit user-requested shutdown.
- The first power update is code-first. Do not claim that a build establishes hardware sleep/wake or battery endurance.
