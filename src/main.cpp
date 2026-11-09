/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * APRS LoRa+ Display: pantalla grande (Waveshare ESP32-S3-Touch-LCD-7B) que se conecta por BLE al tracker APRS LoRa+.
 */
#include <Arduino.h>
#include <cstring>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "board_lcd7b.h"
#include "protocol.h"
#include "tracker_link.h"
#include "ui.h"

static proto::Model model;

// LVGL 9: el buffer es RGB565 (LV_COLOR_DEPTH 16, sin intercambio de bytes), igual que el panel
static void flush(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
    board::draw(a->x1, a->y1, a->x2, a->y2, px);
    lv_display_flush_ready(d);
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

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== APRS LoRa+ Display " DISPLAY_VERSION " ===");
    bool ok = board::begin();

    lv_init();
    lv_tick_set_cb(tickMs);

    // Dos buffers parciales de 60 lineas en PSRAM (LVGL dibuja en uno mientras el otro se copia al panel)
    const uint32_t bytes = board::W * 60 * 2;
    void* b1 = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM);
    void* b2 = heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM);
    if (!b1 || !b2) Serial.println("[main] sin PSRAM para los buffers de LVGL");
    lv_display_t* disp = lv_display_create(board::W, board::H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, flush);
    lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, readTouch);

    trk::begin();
    ui::begin(&model);
    if (!ok) Serial.println("La placa no ha arrancado bien: revisar docs/HARDWARE_LCD7B.md");
}

// Peticiones periodicas al tracker (el resto de datos llega solo como eventos)
static void scheduler() {
    static trk::State last = trk::State::Idle;
    static uint32_t tStatus = 0, tHeard = 0, tMsgs = 0;
    trk::State s = trk::state();
    uint32_t now = millis();
    if (s == trk::State::Linked && last != trk::State::Linked) {
        trk::send(proto::cmdSimple("info"));
        trk::send(proto::cmdSimple("status"));
        trk::send(proto::cmdSub(5, 2));                // sensores cada 5 s y GNSS cada 2 s
        trk::send(proto::cmdHeard(0, 12));
        trk::send(proto::cmdMsgs(0, 10));
        tStatus = tHeard = tMsgs = now;
    }
    if (s != trk::State::Linked && last == trk::State::Linked) { model.revision++; }
    last = s;
    if (s != trk::State::Linked) return;
    if (now - tStatus > 10000) { tStatus = now; trk::send(proto::cmdSimple("status")); }
    if (now - tHeard > (ui::heardVisible() ? 5000u : 20000u)) { tHeard = now; trk::send(proto::cmdHeard(0, 12)); }
    if (now - tMsgs > (ui::messagesVisible() ? 8000u : 30000u)) { tMsgs = now; trk::send(proto::cmdMsgs(0, 10)); }
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
                Serial.println("APRS-LoRa-Plus: device=display board=waveshare_lcd7b version=" DISPLAY_VERSION " chip=ESP32-S3 ota=0");
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
    std::string line;
    int n = 0;
    while (n++ < 20 && trk::poll(line)) {
        proto::applyEvent(model, line.c_str());
        // un mensaje nuevo, enviado o con ack cambia el registro: se vuelve a pedir la lista
        if (line.find("\"e\":\"msg\"") != std::string::npos) trk::send(proto::cmdMsgs(0, 10));
    }
    scheduler();
    static uint32_t t = 0;
    if (millis() - t > 300) { t = millis(); ui::update(); }
    lv_timer_handler();
    delay(5);
}
