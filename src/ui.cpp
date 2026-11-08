/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz LVGL 8.4 para 1024x600 tactil. Textos en ingles y sin acentos (las fuentes integradas de LVGL no los llevan).
 */
#include "ui.h"
#include "board_lcd7b.h"
#include "tracker_link.h"
#include <lvgl.h>
#include <cmath>
#include <cstdio>

namespace ui {

static proto::Model* M = nullptr;
static uint32_t drawnRev = 0xFFFFFFFF;
static lv_obj_t *tabs, *tabDash, *tabHeard, *tabMsgs, *tabSens, *tabSat, *tabMirror, *tabPages, *tabSet;
static lv_obj_t *kb;

// barra superior
static lv_obj_t *topLink, *topInfo;
// dashboard
static lv_obj_t *dashGnss, *dashStatus, *dashErr, *ddMode, *swDigi;
// heard
static lv_obj_t* tblHeard;
// mensajes
static lv_obj_t *tblMsgs, *taTo, *taText, *lblSendRes;
// sensores
static lv_obj_t* lblSens;
// sat
static lv_obj_t *lblSat, *lblRot, *arcAz, *barEl;
// espejo
static lv_obj_t *lblMirrorTitle, *lblMirror;
// dapnet
static lv_obj_t* tblPages;
// ajustes
static lv_obj_t *taPin, *taPeer, *lblSetInfo, *slBack;

static const lv_font_t* F_BIG = &lv_font_montserrat_24;
static const lv_font_t* F_MONO = &lv_font_unscii_16;

static float distKm(double la1, double lo1, double la2, double lo2) {
    const double R = 6371.0, d2r = M_PI / 180.0;
    double dla = (la2 - la1) * d2r, dlo = (lo2 - lo1) * d2r;
    double a = sin(dla / 2) * sin(dla / 2) + cos(la1 * d2r) * cos(la2 * d2r) * sin(dlo / 2) * sin(dlo / 2);
    return (float)(R * 2 * atan2(sqrt(a), sqrt(1 - a)));
}

static void fmtAge(char* b, size_t n, int s) {
    if (s < 60) snprintf(b, n, "%ds", s);
    else if (s < 3600) snprintf(b, n, "%dm", s / 60);
    else snprintf(b, n, "%dh%02dm", s / 3600, (s % 3600) / 60);
}

static lv_obj_t* label(lv_obj_t* parent, const char* t, const lv_font_t* f = nullptr) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, t);
    if (f) lv_obj_set_style_text_font(l, f, 0);
    return l;
}

static lv_obj_t* button(lv_obj_t* parent, const char* t, lv_event_cb_t cb, void* ud = nullptr, int w = 220) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_size(b, w, 56);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, t);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

static void fieldFocus(lv_event_t* e) {
    lv_obj_t* ta = lv_event_get_target(e);
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_FOCUSED) { lv_keyboard_set_textarea(kb, ta); lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN); }
    else if (c == LV_EVENT_DEFOCUSED) { lv_keyboard_set_textarea(kb, nullptr); lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN); }
}

static lv_obj_t* field(lv_obj_t* parent, const char* placeholder, int w, int maxLen, const char* accepted = nullptr) {
    lv_obj_t* ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_max_length(ta, maxLen);
    if (accepted) lv_textarea_set_accepted_chars(ta, accepted);
    lv_obj_set_width(ta, w);
    lv_obj_add_event_cb(ta, fieldFocus, LV_EVENT_ALL, nullptr);
    return ta;
}

