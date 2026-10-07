# AM6 Display — JC2432W328 / XH-32S

PlatformIO firmware for the AM6 2.8-inch 320x240 landscape display.

## Stack

- ESP32 Arduino framework
- TFT_eSPI 2.5.43
- LVGL 8.3.11
- CST820 capacitive touch over I2C
- ESPAsyncWebServer + AsyncTCP
- ArduinoJson 6.21.5
- HTTPClient

## UI

The established dashboard layout is retained:

- Power card on the left
- Cool / Heat mode on the upper right
- Low / Med / High fan speed on the lower right
- Link-state chip in the header

The theme is a dark industrial palette matching the supplied reference: black/navy background, cyan/blue borders, white text, blue Cool state, green Heat/fan state, and compact controls sized for the real 320x240 panel.

The real hardware photo confirmed this panel must use ST7789 inversion OFF. Inversion ON complements black to white, cyan to orange/red, and green to purple. The backlight remains off until the first LVGL frame is flushed to avoid a startup flash.

## Wireless link

The display connects as a Wi-Fi station to the Indoor hardware:

- SSID: `AM6-AAA001`
- Password: `bitahomes`
- Indoor: `192.168.4.1`
- Display: `192.168.4.50` (static)

Display → Indoor:

```text
POST http://192.168.4.1/from_display
```

Indoor → Display:

```text
POST http://192.168.4.50/receive
```

Values:

- `powersw`: 0 OFF / 1 ON
- `mode`: 0 COOL / 1 HEAT
- `fanSw`: 0 LOW / 1 MED / 2 HIGH

The UI enables controls after an authoritative state has been received from Indoor. Controls remain usable while a previous command is synchronizing so the latest user choice can replace the pending request. A user touch is sent to Indoor, the UI shows `SYNC`, and Indoor sends the confirmed state back.

## PlatformIO

Open this folder in VS Code with PlatformIO and build the `jc2432w328` environment.

If changing from an older AM6 display project, run PlatformIO **Clean** or delete `.pio` before the first build so old TFT/LVGL build flags cannot remain cached.

## Display/touch pin configuration

TFT_eSPI build flags configure the ST7789 on the known JC2432W328 mapping already proven to render on this hardware. CST820 uses SDA GPIO33, SCL GPIO32, RESET GPIO25. Backlight uses GPIO27.

Touch reliability improvements in this build:

- CST820 I2C reduced to 100 kHz for margin
- vendor-style reset timing (LOW 10 ms, HIGH 300 ms)
- up to 3 retries on register reads
- automatic low-power mode disabled (`0xFE = 0xFF`)
- 45 ms release grace filters one-sample I2C dropouts
- LVGL controls react on `LV_EVENT_PRESSED` instead of waiting for click/release
- controls remain usable during `SYNC` when the Indoor link is still fresh
- Serial prints `[TOUCH] DOWN x=... y=...` and `[TOUCH] UP` for diagnosis

There is no RS485/UART communication in the display firmware.
