# NOTE4 Hardware Reference

This reference applies to the monochrome ZECTRIX NOTE4 Developer Kit, PCB v1.0. It does not apply to NOTE4C.

## Core hardware

| Item | Value |
|---|---|
| MCU | ESP32-S3 N16R8 |
| Flash / PSRAM | 16 MB / 8 MB octal PSRAM |
| Display | 4.2-inch, 400 × 300, black-and-white e-paper |
| Display controller | SSD2683 |
| Wireless | 2.4 GHz Wi-Fi and Bluetooth LE 5.0 |
| Audio | ES8311 codec, microphone, speaker |
| RTC | PCF8563 |
| NFC | NT3H2111 / NT3H2211 family interface |
| USB | USB-C power, native USB data, flashing, and serial |
| Battery | 2,000+ mAh rechargeable |
| Logic level | 3.3 V; do not apply 5 V to GPIO |

Sources: [ZECTRIX development guide](https://wiki.zectrix.com/en/software/note4-development-guide), [official reference project](https://github.com/itopinion/zectrix-note4-epd-demo).

## Controls and USB

| Physical control | Signal | Behavior |
|---|---|---|
| Front circular button | GPIO0 | BOOT / Confirm, active low; also a boot-strapping pin |
| Right upper button | GPIO39 | Up, active low |
| Right lower button | GPIO18 | Down / power input, active low |
| Recessed left pinhole | ESP32-S3 EN / RST | Hardware reset; does not erase firmware |
| USB D- / D+ | GPIO19 / GPIO20 | Native USB data, flash, and serial |
| UART0 TX / RX | GPIO43 / GPIO44 | UART0 |

The demo interprets an OK hold of 1.5 seconds as return/cancel and a DOWN hold of 3 seconds as clear-and-shutdown. Those actions are firmware policy, not fixed hardware behavior.

## E-paper SPI

| Function | GPIO / default |
|---|---:|
| Panel power enable | 6 |
| BUSY | 8 |
| RESET | 9 |
| DC | 10 |
| CS | 11 |
| SCK | 12 |
| MOSI / SDA | 13 |
| SPI host | `SPI3_HOST` |
| SPI clock | 20 MHz |
| BUSY timeout | 2,000 ms |

Use `zectrix_epd_get_default_config()` rather than duplicating these values when the official component is present. An application may modify the returned `zectrix_epd_config_t` before creating the driver.

## Board power, battery, and indicators

| Function | GPIO | Notes |
|---|---:|---|
| Charge complete / standby | 1 | `STDBY_H` |
| Charging status | 2 | `CHRG_L` |
| Status / power LED | 3 | Green; official board adapter drives it active low |
| Battery voltage ADC | 4 | Through resistor divider |
| RTC interrupt | 5 | PCF8563 interrupt |
| NFC field detect | 7 | Official adapter configures active low |
| Battery power hold | 17 | Assert high early to retain the battery rail |
| NFC power control | 21 | NFC power-down / enable |

USB can keep the device powered after the firmware releases the battery latch. The reference shutdown path clears the panel, powers peripherals down, releases the latch, and enters deep sleep as a USB-powered fallback.

E-paper retaining its final image without power is normal.

## I2C peripherals

| Signal or device | Value |
|---|---|
| SDA | GPIO47 |
| SCL | GPIO48 |
| PCF8563 RTC | Address `0x51` |
| NT3H NFC | Address `0x55` |

The board source provides a shared I2C lock. Use it when operations may overlap, especially NFC transfer sessions and other I2C peripherals.

## Audio

| Function | GPIO |
|---|---:|
| I2S MCLK | 14 |
| I2S BCLK / SCLK | 15 |
| Codec DOUT / ESP32 RX | 16 |
| I2S LRCK / WS | 38 |
| Audio rail enable | 42 |
| Codec DIN / ESP32 TX | 45 |
| Speaker amplifier enable | 46 |

The reference configuration uses a 16 kHz audio sample rate. Initialize audio lazily when possible and disable its rail and amplifier when unused.

## Schematic and component documents

- [NOTE4 Developer Kit v1.0 schematic](https://wiki.zectrix.com/sch_zectrix_note4_developer_kit_v1.0_en.pdf)
- [ESP32-S3 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [PCF8563 datasheet](https://www.nxp.com/docs/en/data-sheet/PCF8563.pdf)
- [ES8311 product brief](https://www.everest-semi.com/pdf/ES8311%20PB.pdf)
- [Espressif ES8311 component](https://components.espressif.com/components/espressif/es8311/versions/1.0.0?language=en)

ZECTRIX does not link a public manufacturer-hosted SSD2683 datasheet. Treat the official driver, its public header, and the board schematic as the supported display reference.
