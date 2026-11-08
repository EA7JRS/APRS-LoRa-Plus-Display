/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * APRS LoRa+ Display: pantalla grande (Waveshare ESP32-S3-Touch-LCD-7B) que se conecta por BLE al tracker APRS LoRa+.
 */
#include <Arduino.h>
#include <cstring>
#include <lvgl.h>
#include <esp_timer.h>
#include "board_lcd7b.h"
#include "protocol.h"
#include "tracker_link.h"
#include "ui.h"

static proto::Model model;
static lv_disp_draw_buf_t drawBuf;
static lv_disp_drv_t dispDrv;
static lv_indev_drv_t indevDrv;

static void flush(lv_disp_drv_t* d, const lv_area_t* a, lv_color_t* px) {
    board::draw(a->x1, a->y1, a->x2, a->y2, px);
    lv_disp_flush_ready(d);
}

static void readTouch(lv_indev_drv_t*, lv_indev_data_t* data) {
    static int lx = 0, ly = 0;
    static bool pressed = false;
    int x, y;
    int r = board::touch(x, y);
    if (r == 1) { lx = x; ly = y; pressed = true; }
    else if (r == 0) pressed = false;
    data->point.x = lx; data->point.y = ly;
    data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

static void tick(void*) { lv_tick_inc(2); }

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== APRS LoRa+ Display " DISPLAY_VERSION " ===");
    bool ok = board::begin();

    lv_init();
    const size_t px = board::W * 60;
    lv_color_t* b1 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t* b2 = (lv_color_t*)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&drawBuf, b1, b2, px);
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = board::W;
    dispDrv.ver_res = board::H;
    dispDrv.flush_cb = flush;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);
    lv_indev_drv_init(&indevDrv);
    indevDrv.type = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = readTouch;
    lv_indev_drv_register(&indevDrv);

    const esp_timer_create_args_t ta = {.callback = tick, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK, .name = "lvtick", .skip_unhandled_events = true};
    esp_timer_handle_t th;
    esp_timer_create(&ta, &th);
    esp_timer_start_periodic(th, 2000);

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
