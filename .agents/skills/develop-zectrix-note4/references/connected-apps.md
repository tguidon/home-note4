# Connected Applications: Lessons from Device Testing

These observations come from `home-note4` through commit `2d1b6c2`, tested on 2026-09-05 with ESP-IDF 5.4.4 and one monochrome NOTE4 matching the v1.0 N16R8 profile. They supplement the official references; they are not manufacturer guarantees. The repository may not be available in another workspace, so the guidance below is self-contained.

## Responsive navigation on e-paper

Separate input handling, network fetching, and synchronous panel refreshes. Give one worker exclusive ownership of the display handle, including power transitions. Publish protected data snapshots from the network worker, and keep locks out of HTTP calls and panel waveforms.

For dashboard navigation, keep one pending frame that newer input can replace. Copy the frame into worker-owned storage before drawing. This avoids replaying obsolete pages after rapid taps. Skip identical pending frames, but distinguish failed refreshes from successful display state so a failure can be retried.

Use cached content immediately when changing pages. Show a loading state when no cache exists. An extra loading frame before cached content adds another waveform and can make navigation feel slower. A short LED pulse provides prompt input feedback while the panel is busy; this app used 120 ms and the owner liked it.

Full 1 bpp updates measured about 1.29–1.32 seconds, including panel power transitions, in this app. Treat that as observed latency, not a fixed specification. Moving refreshes to a worker keeps input responsive but does not shorten the waveform. Keep the existing driver lifecycle and partial-refresh invariants in [display-and-assets.md](display-and-assets.md).

## Large JSON responses and PSRAM

A date filter does not guarantee a small response. The tennis feed returned a full tournament of about 1.63 MB. Measure actual response sizes and parse times on the device.

Total free PSRAM can hide allocation failure caused by fragmentation. Log `heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)` near large allocations. In this app, reserving one reusable 3 MiB response buffer before HTTP traffic preserved room for a contiguous token array. That size is application-specific: budget response storage, parser storage, TLS allocations, snapshots, and task stacks together.

For large feeds, consider a bounded token or streaming parser instead of a complete JSON object tree. Avoid rescanning the whole token array for each field; parent links and subtree boundaries reduced this app's parse time from roughly seven seconds to 1.5 seconds. Enforce body-size, token-count, timeout, and response-completeness limits. A parser that succeeds on a desktop fixture still needs device memory and latency checks.

## Cached feeds and time

Parse into temporary state and replace cached data only after validation. Distinguish a valid empty response, unpublished results, and a transport or parse failure. Preserve useful prior results while newer data is unavailable, and show when cached data is stale.

Keep scheduled events separate from published results. A scheduled session may have started without its classification being available. Do not label a provider's published classification as final or live unless its contract supports that claim.

Compute day-based display behavior from the configured local timezone, including when rendering cached data offline. Use UTC timestamps for instants and monotonic time for retry intervals. Test local midnight, daylight-saving changes, empty schedules, event completion, and cold starts. Do not assume a sport always runs on a particular weekday.

## Local provisioning and data ownership

Custom ESP-IDF firmware can serve its own temporary Wi-Fi setup page using a SoftAP and local HTTP server. This app required no vendor account or WeChat. Keep credentials in device NVS, out of compiled assets and logs; stop the provisioning service when setup ends. A random setup password and form token were used here.

When explaining where data goes, inspect the application, dependencies, configured endpoints, provisioning, and update paths. Include DNS and clock synchronization as well as content APIs. Open-source application code alone does not prove that every firmware component is open source or that no network traffic occurs. Keep certificate validation enabled and establish a credible clock before HTTPS.

Treat full-flash backups and raw serial logs as potentially sensitive: they may contain saved credentials or setup passwords. Keep them out of source control and filter user-facing logs.

## Battery estimates and local history

`ReadPowerSnapshot()` provides voltage and an estimated percentage, not a calibrated measure of remaining runtime. USB charging and device load affect voltage. Record millivolts, validity, external-power and charging flags alongside percentage; include a sequence number, boot identifier, uptime, and wall time when known.

Use bounded storage with an explicit write cadence. This app used a separate NVS namespace, 512 records in blocks of 32, a settled startup sample, 15-minute samples, and a shutdown sample. These are examples, not required constants. Account for existing NVS users and write overhead; a separate namespace still shares partition capacity. Stop telemetry writes on storage errors instead of erasing shared NVS and losing Wi-Fi settings.

Provide a local export with framing and a record count so readers can reject partial transfers. Export before and after an update to verify retention. This app preserved all prior records through application-only flashes. Battery calibration and endurance still require sustained battery-powered measurements; USB samples and a retained e-paper image cannot establish battery life. Factory endurance claims do not transfer to an awake, network-connected custom app.

## Evidence that transfers to future work

Render host previews through the actual firmware drawing code at 400 × 300. Exercise paging, long labels, empty data, stale data, and rollover states. Keep captured provider fixtures separate from firmware assets and label simulated results.

Local USB commands can call the same action handler as physical buttons for repeatable navigation checks. They verify that handler and the rendering path, not switch electrical behavior or physical button timing. Report host tests, flash verification, device logs, renderer previews, and owner observations separately. `ESP_OK` from the display driver does not prove the screen looks correct.
