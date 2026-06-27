# CLAUDE.md - Galactic Unicorn Pad (Touch Controller)

## Project Overview

Elecrow CrowPanel 7" (ESP32-S3) を使った **据え置きタッチコントローラー**。隣接プロジェクト
`galactic-unicorn-leg`（Pimoroni Galactic Unicorn / 53x11 LED電光掲示板）を、スマホやWebブラウザを
介さずに操作する。

中核UIは **単語ピッカー**: 画面に並ぶ英単語タイルを順番にタップしてメッセージを組み立て、
プレビューで確認し、「SEND」で電光掲示板の HTTP API に送信する。

- 仕様書: `docs/specification.md`
- 操作対象API: `../galactic-unicorn-leg/src/web/routes.py`

## Hardware

- **Board**: Elecrow CrowPanel 7" HMI Display（型番 WZ8048C070。**"Advance"系ではない**）
- **MCU**: ESP32-S3-WROOM-1-N4R8（240MHz dual-core LX7, 4MB Flash, **8MB OCTAL PSRAM**）
- **Display**: 800×480 RGB parallel（ドライバ EK9716BD3 + EK73002ACGB）
- **Touch**: 静電容量 GT911（I2C, SDA=19 / SCL=20, addr 0x5D or 0x14）
- **Backlight**: GPIO2 直結（このモデルはCH422G非使用）
- **Connectivity**: WiFi 2.4GHz + BT5.0

ピン定義/タイミングは `include/board_pins.h`（Elecrow公式7"チュートリアルから転記）。

## Tech Stack

- **Language**: C++ (Arduino framework)
- **Build**: PlatformIO（`platformio.ini`, env `crowpanel-7`）
- **UI**: LVGL 8.3.x
- **Panel driver**: Arduino_GFX（`Arduino_ESP32RGBPanel` + `Arduino_RGB_Display`）
- **Touch**: TAMC_GT911
- **HTTP/JSON**: HTTPClient + ArduinoJson 7
- **Persistence**: Preferences (NVS) / LittleFS（編集可能な単語・プリセット, M3〜）

## Critical Constraints

ハードウェアの硬い制約。違反するとブートクラッシュや無表示になる。

- **PSRAMは OCTAL**: `board_build.arduino.memory_type = qio_opi` 必須。誤ると
  `heap_caps_malloc(MALLOC_CAP_SPIRAM)` が失敗し、フレームバッファ確保不能でブートクラッシュ。
- **RGBピン/タイミングは正確に**: `board_pins.h` の値が1つでも違うと無表示・画像ローリング・色滲み。
  リビジョン差があるため、不調時は実機のElecrowサンプルと付き合わせる。
- **CH422G**: 一部の別リビジョンはバックライト/リセットをCH422G I2Cエクスパンダ経由で駆動する。
  本モデルは直結。`BOARD_USE_CH422G` で切り替え可能（既定OFF）。
- **PCLKは控えめに**: PSRAMフレームバッファ帯域の都合で高PCLKはtearingの原因。15MHz開始。
- **HTTPはブロッキング**: `HTTPClient` はUIスレッドを数百ms止める。M1〜M2では「sending...」表示の裏で
  同期送信を許容。M4でCore0のFreeRTOSタスク+キューに分離する。
- **メッセージは128字上限**: 電光掲示板側の制約。`MessageBuilder` で強制（`MESSAGE_MAX_CHARS`）。
- **半角英数中心**: 電光掲示板のフォントはbitmap6/bitmap8。単語は英大文字想定、日本語入力なし。

## Project Structure

```
galactic-unicorn-pad/
├── CLAUDE.md / README.md
├── platformio.ini / partitions_4mb.csv
├── docs/specification.md
├── include/                  # -I include
│   ├── lv_conf.h             # LVGL設定（color16, アロケータ→PSRAM, フォント）
│   ├── board_pins.h          # CrowPanel 7" RGB+GT911 ピン/タイミング
│   └── app_config.h          # 既定WiFi/ホスト/上限/ポーリング
├── data/                     # LittleFS (pio run -t uploadfs)。M3で使用
│   ├── words.json / presets.json
└── src/                      # -I src
    ├── main.cpp              # setup()初期化順 / loop()
    ├── display/{display,touch}.{h,cpp}   # RGBパネル+LVGL / GT911
    ├── net/{wifi_manager,board_client}.{h,cpp}  # WiFi/mDNS / /api/* クライアント
    ├── data/settings.{h,cpp}             # NVS (Preferences)
    ├── model/message_builder.{h,cpp}     # 組み立て中メッセージ
    └── ui/{ui,screen_main}.{h,cpp}       # 画面管理 / 単語ピッカー
```

## Coding Guidelines

### 初期化順（main.cpp）
`settings::load` → `display::begin`（CH422G→backlight→RGB→LVGL）→ `touch::begin`
→ `wifimgr::begin` → `resolveHost` → `board.begin` → `ui::init`。各段は失敗してもログして継続
（boot must succeed の精神は leg と共通）。

### 責務分離
- `net/board_client` は **UI非依存の純HTTP**。LVGLを参照しない。
- `model/message_builder` はハードもネットも知らない純ロジック（PCでテスト可能）。
- `ui/` のみ LVGL を参照。コールバックは static ポインタ経由で board/builder にアクセス。

### メモリ
- LVGLのアロケータは `lv_conf.h` で PSRAM（`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`）に向ける。
- ドローバッファは部分バッファ（800×80 ×2）をPSRAMに確保。全画面ダブルバッファ化はM4。
- ArduinoJsonは v7 `JsonDocument`（`DynamicJsonDocument` は非推奨）。

### Naming
- Files: `snake_case`、Classes: `PascalCase`、関数: `snake_case`、定数: `UPPER_SNAKE_CASE`
- private/static グローバルは `g_`/`s_` prefix

## LED Board API (操作対象)

| Method | Path | Body |
|--------|------|------|
| GET | `/api/status` | — |
| POST | `/api/message` | `{text, display_mode, scroll_speed, color:{r,g,b}, bg_color, font}` |
| POST | `/api/system/brightness` | `{brightness_offset}` |
| POST | `/api/system/volume` | `{volume}` |
| POST | `/api/call` | `{preset_id, volume, count}` |
| GET | `/api/sound/presets` | — |

色は `/api/message` の送信時のみ反映（独立color APIなし）。

## Roadmap

- **M1**（完了）: 点灯+タッチ+WiFi+固定メッセージ送信
- **M2**（完了）: 単語ピッカー（プレビュー/編集/カテゴリ/単語グリッド/SEND/CALL）
- **M3**: LittleFSから単語・プリセット、色/明るさ/プリセット画面
- **M4**: NVS設定・WiFiプロビジョニング画面、Core0ネットタスク、フレームバッファ最適化
