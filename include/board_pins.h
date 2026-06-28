// board_pins.h — Elecrow CrowPanel 7" (WZ8048C070 / ESP32-S3-WROOM-1-N4R8)
//
// RGB parallel panel + GT911 touch pin map and timing.
// Values transcribed from Elecrow's official 7.0" Arduino tutorial:
//   https://www.elecrow.com/wiki/ESP32_Display_7.0-inch_HMI_Arduino_Tutorial.html
//
// IMPORTANT (gates everything — verify against YOUR board revision):
//   * If the screen stays dark or the image rolls/tears, re-check these against
//     the Elecrow sample shipped for your exact unit (revisions differ).
//   * This non-"Advance" 7" model drives the backlight directly on GPIO2 and
//     does NOT use a CH422G IO expander. Some other CrowPanel revisions route
//     backlight/touch-reset through a CH422G — if yours does, set
//     BOARD_USE_CH422G to 1 and fill in the expander details in display.cpp.
#pragma once

// ---- Panel resolution ----
#define PANEL_WIDTH   800
#define PANEL_HEIGHT  480

// ---- RGB control pins ----
#define PIN_DE        41
#define PIN_VSYNC     40
#define PIN_HSYNC     39
#define PIN_PCLK      0

// ---- RGB data pins (R0-R4, G0-G5, B0-B4) ----
#define PIN_R0        14
#define PIN_R1        21
#define PIN_R2        47
#define PIN_R3        48
#define PIN_R4        45
#define PIN_G0        9
#define PIN_G1        46
#define PIN_G2        3
#define PIN_G3        8
#define PIN_G4        16
#define PIN_G5        1
#define PIN_B0        15
#define PIN_B1        7
#define PIN_B2        6
#define PIN_B3        5
#define PIN_B4        4

// ---- RGB timing (Arduino_ESP32RGBPanel order) ----
#define RGB_HSYNC_POLARITY      0
#define RGB_HSYNC_FRONT_PORCH   40
#define RGB_HSYNC_PULSE_WIDTH   48
#define RGB_HSYNC_BACK_PORCH    40
#define RGB_VSYNC_POLARITY      0
#define RGB_VSYNC_FRONT_PORCH   1
#define RGB_VSYNC_PULSE_WIDTH   31
#define RGB_VSYNC_BACK_PORCH    13
#define RGB_PCLK_ACTIVE_NEG     1
// Refresh = PCLK / ((800+128) * (480+45)) = PCLK / 487200.
//   15 MHz -> ~31 Hz, 18 MHz -> ~37 Hz, 21 MHz -> ~43 Hz.
// 18 MHz caused rolling/drift on this unit (PSRAM bandwidth ceiling without a
// bounce buffer, which needs Arduino_GFX >=1.6.0). Keep at the stable 15 MHz.
#define RGB_PREFER_SPEED        15000000  // 15 MHz (~31 Hz refresh)

// ---- Backlight ----
// Direct GPIO on this model. (Behind CH422G on some other revisions.)
#define PIN_BACKLIGHT 2

// ---- GT911 capacitive touch (I2C) ----
#define PIN_TOUCH_SDA 19
#define PIN_TOUCH_SCL 20
// RST/INT are not broken out as plain GPIO on this model's sample; -1 = unused
// (GT911 is polled over I2C). If touch is dead, also try the alternate address.
#define PIN_TOUCH_RST -1
#define PIN_TOUCH_INT -1
#define TOUCH_GT911_ADDR  0x5D   // alternate: 0x14 (latched by INT level at reset)

// ---- Optional CH422G IO expander (off for this model) ----
#define BOARD_USE_CH422G  0
#define CH422G_I2C_ADDR   0x24   // (only relevant if BOARD_USE_CH422G == 1)
