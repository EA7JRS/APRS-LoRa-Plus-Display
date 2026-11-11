/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Waveshare ESP32-S3-Touch-LCD-7 (sin B; rev 1.2): panel RGB 800x480, tactil GT911, expansor CH422G.
 * Mismos pines que la 7B; cambian el expansor, la resolucion y los tiempos. Fuente: waveshareteam/ESP32-S3-Touch-LCD-7
 * (examples/ESP-IDF/09_lvgl_v9_demo/components/waveshare_rgb_lcd_port.*).
 */
#include "board.h"
#if defined(BOARD_LCD7)
#include "board_common.h"
#include <Wire.h>

#ifndef LCD_PCLK_HZ
#define LCD_PCLK_HZ 16000000
#endif

namespace board {
using namespace common;

constexpr int PIN_SDA = 8, PIN_SCL = 9, PIN_TP_INT = 4;
constexpr int PIN_VSYNC = 3, PIN_HSYNC = 46, PIN_DE = 5, PIN_PCLK = 7;
// data[0..4] = B3..B7, data[5..10] = G2..G7, data[11..15] = R3..R7
constexpr int DATA_PINS[16] = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40};

// CH422G: no tiene registros; cada funcion es una direccion I2C a la que se escribe un byte. 0x24 = modo (0x01: IO0-7 como
// salidas), 0x38 = salidas IO0-7. Por eso en el escaneo "contestan" 0x20-0x27 y 0x30-0x3F.
// EXIO1 reset tactil, EXIO2 luz (DISP, solo encendida / apagada), EXIO3 reset LCD, EXIO4 CS de la TF, EXIO5 USB/CAN (alto = CAN).
constexpr uint8_t CH_MODE = 0x24, CH_OUT = 0x38;
constexpr uint8_t EXIO_TP_RST = 1 << 1, EXIO_DISP = 1 << 2, EXIO_LCD_RST = 1 << 3, EXIO_SD_CS = 1 << 4;
static uint8_t ioxOut = 0;
static bool chWrite(uint8_t addr, uint8_t v) {
    Wire.beginTransmission(addr);
    Wire.write(v);
    return Wire.endTransmission() == 0;
}
static bool ioxSet(uint8_t mask, bool on) {
    if (on) ioxOut |= mask; else ioxOut &= ~mask;
    return chWrite(CH_OUT, ioxOut);
}

static uint8_t gtAddr = 0x5D;
static void (*doneCb)() = nullptr;

bool begin() {
    Wire.begin(PIN_SDA, PIN_SCL, 400000);
    delay(10);
    i2cScan();                                                    // CH422G: 0x20-0x27 y 0x30-0x3F; tactil 0x5D o 0x14 tras su reset

    if (!chWrite(CH_MODE, 0x01)) { Serial.println("[board] el expansor CH422G no responde"); return false; }
    ioxOut = EXIO_SD_CS | EXIO_LCD_RST;                           // TF sin seleccionar, luz apagada, USB (no CAN)
    chWrite(CH_OUT, ioxOut);
    delay(10);

    // Reset del GT911 con INT bajo: direccion 0x5D (secuencia del ejemplo de Waveshare)
    pinMode(PIN_TP_INT, OUTPUT); digitalWrite(PIN_TP_INT, LOW);
    ioxSet(EXIO_TP_RST, false); delay(100);
    ioxSet(EXIO_TP_RST, true); delay(200);
    pinMode(PIN_TP_INT, INPUT);
    if (!i2cProbe(0x5D)) {
        if (i2cProbe(0x14)) gtAddr = 0x14;
        else Serial.println("[board] aviso: el tactil GT911 no responde");
    }

    ioxSet(EXIO_LCD_RST, false); delay(20);
    ioxSet(EXIO_LCD_RST, true); delay(120);

    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    cfg.timings.pclk_hz = LCD_PCLK_HZ;
    cfg.timings.h_res = W;
    cfg.timings.v_res = H;
    cfg.timings.hsync_pulse_width = 4;                            // tiempos de waveshare_rgb_lcd_port.c
    cfg.timings.hsync_back_porch = 8;
    cfg.timings.hsync_front_porch = 8;
    cfg.timings.vsync_pulse_width = 4;
    cfg.timings.vsync_back_porch = 8;
    cfg.timings.vsync_front_porch = 8;
    cfg.timings.flags.pclk_active_neg = 1;
    cfg.data_width = 16;
    cfg.bits_per_pixel = 16;
    cfg.num_fbs = 1;
    cfg.bounce_buffer_size_px = W * 10;
    cfg.hsync_gpio_num = PIN_HSYNC;
    cfg.vsync_gpio_num = PIN_VSYNC;
    cfg.de_gpio_num = PIN_DE;
    cfg.pclk_gpio_num = PIN_PCLK;
    cfg.disp_gpio_num = -1;
    for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = DATA_PINS[i];
    cfg.flags.fb_in_psram = 1;
    if (!rgbPanelBegin(cfg)) return false;

    setBacklight(100);
    Serial.printf("[board] LCD-7: panel listo (pclk %u Hz), tactil en 0x%02X\n", (unsigned)LCD_PCLK_HZ, gtAddr);
    return true;
}

void* allocDrawBuffer(size_t bytes) { return heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM); }
int drawLines() { return 60; }
bool swapBytes() { return false; }
void setDrawDone(void (*cb)()) { doneCb = cb; }
void draw(int x1, int y1, int x2, int y2, void* px) {
    rgbPanelDraw(x1, y1, x2, y2, px);
    if (doneCb) doneCb();
}
int touch(int& x, int& y) { return gt911Read(gtAddr, x, y, W, H); }

void setBacklight(int percent) { ioxSet(EXIO_DISP, percent > 0); }   // esta placa no regula el brillo: encendida o apagada

}
#endif
