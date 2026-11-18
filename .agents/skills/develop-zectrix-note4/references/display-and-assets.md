# Display and Image Assets

Use the official `zectrix_epd` ESP-IDF component for the SSD2683. Its public C API can be called from C or C++ and all operations are synchronous.

## Lifecycle

```c
zectrix_epd_config_t config;
zectrix_epd_get_default_config(&config);

zectrix_epd_handle_t epd = NULL;
ESP_ERROR_CHECK(zectrix_epd_new(&config, &epd));
ESP_ERROR_CHECK(zectrix_epd_power_on(epd));

/* Refresh operations. */

ESP_ERROR_CHECK(zectrix_epd_power_off(epd));
ESP_ERROR_CHECK(zectrix_epd_del(epd));
```

`zectrix_epd_new()` configures GPIO and SPI but leaves the panel rail off. Refresh calls complete after the BUSY handshake or return a timeout.

In a long-running product, handle recoverable `esp_err_t` values instead of using `ESP_ERROR_CHECK` where a display failure should not reboot the entire device. Expected error classes include invalid arguments, invalid size, invalid state, and timeout.

## Pixel formats

| Mode | Layout | Size | Value meaning |
|---|---|---:|---|
| Full 1 bpp | 400 × 300, row-major, MSB first | 15,000 bytes | `0` black, `1` white |
| Partial 1 bpp | Tightly packed rows, `ceil(width / 8) × height` | Region-dependent | Same as 1 bpp |
| Full 4 bpp | 400 × 300, two pixels per byte; left pixel in high nibble | 60,000 bytes | `0` black through `15` white |

The public operations are `zectrix_epd_refresh_full_1bpp`, `zectrix_epd_refresh_partial_1bpp`, and `zectrix_epd_refresh_full_4bpp`.

## Refresh rules

- The partial-refresh base belongs to the current live driver handle. Complete a full 1 bpp refresh after every boot or `zectrix_epd_new()` before the first partial update.
- Keep partial rectangles inside the 400 × 300 panel and pass the exact packed byte count.
- A partial-refresh timeout invalidates controller readiness and the assumed base. Recover with `zectrix_epd_power_off()`, then `zectrix_epd_power_on()`, then a full 1 bpp refresh. Do not immediately retry a partial or full refresh in the timed-out powered state.
- Periodically replace partial refreshes with a full refresh to limit accumulated artifacts. The demo promotes after eight UI partial refreshes; that number is an application policy, not a driver requirement.
- Before 4 bpp, a white full 1 bpp refresh gives a cleaner transition.
- After 4 bpp, perform another full 1 bpp refresh before partial updates.
- Retained pixels while power is off are normal for e-paper.

## SPI ownership

The default configuration uses `SPI3_HOST`, a 20 MHz SPI clock, and a 2,000 ms BUSY timeout. It initializes the SPI bus. Set `initialize_spi_bus = false` only when the application already owns the selected host and has configured compatible pins and DMA settings.

## Convert assets

The reference repository supplies Pillow-based converters:

```bash
python -m pip install Pillow
python tools/prepare_1bpp.py input.png output.png output.bin
python tools/prepare_4bpp.py input.png output.png output.bin
python tools/generate_ascii_font.py --help
python tools/prepare_footprint_animation.py --help
```

The 1 bpp converter defaults to threshold `160`; the 4 bpp converter defaults to contrast `1.55`. Inspect each preview and tune for the content, for example with `--threshold 145` or `--contrast 1.35`. Confirm final dimensions and byte count before building.

Embed generated binaries with the consuming component's `EMBED_FILES` list.

For custom drawing, the demo's `zectrix_canvas` and bundled bitmap font are examples. Design for black-and-white legibility, avoid antialiasing unless it is intentionally quantized, and reserve partial refresh for small changing regions.

Sources: [ZECTRIX display guide](https://wiki.zectrix.com/en/guides/esp32-s3-e-paper-display-guide), [EPD API](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/EPD_API.md), [public header](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/components/zectrix_epd/include/zectrix_epd.h), [driver implementation](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/components/zectrix_epd/zectrix_epd.cc), [UI refresh policy](https://github.com/itopinion/zectrix-note4-epd-demo/blob/main/docs/UI_FLOW.md).
