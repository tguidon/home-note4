# Validation and Troubleshooting

## Validation ladder

Use the smallest test that can reveal the problem, then progress toward real hardware:

1. Confirm target, component dependencies, configuration, and partition layout.
2. Compile with `idf.py build`.
3. Inspect the generated flash command and binary offsets when deployment is involved.
4. Flash the verified port and capture the 115200-baud boot log.
5. Exercise the changed feature on the device.
6. Run the full reference hardware test suite after board-support, pin, power, or peripheral changes.

A successful build verifies compilation, not GPIO mapping, waveform behavior, charging, RF, acoustic coupling, NFC restoration, or battery shutdown.

## Reference hardware tests

| Test | Pass condition | Setup |
|---|---|---|
| Wi-Fi RF | Demo mode finds a 2.4 GHz AP; qualification mode sees the configured SSID three consecutive times above threshold | Provide a 2.4 GHz AP |
| Audio | Played AFSK sequence is recorded and decoded | Quiet space; uncover mic and speaker |
| RTC | One-second PCF8563 countdown sets its flag or interrupt in the retry window | None |
| Power | Active charging, or full status with a valid battery above 97%; no-battery and fault states fail | Battery plus USB |
| LED | Operator sees the LED blink | Confirm on device |
| Buttons | Correct `OK`, `UP`, `DOWN` sequence is received | Follow prompts |
| NFC | Original user bytes are backed up, a temporary NDEF is written/read, phone field is detected, then bytes are restored | NFC-capable phone; stable power |

Interactive tests normally time out after about 60 seconds. The demo allows long OK to cancel and long DOWN to request shutdown.

Source: [official test criteria](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/TEST_CRITERIA.md).

## Common symptoms

### No serial port or connection

- Replace charge-only cables with a verified data cable.
- Connect directly instead of through an unreliable hub.
- Compare port lists before and after connection.
- Use the BOOT-plus-RESET download-mode sequence in [deployment-and-recovery.md](deployment-and-recovery.md).
- On Linux, check device permissions and the relevant serial group.

### Repeated reset after flashing

- Confirm `esp32s3`, 16 MB flash, and octal PSRAM.
- Check that the installed bootloader, partition table, and application agree. Use a whole-project flash when correcting a verified layout mismatch; compatible routine updates may use an application-only flash as described in [deployment-and-recovery.md](deployment-and-recovery.md).
- Read the first reset reason and panic/backtrace from the serial log.
- Confirm the application asserts the GPIO17 battery latch early enough when running from battery.

### E-paper BUSY timeout

- Confirm monochrome NOTE4 PCB v1.0 and the EPD pin mapping.
- Inspect the display flex cable only when safe and necessary.
- Confirm panel power, RESET, BUSY, and SPI configuration.
- Do not issue partial refresh before a successful full 1 bpp base for the current boot and live driver handle.
- After a partial timeout, power the panel off and on, then establish a full 1 bpp base. Do not retry in the timed-out powered state.
- Log and inspect the exact `esp_err_t` instead of treating every timeout as the same failure.

### Ghosting or dirty partial updates

- Check patch packing, bounds, and the exact byte count.
- Periodically force a full refresh.
- Establish a new full 1 bpp base after grayscale output, every boot, and every new driver handle.
- Tune cadence on the real panel at expected temperatures.

### Device will not power off on USB

This can be normal: USB holds the rail up. Validate battery-latch shutdown with USB disconnected after the flash/monitor session. The reference firmware clears the panel and uses deep sleep as the USB-powered fallback.

### Component download fails

Verify access to the ESP Component Registry and the constraint in `main/idf_component.yml`, then retry the build. The reference requires `espressif/esp_codec_dev ~1.5`.

### Peripheral-specific failure

- I2C: confirm GPIO47/48, 3.3 V, bus serialization, and device addresses `0x51` / `0x55`.
- Audio: confirm the audio rail, ES8311 initialization, `valid()`, I2S pins, and amplifier enable.
- NFC: keep power stable, confirm field polarity, and protect/restore user memory.
- Charging: interpret GPIO1/2 together with a valid battery-voltage reading; do not infer “full” from one pin alone.

## Evidence to report

Record the device revision, ESP-IDF version, commit or release, port, build result, flash result, relevant serial log, and each physical pass/fail. Keep uncertain measurements labeled as uncertain.
