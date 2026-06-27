# galactic-unicorn-pad

A standalone **touch controller** for the [galactic-unicorn-leg](../galactic-unicorn-leg) LED noticeboard.

Tap word tiles to assemble a message, preview it, and send it to the Galactic Unicorn LED matrix over WiFi — no phone or web browser needed. Built for the **Elecrow CrowPanel 7" HMI display** (ESP32-S3) with LVGL.

## How it works

```
Touch → MessageBuilder (word list) → BoardClient.postMessage()
      → POST http://<board>/api/message → LED board renders
```

The LED board is the source of truth; the pad is a thin HTTP client of its
existing `/api/*` endpoints. The pad polls `GET /api/status` to show what the
board is currently displaying.

## Hardware

- **Board**: Elecrow CrowPanel 7" HMI Display (WZ8048C070, *not* the "Advance" series)
- **MCU**: ESP32-S3-WROOM-1-N4R8 (4MB Flash, 8MB OCTAL PSRAM)
- **Display**: 800×480 RGB parallel, capacitive touch (GT911)

## Build & flash (PlatformIO)

1. Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
   (`pip install platformio`) or the VS Code extension.
2. Copy the config template and fill in your WiFi + board host:
   ```bash
   cp include/app_config.h.example include/app_config.h
   ```
   Edit `include/app_config.h` (`DEFAULT_WIFI_SSID`, `DEFAULT_WIFI_PASS`,
   `DEFAULT_BOARD_HOST`). **`app_config.h` is gitignored**, so your WiFi
   credentials are never committed.
3. Connect the CrowPanel over USB-C and:

```bash
pio run                 # compile
pio run -t upload       # flash
pio device monitor      # serial logs @ 115200
pio run -t uploadfs     # (optional) upload data/ to LittleFS — used from M3
```

## First-boot checklist

- **Blank screen / rolling image / wrong colors** → re-check `include/board_pins.h`
  against the Elecrow sample for your exact unit; flip `LV_COLOR_16_SWAP` in
  `include/lv_conf.h` if red/blue are swapped.
- **Dead touch** → try the alternate GT911 address `0x14` (see `board_pins.h`).
- **Boot crash on framebuffer alloc** → confirm `board_build.arduino.memory_type = qio_opi`
  in `platformio.ini` (this board has OCTAL PSRAM).

## Status

Implements **M1–M2**: bring-up, WiFi, API client, and the word-picker UI.
See `docs/specification.md` for the full scope and the M3–M4 roadmap
(color/brightness/presets, LittleFS content, NVS provisioning, async networking).