static void tableStyle(lv_obj_t* t) {
    lv_obj_set_style_text_font(t, &lv_font_montserrat_16, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(t, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(t, 6, LV_PART_ITEMS);
}

// ---------- acciones ----------
static void onBeacon(lv_event_t*) { trk::send(proto::cmdSimple("beacon")); }
static void onDigi(lv_event_t* e) {
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    trk::send("{\"c\":\"digi\",\"on\":" + std::string(on ? "true" : "false") + "}\n");
}
static void onModeConfirm(lv_event_t* e) {
    lv_obj_t* mb = lv_event_get_current_target(e);
    uint16_t btn = lv_msgbox_get_active_btn(mb);
    if (btn == 0 && M) {
        uint16_t sel = lv_dropdown_get_selected(ddMode);
        if (sel < M->info.modeIds.size()) trk::send(proto::cmdMode(M->info.modeIds[sel]));
    }
    lv_msgbox_close(mb);
}
static void onModeSet(lv_event_t*) {
    static const char* btns[] = {"Switch", "Cancel", ""};
    lv_obj_t* mb = lv_msgbox_create(nullptr, "Change tracker mode", "The tracker restarts in the selected mode.", btns, false);
    lv_obj_center(mb);
    lv_obj_add_event_cb(mb, onModeConfirm, LV_EVENT_VALUE_CHANGED, nullptr);
}
static void onSend(lv_event_t*) {
    const char* to = lv_textarea_get_text(taTo);
    const char* tx = lv_textarea_get_text(taText);
    if (!*to || !*tx) { lv_label_set_text(lblSendRes, "Enter a callsign and a text"); return; }
    trk::send(proto::cmdMsgSend(to, tx, true));
    lv_textarea_set_text(taText, "");
    lv_label_set_text(lblSendRes, "Sent to the tracker (it goes out with the next window)");
    trk::send(proto::cmdMsgs(0, 10));
}
static void onTblMsgs(lv_event_t*) {
    uint16_t r, c;
    lv_table_get_selected_cell(tblMsgs, &r, &c);
    if (r >= 1 && M && r - 1 < M->msgs.size() && !M->msgs[r - 1].call.empty()) lv_textarea_set_text(taTo, M->msgs[r - 1].call.c_str());
}
static void onSavePin(lv_event_t*) {
    trk::setPin((uint32_t)atol(lv_textarea_get_text(taPin)));
    trk::setPeerAddress(lv_textarea_get_text(taPeer));
    trk::reconnect();
}
static void onForget(lv_event_t*) { trk::forgetPairing(); }
static void onReconnect(lv_event_t*) { trk::reconnect(); }
static void onBack(lv_event_t* e) { board::setBacklight(lv_slider_get_value(lv_event_get_target(e))); }

static void onTab(lv_event_t*) {
    // al abrir cada pestaña se piden sus datos al momento
    uint16_t t = lv_tabview_get_tab_act(tabs);
    if (t == 1) trk::send(proto::cmdHeard(0, 12));
    if (t == 2) trk::send(proto::cmdMsgs(0, 10));
    if (t == 5) trk::send(proto::cmdScreen(2)); else trk::send(proto::cmdScreen(0));
    if (t == 6) trk::send(proto::cmdSimple("dapnet"));
}

// ---------- construccion ----------
static void buildDash() {
    lv_obj_set_flex_flow(tabDash, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tabDash, 10, 0);
    dashGnss = label(tabDash, "GNSS: -", F_BIG);
    dashStatus = label(tabDash, "Tracker: -", F_BIG);
    dashErr = label(tabDash, "");
    lv_obj_set_style_text_color(dashErr, lv_palette_main(LV_PALETTE_RED), 0);

    lv_obj_t* row = lv_obj_create(tabDash);
    lv_obj_set_size(row, LV_PCT(100), 80);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 16, 0);
    button(row, "Beacon now", onBeacon);
    label(row, "Digipeater");
    swDigi = lv_switch_create(row);
    lv_obj_add_event_cb(swDigi, onDigi, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* row2 = lv_obj_create(tabDash);
    lv_obj_set_size(row2, LV_PCT(100), 80);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row2, 16, 0);
    label(row2, "Mode");
    ddMode = lv_dropdown_create(row2);
    lv_obj_set_width(ddMode, 320);
    lv_dropdown_set_options(ddMode, "-");
    button(row2, "Switch mode", onModeSet);
}

static void buildHeard() {
    tblHeard = lv_table_create(tabHeard);
    lv_obj_set_size(tblHeard, LV_PCT(100), LV_PCT(100));
    tableStyle(tblHeard);
    lv_table_set_col_cnt(tblHeard, 7);
    const int w[7] = {170, 70, 100, 90, 80, 90, 330};
    for (int i = 0; i < 7; i++) lv_table_set_col_width(tblHeard, i, w[i]);
    const char* h[7] = {"Station", "Sym", "Dist", "RSSI", "SNR", "Age", "Comment"};
    for (int i = 0; i < 7; i++) lv_table_set_cell_value(tblHeard, 0, i, h[i]);
}

static void buildMsgs() {
    lv_obj_set_flex_flow(tabMsgs, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tabMsgs, 8, 0);
    tblMsgs = lv_table_create(tabMsgs);
    lv_obj_set_size(tblMsgs, LV_PCT(100), 280);
    tableStyle(tblMsgs);
    lv_table_set_col_cnt(tblMsgs, 4);
    const int w[4] = {60, 170, 560, 120};
    for (int i = 0; i < 4; i++) lv_table_set_col_width(tblMsgs, i, w[i]);
    const char* h[4] = {"", "Station", "Text", "State"};
    for (int i = 0; i < 4; i++) lv_table_set_cell_value(tblMsgs, 0, i, h[i]);
    lv_obj_add_event_cb(tblMsgs, onTblMsgs, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* row = lv_obj_create(tabMsgs);
    lv_obj_set_size(row, LV_PCT(100), 80);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 12, 0);
    taTo = field(row, "To (EA7XXX-9)", 200, 9);
    taText = field(row, "Message (max 67)", 460, 67);
    button(row, "Send", onSend, nullptr, 120);
    lblSendRes = label(tabMsgs, "");
}

static void buildSens() { lblSens = label(tabSens, "-", F_BIG); }

static void buildSat() {
    lv_obj_set_flex_flow(tabSat, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(tabSat, 24, 0);
    lv_obj_t* left = lv_obj_create(tabSat);
    lv_obj_set_size(left, 520, LV_PCT(100));
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lblSat = label(left, "SAT mode inactive", F_BIG);
    lblRot = label(left, "", F_BIG);
    lv_obj_t* right = lv_obj_create(tabSat);
    lv_obj_set_size(right, 340, LV_PCT(100));
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(right, "Azimuth");
    arcAz = lv_arc_create(right);
    lv_obj_set_size(arcAz, 220, 220);
    lv_arc_set_range(arcAz, 0, 360);
    lv_arc_set_bg_angles(arcAz, 0, 360);
    lv_arc_set_rotation(arcAz, 270);
    lv_obj_remove_style(arcAz, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arcAz, LV_OBJ_FLAG_CLICKABLE);
    label(right, "Elevation");
    barEl = lv_bar_create(right);
    lv_obj_set_size(barEl, 260, 24);
    lv_bar_set_range(barEl, 0, 90);
}

static void buildMirror() {
    lv_obj_set_flex_flow(tabMirror, LV_FLEX_FLOW_COLUMN);
    lblMirrorTitle = label(tabMirror, "Tracker screen (mirror)", F_BIG);
    lblMirror = label(tabMirror, "Open this tab with the tracker linked", F_MONO);
}

static void buildPages() {
    tblPages = lv_table_create(tabPages);
    lv_obj_set_size(tblPages, LV_PCT(100), LV_PCT(100));
    tableStyle(tblPages);
    lv_table_set_col_cnt(tblPages, 3);
    lv_table_set_col_width(tblPages, 0, 110); lv_table_set_col_width(tblPages, 1, 100); lv_table_set_col_width(tblPages, 2, 640);
    lv_table_set_cell_value(tblPages, 0, 0, "RIC"); lv_table_set_cell_value(tblPages, 0, 1, "RSSI"); lv_table_set_cell_value(tblPages, 0, 2, "Text");
}

static void buildSet() {
    lv_obj_set_flex_flow(tabSet, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tabSet, 10, 0);
    lblSetInfo = label(tabSet, "", F_BIG);
    label(tabSet, "Tracker control PIN (WebUI > Bluetooth > Control PIN, default 123456)");
    taPin = field(tabSet, "PIN", 220, 6, "0123456789");
    char b[8]; snprintf(b, sizeof b, "%06lu", (unsigned long)trk::pin()); lv_textarea_set_text(taPin, b);
    label(tabSet, "Tracker address (empty = first tracker heard)");
    taPeer = field(tabSet, "AA:BB:CC:DD:EE:FF", 320, 17);
    lv_textarea_set_text(taPeer, trk::peerAddress().c_str());
    lv_obj_t* row = lv_obj_create(tabSet);
    lv_obj_set_size(row, LV_PCT(100), 80);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 12, 0);
    button(row, "Save and reconnect", onSavePin, nullptr, 260);
    button(row, "Forget pairing", onForget, nullptr, 220);
    button(row, "Reconnect", onReconnect, nullptr, 180);
    label(tabSet, "Backlight");
    slBack = lv_slider_create(tabSet);
    lv_obj_set_width(slBack, 400);
    lv_slider_set_range(slBack, 5, 100);
    lv_slider_set_value(slBack, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(slBack, onBack, LV_EVENT_VALUE_CHANGED, nullptr);
}

void begin(proto::Model* model) {
    M = model;
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_text_font(scr, &lv_font_montserrat_20, 0);

    lv_obj_t* top = lv_obj_create(scr);
    lv_obj_set_size(top, board::W, 48);
    lv_obj_set_pos(top, 0, 0);
    lv_obj_set_style_radius(top, 0, 0);
    label(top, "APRS LoRa+ Display " DISPLAY_VERSION, F_BIG);
    topLink = label(top, "", nullptr);
    lv_obj_align(topLink, LV_ALIGN_RIGHT_MID, -6, 0);
    topInfo = label(top, "", nullptr);
    lv_obj_align(topInfo, LV_ALIGN_CENTER, 90, 0);

    tabs = lv_tabview_create(scr, LV_DIR_LEFT, 150);
    lv_obj_set_size(tabs, board::W, board::H - 48);
    lv_obj_set_pos(tabs, 0, 48);
    tabDash = lv_tabview_add_tab(tabs, "Status");
    tabHeard = lv_tabview_add_tab(tabs, "Heard");
    tabMsgs = lv_tabview_add_tab(tabs, "Messages");
    tabSens = lv_tabview_add_tab(tabs, "Sensors");
    tabSat = lv_tabview_add_tab(tabs, "SAT / Rotor");
    tabMirror = lv_tabview_add_tab(tabs, "Mirror");
    tabPages = lv_tabview_add_tab(tabs, "DAPNET");
    tabSet = lv_tabview_add_tab(tabs, "Settings");
    lv_obj_add_event_cb(tabs, onTab, LV_EVENT_VALUE_CHANGED, nullptr);

    kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(kb, board::W, 250);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    buildDash(); buildHeard(); buildMsgs(); buildSens(); buildSat(); buildMirror(); buildPages(); buildSet();
}

// ---------- actualizacion ----------
static const char* stName(int st) {
    switch (st) { case 0: return "new"; case 1: return "read"; case 2: return "sent"; case 3: return "waiting ack"; case 4: return "ack"; case 5: return "failed"; default: return "?"; }
}

void update() {
    if (!M) return;
    char b[512];
    trk::State s = trk::state();
    snprintf(b, sizeof b, "%s%s%s", trk::stateText(), s == trk::State::Linked ? ": " : "", s == trk::State::Linked ? trk::peerName().c_str() : "");
    lv_label_set_text(topLink, b);
    lv_obj_set_style_text_color(topLink, s == trk::State::Linked ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_ORANGE), 0);
    char setb[256];
    snprintf(setb, sizeof setb, "BLE link: %s\nTracker: %s  fw %s", trk::stateText(), M->info.ok ? M->info.board.c_str() : "-", M->info.ok ? M->info.fw.c_str() : "-");
    lv_label_set_text(lblSetInfo, setb);
    if (M->revision == drawnRev) return;
    drawnRev = M->revision;

    if (M->status.ok && M->status.batt > 0) snprintf(b, sizeof b, "Batt %.2f V", M->status.batt); else b[0] = 0;
    lv_label_set_text(topInfo, b);

    // Estado
    if (M->gnss.ok) {
        snprintf(b, sizeof b, "GNSS: %s   %.5f, %.5f   alt %.0f m   %.0f km/h   %d sats   hdop %.1f\nUTC %s",
                 M->gnss.fix ? "fix" : "no fix", M->gnss.lat, M->gnss.lon, M->gnss.alt, M->gnss.spd, M->gnss.sats, M->gnss.hdop, M->gnss.utc.c_str());
        lv_label_set_text(dashGnss, b);
    }
    if (M->status.ok) {
        snprintf(b, sizeof b, "Mode %d  profile %d  LoRa %d  %s%s%s   up %lu min", M->status.mode, M->status.profile, M->status.lora,
                 M->status.smart ? "SmartBeacon " : "", M->status.digi ? "digi " : "", M->status.sos ? "SOS" : "", (unsigned long)(M->status.up / 60));
        lv_label_set_text(dashStatus, b);
        if (M->status.digi) lv_obj_add_state(swDigi, LV_STATE_CHECKED); else lv_obj_clear_state(swDigi, LV_STATE_CHECKED);
    }
    lv_label_set_text(dashErr, M->lastError.c_str());
    if (M->info.ok) {
        std::string opts;
        int sel = 0;
        for (size_t i = 0; i < M->info.modeNames.size(); i++) {
            if (i) opts += '\n';
            opts += M->info.modeNames[i];
            if (M->info.modeIds[i] == M->info.mode) sel = (int)i;
        }
        if (!opts.empty()) { lv_dropdown_set_options(ddMode, opts.c_str()); lv_dropdown_set_selected(ddMode, sel); }
    }

    // Oidas
    lv_table_set_row_cnt(tblHeard, M->heard.size() + 1);
    for (size_t i = 0; i < M->heard.size(); i++) {
        const proto::Heard& h = M->heard[i];
        char d[16] = "-", r[16], sn[16], a[16];
        if (h.hasPos && M->gnss.fix) snprintf(d, sizeof d, "%.1f km", distKm(M->gnss.lat, M->gnss.lon, h.lat, h.lon));
        snprintf(r, sizeof r, "%d", h.rssi); snprintf(sn, sizeof sn, "%.1f", h.snr); fmtAge(a, sizeof a, h.age);
        lv_table_set_cell_value(tblHeard, i + 1, 0, h.call.c_str());
        lv_table_set_cell_value(tblHeard, i + 1, 1, h.sym);
        lv_table_set_cell_value(tblHeard, i + 1, 2, d);
        lv_table_set_cell_value(tblHeard, i + 1, 3, r);
        lv_table_set_cell_value(tblHeard, i + 1, 4, sn);
        lv_table_set_cell_value(tblHeard, i + 1, 5, a);
        lv_table_set_cell_value(tblHeard, i + 1, 6, h.cmt.c_str());
    }

    // Mensajes
    lv_table_set_row_cnt(tblMsgs, M->msgs.size() + 1);
    for (size_t i = 0; i < M->msgs.size(); i++) {
        const proto::Msg& g = M->msgs[i];
        lv_table_set_cell_value(tblMsgs, i + 1, 0, g.dir == 'T' ? ">" : "<");
        lv_table_set_cell_value(tblMsgs, i + 1, 1, g.call.c_str());
        lv_table_set_cell_value(tblMsgs, i + 1, 2, g.text.c_str());
        lv_table_set_cell_value(tblMsgs, i + 1, 3, g.dir == 'T' ? stName(g.st) : (g.st == 0 ? "new" : ""));
    }

    // Sensores
    {
        std::string t;
        auto add = [&](const char* name, float v, const char* unit, const char* fmt) {
            char x[64]; snprintf(x, sizeof x, fmt, v); t += std::string(name) + ": " + x + " " + unit + "\n";
        };
        const proto::Sensors& s = M->sensors;
        if (s.ok) {
            if (s.batt > 0) add("Battery", s.batt, "V", "%.2f");
            if (s.battPct >= 0) { char x[32]; snprintf(x, sizeof x, "%d %%", s.battPct); t += "Charge: " + std::string(x) + "\n"; }
            if (s.temp > -900) add("Temperature", s.temp, "C", "%.1f");
            if (s.hum >= 0) add("Humidity", s.hum, "%", "%.0f");
            if (s.press > 0) add("Pressure", s.press, "hPa", "%.1f");
            if (s.qnh > 0) add("QNH", s.qnh, "hPa", "%.1f");
            if (s.baroAlt > -900) add("Baro altitude", s.baroAlt, "m", "%.0f");
            if (s.heading >= 0) add("Heading", s.heading, "deg", "%.0f");
        }
        lv_label_set_text(lblSens, t.empty() ? "No sensor data yet" : t.c_str());
    }

    // SAT / rotor
    if (M->sat.ok) {
        snprintf(b, sizeof b, "%s\nAz %.1f  El %.1f  Rng %.0f km\nMax el %d deg", M->sat.name.c_str(), M->sat.az, M->sat.el, M->sat.rng, M->sat.maxEl);
        lv_label_set_text(lblSat, b);
        lv_arc_set_value(arcAz, (int)M->sat.az);
        lv_bar_set_value(barEl, M->sat.el < 0 ? 0 : (int)M->sat.el, LV_ANIM_OFF);
    }
    if (M->rot.ok) {
        static const char* srcs[] = {"satellite", "APRS compass", "fixed target", "external", "last APRS heard", "last CW beacon"};
        snprintf(b, sizeof b, "Rotor: %s %s\nAz %.1f  El %.1f%s", M->rot.src >= 0 && M->rot.src < 6 ? srcs[M->rot.src] : "?", M->rot.name.c_str(), M->rot.az, M->rot.el, M->rot.trk ? "  (tracking)" : "");
        lv_label_set_text(lblRot, b);
        if (!M->sat.ok) { lv_arc_set_value(arcAz, (int)M->rot.az); lv_bar_set_value(barEl, M->rot.el < 0 ? 0 : (int)M->rot.el, LV_ANIM_OFF); }
    }

    // Espejo
    if (M->screen.ok) {
        snprintf(b, sizeof b, "%s   %s", M->screen.title.c_str(), M->screen.right.c_str());
        lv_label_set_text(lblMirrorTitle, b);
        std::string t;
        for (const std::string& r : M->screen.rows) { t += r; t += '\n'; }
        lv_label_set_text(lblMirror, t.c_str());
    }

    // DAPNET
    lv_table_set_row_cnt(tblPages, M->pages.size() + 1);
    for (size_t i = 0; i < M->pages.size(); i++) {
        char r[16], q[16];
        snprintf(r, sizeof r, "%lu", (unsigned long)M->pages[i].ric); snprintf(q, sizeof q, "%d", M->pages[i].rssi);
        lv_table_set_cell_value(tblPages, i + 1, 0, r);
        lv_table_set_cell_value(tblPages, i + 1, 1, q);
        lv_table_set_cell_value(tblPages, i + 1, 2, M->pages[i].text.c_str());
    }
}

bool mirrorVisible() { return tabs && lv_tabview_get_tab_act(tabs) == 5; }
bool heardVisible() { return tabs && lv_tabview_get_tab_act(tabs) == 1; }
bool messagesVisible() { return tabs && lv_tabview_get_tab_act(tabs) == 2; }

} // namespace ui
