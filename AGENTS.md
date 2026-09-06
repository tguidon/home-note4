# Home Note4 project preferences

- Keep the temporary setup Wi-Fi network name and password consistent across setup sessions so the owner’s phone can reconnect easily. The owner requested this on September 5, 2026. Prefer reusing the last setup password when available.
- This preference is pending implementation: firmware 0.4.0 generates the setup password in RAM and does not persist it. It cannot recover that password after a restart. In the next firmware update, persist a per-device setup password in NVS and reuse it; keep the form token random per session. Do not put passwords in source, logs, or documentation.
- The owner confirmed on September 5, 2026 that adding the private Google Calendar through the setup page works and the calendar appears on the device.
