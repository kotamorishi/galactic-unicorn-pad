# Galactic Unicorn Pad — Specification v0.1

## Overview

Elecrow CrowPanel 7"（ESP32-S3）を使った据え置きタッチコントローラー。隣の電光掲示板
`galactic-unicorn-leg`（Pimoroni Galactic Unicorn / 53x11 LED）を、ローカルネットワーク越しに
タッチ操作で操作する。USB常時給電の常設端末を想定。

中核は **単語ピッカー方式** のメッセージ作成: 画面の英単語タイルを順番にタップして文を組み立て、
プレビューで確認して送信する。

```
Touch → MessageBuilder(単語列) → BoardClient.postMessage()
      → POST /api/message → 電光掲示板が表示
```

電光掲示板が状態の真実の源。コントローラーは `/api/*` の薄いHTTPクライアントで、`GET /api/status`
を定期ポーリングして現在の表示状態を上部バーに出す。

---

## Hardware

| Item | Spec |
|------|------|
| Board | Elecrow CrowPanel 7" HMI Display (WZ8048C070, non-Advance) |
| MCU | ESP32-S3-WROOM-1-N4R8 (240MHz dual-core, 4MB Flash, 8MB OCTAL PSRAM) |
| Display | 800×480 RGB parallel (EK9716BD3 + EK73002ACGB) |
| Touch | GT911 capacitive (I2C SDA=19/SCL=20, addr 0x5D/0x14) |
| Backlight | GPIO2 (direct; CH422G not used on this model) |
| Connectivity | WiFi 2.4GHz, BT5.0 |
| Power | USB-C 5V |

RGBピン/タイミングは `include/board_pins.h` 参照（Elecrow公式7"チュートリアルから転記）。

## Software

| Item | Technology |
|------|------------|
| Language | C++ (Arduino framework) |
| Build | PlatformIO (env `crowpanel-7`) |
| UI | LVGL 8.3.x |
| Panel | Arduino_GFX (`Arduino_ESP32RGBPanel` + `Arduino_RGB_Display`) |
| Touch | TAMC_GT911 |
| HTTP/JSON | HTTPClient + ArduinoJson 7 |
| Persistence | Preferences (NVS) / LittleFS |

---

## Features

### F-01: Word-Picker Message Builder（M2・実装済み）

| Item | Spec |
|------|------|
| 単語タイル | カテゴリ別（NAMES / CHORES / WORDS / STATUS）の英大文字タイルを `lv_btnmatrix` で表示 |
| 組み立て | タイルをタップすると単語が自動スペース区切りで末尾に追加 |
| プレビュー | 上部に組み立て中テキストをライブ表示（大フォント、長文は循環スクロール） |
| 文字カウンタ | `nn/128`。上限の90%超で赤表示 |
| 編集 | SPACE（空白追加）/ DEL（末尾単語削除）/ CLEAR（全消去） |
| 上限 | 128文字（電光掲示板制約）。超過する追加は拒否 |
| 送信 | SEND → `POST /api/message`。送信中は「sending...」表示 |
| 単語データ | 現状はソース埋め込み。M3で `data/words.json`(LittleFS) から差し替え |

### F-02: Call Button（M2・実装済み）

| Item | Spec |
|------|------|
| CALL | `POST /api/call {preset_id:1, volume:50, count:1}` で電光掲示板を鳴らしアラート表示 |

### F-03: Status Polling（M2・実装済み）

| Item | Spec |
|------|------|
| 間隔 | `STATUS_POLL_MS`（既定5秒）ごとに `GET /api/status` |
| 表示 | 上部バーに 曜日・時刻・表示中メッセージ / Off時は次回スケジュール |
| オフライン | 取得失敗時は "board offline" 表示 |

### F-04: Color / Brightness / Presets（M3・未実装）

- 色プリセット + カラーホイール。選択色を以降の `/api/message` に適用（色は送信時のみ反映）。
- 明るさスライダ → `/api/system/brightness`、音量 → `/api/system/volume`。
- 定型文プリセット画面（`data/presets.json`）から選んで読み込み。

### F-05: Provisioning & Settings（M4・未実装）

- 設定画面（オンスクリーンキーボード）でWiFi SSID/パスワード、電光掲示板ホスト(mDNS/IP)を入力し NVS 保存。
- 初回起動（NVS空）で設定画面を自動表示。

### F-06: Resilience & Performance（M4・未実装）

- WiFi切断監視 + 指数バックオフ再接続、オフラインインジケータ。
- HTTPをCore0のFreeRTOSタスク+キューに分離しUIブロックを解消。
- フレームバッファをdirect/full-refresh + bounce buffer化しtearing解消、PCLK調整。

---

## UI Layout（メイン画面 / 800×480, flex column）

```
┌────────────────────────────────────────────┐
│ Mon 08:14:02 | Displaying: HELLO  (status)  │
├────────────────────────────────────────────┤
│ EMMA KOTA LAUNDRY            (preview) 17/128│
├────────────────────────────────────────────┤
│ [ SPACE ] [ DEL ] [ CLEAR ]      (edit bar) │
├────────────────────────────────────────────┤
│ [NAMES] [CHORES] [WORDS] [STATUS] (category)│
├────────────────────────────────────────────┤
│ EMMA   KOTA   MUM                           │
│ DAD    GRAN   ALL          (word grid, grow)│
├────────────────────────────────────────────┤
│ [        SEND        ]  [   CALL   ]        │
└────────────────────────────────────────────┘
```

---

## Data Model

### MessagePayload → POST /api/message
```json
{ "text": "EMMA KOTA LAUNDRY", "display_mode": "scroll",
  "scroll_speed": "medium", "font": "bitmap8",
  "color": {"r":255,"g":255,"b":255}, "bg_color": {"r":0,"g":0,"b":0} }
```

### data/words.json（LittleFS, M3で使用）
```json
{ "categories": [ { "name": "NAMES", "words": ["EMMA","KOTA"] } ] }
```

### NVS (Preferences, namespace "pad")
`ssid, pass, host, port, r, g, b, bright, vol`

---

## Constraints

| Item | Detail |
|------|--------|
| PSRAM | OCTAL。`memory_type = qio_opi` 必須。誤るとブートクラッシュ |
| RGB timing | `board_pins.h` の値が正確でないと無表示/ローリング/色滲み |
| PCLK | 高すぎるとtearing。15MHz開始 |
| HTTP | ブロッキング。M4までUIが一時停止し得る |
| Message | 128字上限（電光掲示板制約） |
| Charset | 半角英数中心。日本語不可 |
| Flash | 4MB。OTA無の単一appスロット + ~896KB LittleFS |

---

## Testing

| Layer | How |
|-------|-----|
| `MessageBuilder` | 純ロジック。PCでユニットテスト可能（append/space/backspace/clear/128境界） |
| `BoardClient` | 電光掲示板を mock HAL + microdot でPC起動し、`curl` で疎通確認 → 実機接続 |
| 実機必須 | パネル点灯・色順、GT911タッチ精度、WiFi、長時間安定性 |

### 実機検証フロー
1. `pio run`（コンパイル）→ `pio run -t upload && pio device monitor`
2. 画面点灯・タッチ反応を目視（色順異常なら `lv_conf.h` の `LV_COLOR_16_SWAP` 反転）
3. 同一LANで `app_config.h` の既定ホストに疎通
4. 単語を組み立て SEND → Galactic Unicorn にメッセージ表示を確認
