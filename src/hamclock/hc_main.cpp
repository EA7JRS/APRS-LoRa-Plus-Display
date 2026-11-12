/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * APRS LoRa+ Display, entorno waveshare_lcd7b_hamclock: reloj de radioaficionado en la Waveshare ESP32-S3-Touch-LCD-7B con los
 * datos de un servidor OpenHamClock (https://github.com/accius/openhamclock) por WiFi. No usa el enlace BLE con el tracker.
 * Placa, panel y tactil: los del Display (src/board.h, src/board_lcd7b.cpp).
 */
#include <Arduino.h>
#include <Preferences.h>
#include <lvgl.h>
#include "../board.h"
#include "hc_model.h"
#include "hc_net.h"
#include "hc_ui.h"

SET_LOOP_TASK_STACK_SIZE(32 * 1024);       // LVGL 9 dibuja con funciones que se llaman entre si (ver src/main.cpp)

static hc::Model model;
static hc::Config cfg;
static lv_display_t* disp = nullptr;

static void drawDone() { lv_display_flush_ready(disp); }
static void flush(lv_display_t*, const lv_area_t* a, uint8_t* px) {
    if (board::swapBytes()) lv_draw_rgb565_swap(px, (uint32_t)lv_area_get_width(a) * lv_area_get_height(a));
    board::draw(a->x1, a->y1, a->x2, a->y2, px);
}
static void readTouch(lv_indev_t*, lv_indev_data_t* data) {
    static int lx = 0, ly = 0;
    static bool pressed = false;
    int x, y;
    int r = board::touch(x, y);
    if (r == 1) { lx = x; ly = y; pressed = true; }
    else if (r == 0) pressed = false;
    data->point.x = lx; data->point.y = ly;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static uint32_t tickMs() { return millis(); }

static void loadConfig() {
    Preferences p;
    p.begin("hamclock", true);
    p.getString("call", cfg.call, sizeof(cfg.call));
    p.getString("grid", cfg.grid, sizeof(cfg.grid));
    p.getString("ssid", cfg.ssid, sizeof(cfg.ssid));
    p.getString("pass", cfg.pass, sizeof(cfg.pass));
    if (p.isKey("server")) p.getString("server", cfg.server, sizeof(cfg.server));
    if (p.isKey("tz")) p.getString("tz", cfg.tz, sizeof(cfg.tz));
    p.getString("rig", cfg.rig, sizeof(cfg.rig));
    p.end();
}
// Save en los ajustes: se guarda y se arranca de nuevo con ellos
static void saveConfig(const hc::Config& c) {
    Preferences p;
    p.begin("hamclock", false);
    p.putString("call", c.call);
    p.putString("grid", c.grid);
    p.putString("ssid", c.ssid);
    p.putString("pass", c.pass);
    p.putString("server", c.server);
    p.putString("tz", c.tz);
    p.putString("rig", c.rig);
    p.end();
    Serial.println("[main] ajustes guardados: reinicio");
    delay(200);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== APRS LoRa+ Display " DISPLAY_VERSION " (" BOARD_ENV "_hamclock) ===");
    Serial.println("[INFO][Main] APRS LoRa+ (c) 2026 Manuel - EA7JRS");
    bool ok = board::begin();
    loadConfig();

    lv_init();
    lv_tick_set_cb(tickMs);
    const uint32_t bytes = (uint32_t)board::W * board::drawLines() * 2;
    void* b1 = board::allocDrawBuffer(bytes);
    void* b2 = board::allocDrawBuffer(bytes);
    if (!b1 || !b2) Serial.println("[main] sin memoria para los buffers de LVGL");
    disp = lv_display_create(board::W, board::H);
    board::setDrawDone(drawDone);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush);
    lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, readTouch);

    hcnet::begin(&model, &cfg);
    hcui::begin(&model, &cfg, saveConfig, hcnet::tune);
    if (!ok) Serial.println("La placa no ha arrancado bien: revisar docs/HARDWARE.md");
}

// Instalador web: por USB contesta a "APRS-INFO?" como el resto de dispositivos APRS LoRa+.
static void usbInfo() {
    static char buf[16];
    static uint8_t n = 0;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            buf[n] = 0;
            if (n && strcmp(buf, "APRS-INFO?") == 0)
                Serial.println("APRS-LoRa-Plus: device=display board=" BOARD_ENV "_hamclock version=" DISPLAY_VERSION " chip=ESP32-S3 ota=0");
            n = 0;
        } else if (n < sizeof(buf) - 1) {
            buf[n++] = c;
        } else {
            n = 0;
        }
    }
}

void loop() {
    usbInfo();
    static uint32_t t = 0;
    if (millis() - t > 250) {
        t = millis();
        hcnet::lock();
        hcui::update();
        hcnet::unlock();
    }
    lv_timer_handler();
    static uint32_t ts = 0;
    if (millis() - ts > 60000) {
        ts = millis();
        Serial.printf("[main] pila libre: %u bytes, RAM libre: %u, PSRAM libre: %u\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr),
                      (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    }
    delay(5);
}
