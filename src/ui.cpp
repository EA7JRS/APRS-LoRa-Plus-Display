/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz LVGL 9.6 tactil. Se adapta a la pantalla de la placa: grande (1024x600), media (480x320 en horizontal o
 * 480x640 en vertical) y pequena (240x280). Textos en ingles y sin acentos (las fuentes integradas de LVGL no los llevan).
 */
#include "ui.h"
#include "board.h"
#include "tracker_link.h"
#include "ui_assets.h"
#include <lvgl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace ui {

// ---------- disposicion segun el tamano de la pantalla ----------
enum class Size { S, M, L };
struct Layout {
    Size sz;
    bool portrait;
    const lv_font_t *fBase, *fBig, *fTable, *fMono, *fTab;
    int topH, tabBar;                     // alto de la barra superior; ancho (izquierda) o alto (abajo) de las pestanas
    lv_dir_t tabPos;
    bool tabIcons;                        // pestanas con iconos en vez de texto
    int btnW, btnH, ddW, kbH, iconScale, arc, pad, cellPad;
};
static Layout L;

static Layout makeLayout(int w, int h) {
    Layout l{};
    l.portrait = h > w;
    if (w >= 800) {
        l = {Size::L, false, &lv_font_montserrat_20, &lv_font_montserrat_24, &lv_font_montserrat_16, &lv_font_unscii_16, &lv_font_montserrat_20,
             48, 150, LV_DIR_LEFT, false, 220, 56, 320, 250, 256, 220, 16, 6};
    } else if (w >= 400) {
        if (l.portrait)          // 480 x 640: pestanas con iconos abajo
            l = {Size::M, true, &lv_font_montserrat_16, &lv_font_montserrat_20, &lv_font_montserrat_14, &lv_font_unscii_16, &lv_font_montserrat_20,
                 38, 52, LV_DIR_BOTTOM, true, 150, 46, 200, h * 2 / 5, 180, 170, 10, 4};
        else                     // 480 x 320: pestanas con texto a la izquierda
            l = {Size::M, false, &lv_font_montserrat_16, &lv_font_montserrat_20, &lv_font_montserrat_14, &lv_font_unscii_16, &lv_font_montserrat_14,
                 36, 96, LV_DIR_LEFT, false, 140, 42, 170, h / 2, 180, 130, 8, 3};
    } else {                     // 240 x 280
        l = {Size::S, l.portrait, &lv_font_montserrat_14, &lv_font_montserrat_16, &lv_font_montserrat_12, &lv_font_unscii_8, &lv_font_montserrat_16,
             28, 38, LV_DIR_BOTTOM, true, 96, 34, 120, h * 11 / 20, 128, 100, 4, 2};
    }
    return l;
}

static proto::Model* M = nullptr;
static uint32_t drawnRev = 0xFFFFFFFF;
static lv_obj_t *scr, *tabs, *tabDash, *tabHeard, *tabMsgs, *tabSens, *tabSat, *tabMirror, *tabPages, *tabSet;
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
static lv_obj_t *taPin, *taPeer, *lblSetInfo, *lblPinRes, *slBack;

// ---------- columnas de las tablas: cuales salen y su peso en el ancho, segun la pantalla ----------
struct Col { int id; float weight; const char* title; };
struct Table {
    std::vector<Col> cols;
    int idx[8];                           // idx[id] = columna en la tabla, o -1 si en esta pantalla no sale
};
enum { H_CALL, H_SYM, H_DIST, H_RSSI, H_SNR, H_AGE, H_CMT };
enum { G_DIR, G_CALL, G_TEXT, G_STATE };
enum { P_RIC, P_RSSI, P_TEXT };
static Table colsHeard, colsMsgs, colsPages;

static void pickColumns() {
    if (L.sz == Size::L) {
        colsHeard.cols = {{H_CALL, 160, "Station"}, {H_SYM, 80, "Sym"}, {H_DIST, 120, "Dist"}, {H_RSSI, 80, "RSSI"}, {H_SNR, 80, "SNR"}, {H_AGE, 80, "Age"}, {H_CMT, 220, "Comment"}};
        colsMsgs.cols = {{G_DIR, 60, ""}, {G_CALL, 170, "Station"}, {G_TEXT, 470, "Text"}, {G_STATE, 120, "State"}};
        colsPages.cols = {{P_RIC, 110, "RIC"}, {P_RSSI, 100, "RSSI"}, {P_TEXT, 610, "Text"}};
    } else if (L.sz == Size::M) {
        colsHeard.cols = {{H_CALL, 3, "Station"}, {H_DIST, 2.2f, "Dist"}, {H_RSSI, 1.5f, "RSSI"}, {H_AGE, 1.5f, "Age"}};
        if (L.portrait) colsHeard.cols.push_back({H_CMT, 3.5f, "Comment"});
        colsMsgs.cols = {{G_DIR, 0.5f, ""}, {G_CALL, 2.2f, "Station"}, {G_TEXT, 4.5f, "Text"}, {G_STATE, 1.6f, "State"}};
        colsPages.cols = {{P_RIC, 1.2f, "RIC"}, {P_TEXT, 4, "Text"}};
    } else {
        colsHeard.cols = {{H_CALL, 3, "Station"}, {H_DIST, 2, "Dist"}, {H_AGE, 1.4f, "Age"}};
        colsMsgs.cols = {{G_DIR, 0.5f, ""}, {G_CALL, 2.2f, "Call"}, {G_TEXT, 3.5f, "Text"}};
        colsPages.cols = {{P_RIC, 1.3f, "RIC"}, {P_TEXT, 3, "Text"}};
    }
}

static void setupTable(lv_obj_t* t, Table& tc) {
    for (int& i : tc.idx) i = -1;
    lv_table_set_column_count(t, tc.cols.size());
    float sum = 0;
    for (const Col& c : tc.cols) sum += c.weight;
    int avail = lv_obj_get_content_width(t) - 4;                   // margen para la barra de desplazamiento
    for (size_t i = 0; i < tc.cols.size(); i++) {
        tc.idx[tc.cols[i].id] = (int)i;
        lv_table_set_column_width(t, i, (int32_t)(avail * tc.cols[i].weight / sum));
        lv_table_set_cell_value(t, 0, i, tc.cols[i].title);
    }
}
static void cell(lv_obj_t* t, const Table& tc, uint32_t row, int id, const char* v) {
    if (tc.idx[id] >= 0) lv_table_set_cell_value(t, row, tc.idx[id], v);
}

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

// solo cambia el texto si es distinto (lv_label_set_text siempre manda repintar)
static void setText(lv_obj_t* l, const char* t) {
    if (strcmp(lv_label_get_text(l), t) != 0) lv_label_set_text(l, t);
}

static lv_obj_t* label(lv_obj_t* parent, const char* t, const lv_font_t* f = nullptr) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, t);
    if (f) lv_obj_set_style_text_font(l, f, 0);
    return l;
}
// etiqueta que ocupa todo el ancho y parte las lineas largas
static lv_obj_t* wrapLabel(lv_obj_t* parent, const char* t, const lv_font_t* f = nullptr) {
    lv_obj_t* l = label(parent, t, f);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    return l;
}

static lv_obj_t* button(lv_obj_t* parent, const char* t, lv_event_cb_t cb, void* ud = nullptr, int w = 0) {
    lv_obj_t* b = lv_button_create(parent);
    // en las pantallas media y pequena el boton se ajusta al texto (con un ancho minimo para el dedo)
    const bool fit = L.sz != Size::L && !w;
    lv_obj_set_size(b, w ? w : fit ? LV_SIZE_CONTENT : L.btnW, L.btnH);
    if (fit) { lv_obj_set_style_pad_hor(b, L.sz == Size::S ? 10 : 16, 0); lv_obj_set_style_min_width(b, L.btnW * 2 / 3, 0); }
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, t);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    return b;
}

// fila que reparte su contenido y pasa a la linea siguiente si no cabe
static lv_obj_t* row(lv_obj_t* parent) {
    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_set_size(r, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(r, L.pad, 0);
    lv_obj_set_style_pad_gap(r, L.pad, 0);
    return r;
}

// Teclado: al abrirlo las pestanas se encogen por encima de el y el campo se desplaza a la vista
static void showKeyboard(lv_obj_t* ta) {
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_hidden(kb, false);
    lv_obj_set_height(tabs, board::H - L.topH - L.kbH);
    lv_obj_update_layout(scr);
    lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
}
static void hideKeyboard() {
    lv_keyboard_set_textarea(kb, nullptr);
    lv_obj_set_hidden(kb, true);
    lv_obj_set_height(tabs, board::H - L.topH);
}
static void fieldFocus(lv_event_t* e) {
    lv_obj_t* ta = lv_event_get_target_obj(e);
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_FOCUSED) showKeyboard(ta);
    else if (c == LV_EVENT_DEFOCUSED) hideKeyboard();
    else if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL) { lv_obj_remove_state(ta, LV_STATE_FOCUSED); hideKeyboard(); }
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

static lv_obj_t* table(lv_obj_t* parent) {
    lv_obj_t* t = lv_table_create(parent);
    lv_obj_set_style_text_font(t, L.fTable, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(t, L.cellPad, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(t, L.cellPad, LV_PART_ITEMS);
    if (L.sz != Size::L) {
        lv_obj_set_style_pad_left(t, L.cellPad + 2, LV_PART_ITEMS);
        lv_obj_set_style_pad_right(t, L.cellPad + 2, LV_PART_ITEMS);
        lv_obj_set_style_pad_all(t, 0, 0);
    }
    return t;
}

// ---------- acciones ----------
static void onBeacon(lv_event_t*) { trk::send(proto::cmdSimple("beacon")); }
static void onDigi(lv_event_t* e) {
    bool on = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    trk::send("{\"c\":\"digi\",\"on\":" + std::string(on ? "true" : "false") + "}\n");
}
static lv_obj_t* mbMode = nullptr;
static void onModeSwitch(lv_event_t*) {
    if (M) {
        uint32_t sel = lv_dropdown_get_selected(ddMode);
        if (sel < M->info.modeIds.size()) trk::send(proto::cmdMode(M->info.modeIds[sel]));
    }
    lv_msgbox_close(mbMode);
    mbMode = nullptr;
}
static void onModeCancel(lv_event_t*) { lv_msgbox_close(mbMode); mbMode = nullptr; }
static void onModeSet(lv_event_t*) {
    if (mbMode) return;
    mbMode = lv_msgbox_create(nullptr);                       // modal, sobre la capa superior
    lv_msgbox_add_title(mbMode, "Change tracker mode");
    lv_msgbox_add_text(mbMode, "The tracker restarts in the selected mode.");
    lv_obj_set_width(mbMode, board::W - 20 < 520 ? board::W - 20 : 520);
    lv_obj_t* bOk = lv_msgbox_add_footer_button(mbMode, "Switch");
    lv_obj_t* bNo = lv_msgbox_add_footer_button(mbMode, "Cancel");
    int bw = L.sz == Size::S ? 90 : 160;
    lv_obj_set_size(bOk, bw, L.btnH);
    lv_obj_set_size(bNo, bw, L.btnH);
    lv_obj_add_event_cb(bOk, onModeSwitch, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(bNo, onModeCancel, LV_EVENT_CLICKED, nullptr);
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
    uint32_t r, c;
    lv_table_get_selected_cell(tblMsgs, &r, &c);
    if (r == LV_TABLE_CELL_NONE) return;
    if (r >= 1 && M && r - 1 < M->msgs.size() && !M->msgs[r - 1].call.empty()) lv_textarea_set_text(taTo, M->msgs[r - 1].call.c_str());
}
static void onSavePin(lv_event_t*) {
    const char* p = lv_textarea_get_text(taPin);
    if (strlen(p) != 6) { lv_label_set_text(lblPinRes, "The PIN has 6 digits"); return; }
    lv_label_set_text(lblPinRes, "");
    trk::setPin((uint32_t)atol(p));
    trk::setPeerAddress(lv_textarea_get_text(taPeer));
    trk::reconnect();
}
static void onForget(lv_event_t*) { trk::forgetPairing(); }
static void onReconnect(lv_event_t*) { trk::reconnect(); }
static void onBack(lv_event_t* e) { board::setBacklight(lv_slider_get_value(lv_event_get_target_obj(e))); }

static void onTab(lv_event_t*) {
    // al abrir cada pestaña se piden sus datos al momento
    uint32_t t = lv_tabview_get_tab_active(tabs);
    if (t == 1) trk::send(proto::cmdHeard(0, 12));
    if (t == 2) trk::send(proto::cmdMsgs(0, 10));
    if (t == 5) trk::send(proto::cmdScreen(2)); else trk::send(proto::cmdScreen(0));
    if (t == 6) trk::send(proto::cmdSimple("dapnet"));
}

// ---------- construccion ----------
static void column(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tab, L.pad, 0);
    lv_obj_set_style_pad_row(tab, L.pad, 0);
}

static void buildDash() {
    column(tabDash);
    const lv_font_t* f = L.sz == Size::L ? L.fBig : L.fBase;
    dashGnss = wrapLabel(tabDash, "GNSS: -", f);
    dashStatus = wrapLabel(tabDash, "Tracker: -", f);
    dashErr = wrapLabel(tabDash, "");
    lv_obj_set_style_text_color(dashErr, lv_palette_main(LV_PALETTE_RED), 0);

    lv_obj_t* r = row(tabDash);
    button(r, "Beacon now", onBeacon);
    label(r, "Digipeater");
    swDigi = lv_switch_create(r);
    lv_obj_add_event_cb(swDigi, onDigi, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* r2 = row(tabDash);
    label(r2, "Mode");
    ddMode = lv_dropdown_create(r2);
    lv_obj_set_width(ddMode, L.ddW);
    lv_dropdown_set_options(ddMode, "-");
    button(r2, "Switch mode", onModeSet);
}

static void buildHeard() {
    lv_obj_set_style_pad_all(tabHeard, L.pad, 0);
    tblHeard = table(tabHeard);
    lv_obj_set_size(tblHeard, LV_PCT(100), LV_PCT(100));
}

static void buildMsgs() {
    column(tabMsgs);
    tblMsgs = table(tabMsgs);
    lv_obj_set_size(tblMsgs, LV_PCT(100), L.sz == Size::L ? 280 : LV_PCT(48));
    lv_obj_add_event_cb(tblMsgs, onTblMsgs, LV_EVENT_VALUE_CHANGED, nullptr);

    if (L.sz == Size::L) {
        lv_obj_t* r = row(tabMsgs);
        taTo = field(r, "To (EA7XXX-9)", 200, 9);
        taText = field(r, "Message (max 67)", 420, 67);
        button(r, "Send", onSend, nullptr, 120);
    } else {                                                      // texto a todo el ancho; debajo destino y boton
        taText = field(tabMsgs, "Message (max 67)", 0, 67);
        lv_obj_set_width(taText, LV_PCT(100));
        lv_obj_t* r = row(tabMsgs);
        taTo = field(r, L.sz == Size::S ? "To" : "To (EA7XXX-9)", L.sz == Size::M ? 160 : 110, 9);
        button(r, "Send", onSend, nullptr, L.sz == Size::M ? 100 : 70);
    }
    lblSendRes = wrapLabel(tabMsgs, "");
}

static void buildSens() {
    lv_obj_set_style_pad_all(tabSens, L.pad, 0);
    lblSens = wrapLabel(tabSens, "-", L.sz == Size::L ? L.fBig : L.fBase);
}

static void buildSat() {
    lv_obj_set_flex_flow(tabSat, L.sz == Size::L ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tabSat, L.pad, 0);
    lv_obj_set_style_pad_gap(tabSat, L.sz == Size::L ? 24 : L.pad, 0);
    const lv_font_t* f = L.sz == Size::L ? L.fBig : L.fBase;
    lv_obj_t* left = lv_obj_create(tabSat);
    lv_obj_set_size(left, L.sz == Size::L ? 460 : LV_PCT(100), L.sz == Size::L ? LV_PCT(100) : LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(left, L.pad, 0);
    lblSat = wrapLabel(left, "SAT mode inactive", f);
    lblRot = wrapLabel(left, "", f);
    lv_obj_t* right = lv_obj_create(tabSat);
    lv_obj_set_size(right, L.sz == Size::L ? 340 : LV_PCT(100), L.sz == Size::L ? LV_PCT(100) : LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(right, L.pad, 0);
    label(right, "Azimuth");
    arcAz = lv_arc_create(right);
    lv_obj_set_size(arcAz, L.arc, L.arc);
    lv_arc_set_range(arcAz, 0, 360);
    lv_arc_set_bg_angles(arcAz, 0, 360);
    lv_arc_set_rotation(arcAz, 270);
    lv_obj_remove_style(arcAz, nullptr, LV_PART_KNOB);
    lv_obj_set_clickable(arcAz, false);
    label(right, "Elevation");
    barEl = lv_bar_create(right);
    lv_obj_set_size(barEl, L.arc + 40 > 260 ? 260 : L.arc + 40, L.sz == Size::S ? 14 : 24);
    lv_bar_set_range(barEl, 0, 90);
}

static void buildMirror() {
    column(tabMirror);
    lblMirrorTitle = wrapLabel(tabMirror, "Tracker screen (mirror)", L.sz == Size::L ? L.fBig : L.fBase);
    lblMirror = label(tabMirror, "Open this tab with the tracker linked", L.fMono);
}

static void buildPages() {
    lv_obj_set_style_pad_all(tabPages, L.pad, 0);
    tblPages = table(tabPages);
    lv_obj_set_size(tblPages, LV_PCT(100), LV_PCT(100));
}

static void buildSet() {
    column(tabSet);
    lblSetInfo = wrapLabel(tabSet, "", L.sz == Size::L ? L.fBig : L.fBase);
    wrapLabel(tabSet, "Tracker control PIN (WebUI > Bluetooth > Control PIN, default 123456)");
    taPin = field(tabSet, "PIN", L.sz == Size::L ? 220 : 130, 6, "0123456789");
    char b[12]; snprintf(b, sizeof b, "%06lu", (unsigned long)trk::pin()); lv_textarea_set_text(taPin, b);
    wrapLabel(tabSet, "Tracker address (empty = first tracker heard)");
    taPeer = field(tabSet, "AA:BB:CC:DD:EE:FF", L.sz == Size::L ? 320 : L.sz == Size::M ? 240 : 200, 17);
    lv_textarea_set_text(taPeer, trk::peerAddress().c_str());
    lv_obj_t* r = row(tabSet);
    button(r, L.sz == Size::S ? "Save" : "Save and reconnect", onSavePin, nullptr, L.sz == Size::L ? 260 : 0);
    button(r, L.sz == Size::S ? "Forget" : "Forget pairing", onForget);
    button(r, "Reconnect", onReconnect);
    lblPinRes = wrapLabel(tabSet, "");
    lv_obj_set_style_text_color(lblPinRes, lv_palette_main(LV_PALETTE_RED), 0);
    label(tabSet, "Backlight");
    slBack = lv_slider_create(tabSet);
    lv_obj_set_width(slBack, L.sz == Size::L ? 400 : LV_PCT(85));
    lv_slider_set_range(slBack, 5, 100);
    lv_slider_set_value(slBack, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(slBack, onBack, LV_EVENT_VALUE_CHANGED, nullptr);
}

// Pantalla de arranque: la de bienvenida de la app (logo sobre #131A22, entra con fundido) y debajo "Display <version>";
// a los 2,2 s pasa con fundido a la pantalla principal (que ya esta montada) y se borra.
static void showSplash(lv_obj_t* mainScr) {
    lv_obj_t* sp = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(sp, lv_color_hex(SPLASH_BG_RGB), 0);
    lv_obj_set_style_bg_opa(sp, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(sp, false);
    // LVGL 9 no escala imagenes indexadas: hay una pequena hecha de antemano (tools/make_ui_assets.py)
    const lv_image_dsc_t* sim = board::H >= 600 ? &img_splash : &img_splash_small;
    lv_obj_t* img = lv_image_create(sp);
    lv_image_set_src(img, sim);
    const int hShown = sim->header.h;
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -L.topH / 2);
    lv_obj_t* ver = lv_label_create(sp);
    lv_label_set_text(ver, "Display " DISPLAY_VERSION);
    lv_obj_set_style_text_font(ver, L.fBase, 0);
    lv_obj_set_style_text_color(ver, lv_color_hex(0x28B5E1), 0);
    lv_obj_align(ver, LV_ALIGN_CENTER, 0, -L.topH / 2 + hShown / 2 + 4 + lv_font_get_line_height(L.fBase) / 2);
    lv_screen_load(sp);
    lv_obj_fade_in(img, 700, 0);
    lv_obj_fade_in(ver, 700, 300);
    lv_screen_load_anim(mainScr, LV_SCREEN_LOAD_ANIM_FADE_IN, 500, 2200, true);
}

void begin(proto::Model* model) {
    M = model;
    L = makeLayout(board::W, board::H);
    pickColumns();
    // tema con la fuente de esta pantalla (botones, listas, campos y teclado la heredan)
    lv_display_t* d = lv_display_get_default();
    lv_display_set_theme(d, lv_theme_default_init(d, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, L.fBase));
    lv_obj_set_style_text_font(lv_layer_top(), L.fBase, 0);     // la ventana de modo y el teclado viven en la capa superior
    scr = lv_obj_create(nullptr);
    lv_obj_set_style_text_font(scr, L.fBase, 0);

    lv_obj_t* top = lv_obj_create(scr);
    lv_obj_set_size(top, board::W, L.topH);
    lv_obj_set_pos(top, 0, 0);
    lv_obj_set_style_radius(top, 0, 0);
    lv_obj_set_style_pad_ver(top, 0, 0);
    lv_obj_set_style_pad_hor(top, L.pad, 0);
    lv_obj_set_scrollable(top, false);
    lv_obj_t* icon = lv_image_create(top);                    // icono de la app, como en las WebUI
    lv_image_set_src(icon, &img_app_icon);
    if (L.iconScale != 256) {
        lv_image_set_scale(icon, L.iconScale);
        lv_obj_set_size(icon, 40 * L.iconScale / 256, 40 * L.iconScale / 256);
        lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CENTER);
    }
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, L.sz == Size::L ? -6 : 0, 0);
    const int iconW = L.sz == Size::L ? 34 : 40 * L.iconScale / 256;
    lv_obj_t* title = label(top, L.sz == Size::L ? "APRS LoRa+ Display " DISPLAY_VERSION : "APRS LoRa+",
                            L.sz == Size::L ? L.fBig : L.fBase);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, iconW + (L.sz == Size::L ? 10 : 6), 0);
    topLink = label(top, "");
    lv_obj_align(topLink, LV_ALIGN_RIGHT_MID, 0, 0);
    topInfo = label(top, "");
    if (L.sz == Size::L) lv_obj_align(topInfo, LV_ALIGN_CENTER, 90, 0);
    else if (L.sz == Size::M) lv_obj_align(topInfo, LV_ALIGN_CENTER, -10, 0);
    else lv_obj_set_hidden(topInfo, true);

    tabs = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(tabs, L.tabPos);
    lv_tabview_set_tab_bar_size(tabs, L.tabBar);
    lv_obj_set_size(tabs, board::W, board::H - L.topH);
    lv_obj_set_pos(tabs, 0, L.topH);
    lv_obj_set_style_text_font(lv_tabview_get_tab_bar(tabs), L.fTab, 0);
    struct { const char* text; const char* icon; lv_obj_t** tab; } T[] = {
        {"Status", LV_SYMBOL_HOME, &tabDash}, {"Heard", LV_SYMBOL_LIST, &tabHeard}, {"Messages", LV_SYMBOL_ENVELOPE, &tabMsgs},
        {"Sensors", LV_SYMBOL_BATTERY_FULL, &tabSens}, {"SAT / Rotor", LV_SYMBOL_GPS, &tabSat}, {"Mirror", LV_SYMBOL_EYE_OPEN, &tabMirror},
        {"DAPNET", LV_SYMBOL_BELL, &tabPages}, {"Settings", LV_SYMBOL_SETTINGS, &tabSet}};
    for (auto& t : T) *t.tab = lv_tabview_add_tab(tabs, L.tabIcons ? t.icon : t.text);
    lv_obj_add_event_cb(tabs, onTab, LV_EVENT_VALUE_CHANGED, nullptr);

    kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(kb, board::W, L.kbH);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_hidden(kb, true);

    buildDash(); buildHeard(); buildMsgs(); buildSens(); buildSat(); buildMirror(); buildPages(); buildSet();

    // las columnas se reparten con el ancho real de cada tabla, que se conoce despues de colocar todo
    lv_obj_update_layout(scr);
    setupTable(tblHeard, colsHeard);
    setupTable(tblMsgs, colsMsgs);
    setupTable(tblPages, colsPages);

    showSplash(scr);
}

// ---------- actualizacion ----------
static const char* stName(int st) {
    switch (st) { case 0: return "new"; case 1: return "read"; case 2: return "sent"; case 3: return "waiting ack"; case 4: return "ack"; case 5: return "failed"; default: return "?"; }
}

void update() {
    if (!M) return;
    char b[512];
    trk::State s = trk::state();
    const bool name = s == trk::State::Linked && L.sz != Size::S;   // en la pequena no cabe el nombre (esta en Settings)
    snprintf(b, sizeof b, "%s%s%s", trk::stateText(), name ? ": " : "", name ? trk::peerName().c_str() : "");
    setText(topLink, b);
    lv_obj_set_style_text_color(topLink, s == trk::State::Linked ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_ORANGE), 0);
    char setb[256];
    snprintf(setb, sizeof setb, "BLE link: %s %s\nTracker: %s  fw %s", trk::stateText(), trk::state() == trk::State::Linked ? trk::peerName().c_str() : "", M->info.ok ? M->info.board.c_str() : "-", M->info.ok ? M->info.fw.c_str() : "-");
    setText(lblSetInfo, setb);
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
        if (M->status.digi) lv_obj_add_state(swDigi, LV_STATE_CHECKED); else lv_obj_remove_state(swDigi, LV_STATE_CHECKED);
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
        // solo al cambiar la lista o el modo del tracker: si no, cada evento borraria lo que el usuario acaba de elegir
        static std::string shownOpts;
        static int shownMode = -1;
        if (!opts.empty() && (opts != shownOpts || M->info.mode != shownMode)) {
            lv_dropdown_set_options(ddMode, opts.c_str());
            lv_dropdown_set_selected(ddMode, sel);
            shownOpts = opts;
            shownMode = M->info.mode;
        }
    }

    // Oidas
    lv_table_set_row_count(tblHeard, M->heard.size() + 1);
    for (size_t i = 0; i < M->heard.size(); i++) {
        const proto::Heard& h = M->heard[i];
        char d[16] = "-", r[16], sn[16], a[16];
        if (h.hasPos && M->gnss.fix) snprintf(d, sizeof d, "%.1f km", distKm(M->gnss.lat, M->gnss.lon, h.lat, h.lon));
        snprintf(r, sizeof r, "%d", h.rssi); snprintf(sn, sizeof sn, "%.1f", h.snr); fmtAge(a, sizeof a, h.age);
        cell(tblHeard, colsHeard, i + 1, H_CALL, h.call.c_str());
        cell(tblHeard, colsHeard, i + 1, H_SYM, h.sym);
        cell(tblHeard, colsHeard, i + 1, H_DIST, d);
        cell(tblHeard, colsHeard, i + 1, H_RSSI, r);
        cell(tblHeard, colsHeard, i + 1, H_SNR, sn);
        cell(tblHeard, colsHeard, i + 1, H_AGE, a);
        cell(tblHeard, colsHeard, i + 1, H_CMT, h.cmt.c_str());
    }

    // Mensajes
    lv_table_set_row_count(tblMsgs, M->msgs.size() + 1);
    for (size_t i = 0; i < M->msgs.size(); i++) {
        const proto::Msg& g = M->msgs[i];
        cell(tblMsgs, colsMsgs, i + 1, G_DIR, g.dir == 'T' ? ">" : "<");
        cell(tblMsgs, colsMsgs, i + 1, G_CALL, g.call.c_str());
        cell(tblMsgs, colsMsgs, i + 1, G_TEXT, g.text.c_str());
        cell(tblMsgs, colsMsgs, i + 1, G_STATE, g.dir == 'T' ? stName(g.st) : (g.st == 0 ? "new" : ""));
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
            if (s.qnh > 0) add(s.qnhSrc == "net" ? "QNH (internet)" : s.qnhSrc == "man" ? "QNH (manual)" : "QNH", s.qnh, "hPa", "%.1f");
            if (s.baroAlt > -900) add("Baro altitude", s.baroAlt, "m", "%.0f");
            if (s.heading >= 0) add("Heading", s.heading, "deg", "%.0f");
            // Tiempo de internet (la app lo baja de OpenWeatherMap / IQAir y lo manda al tracker): nunca es un sensor del tracker
            const proto::NetWx& n = s.net;
            if (n.ok) {
                char h[96];
                snprintf(h, sizeof h, "\nInternet weather (%s%s%s, %u min ago)\n", n.src.c_str(), n.place.empty() ? "" : " - ",
                         n.place.c_str(), (unsigned)(n.age / 60));
                t += h;
                if (!n.desc.empty()) t += n.desc + "\n";
                if (n.t > -900) add("Temperature", n.t, "C", "%.1f");
                if (n.h >= 0) add("Humidity", n.h, "%", "%.0f");
                if (n.p > 0) add("Pressure (sea level)", n.p, "hPa", "%.1f");
                if (n.ws >= 0) {
                    char w[64];
                    snprintf(w, sizeof w, "Wind: %.1f m/s", n.ws);
                    t += w;
                    if (n.wg >= 0) { snprintf(w, sizeof w, ", gusts %.1f", n.wg); t += w; }
                    if (n.wd >= 0) { snprintf(w, sizeof w, " from %d deg", n.wd); t += w; }
                    t += "\n";
                }
                if (n.rain >= 0) add("Rain", n.rain, "mm/h", "%.1f");
                if (n.clouds >= 0) { char c[32]; snprintf(c, sizeof c, "Clouds: %d %%\n", n.clouds); t += c; }
                if (n.aqiUs >= 0) {
                    char a[64]; snprintf(a, sizeof a, "Air quality: US AQI %d%s%s\n", n.aqiUs, n.pollutant.empty() ? "" : " ", n.pollutant.c_str()); t += a;
                } else if (n.aqiOwm > 0) {
                    static const char* names[] = {"", "good", "fair", "moderate", "poor", "very poor"};
                    char a[48]; snprintf(a, sizeof a, "Air quality: %d/5 %s\n", n.aqiOwm, names[n.aqiOwm > 5 ? 5 : n.aqiOwm]); t += a;
                }
                if (n.pm25 >= 0) add("PM2.5", n.pm25, "ug/m3", "%.1f");
                if (n.pm10 >= 0) add("PM10", n.pm10, "ug/m3", "%.1f");
            }
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
    lv_table_set_row_count(tblPages, M->pages.size() + 1);
    for (size_t i = 0; i < M->pages.size(); i++) {
        char r[16], q[16];
        snprintf(r, sizeof r, "%lu", (unsigned long)M->pages[i].ric); snprintf(q, sizeof q, "%d", M->pages[i].rssi);
        cell(tblPages, colsPages, i + 1, P_RIC, r);
        cell(tblPages, colsPages, i + 1, P_RSSI, q);
        cell(tblPages, colsPages, i + 1, P_TEXT, M->pages[i].text.c_str());
    }
}

bool mirrorVisible() { return tabs && lv_tabview_get_tab_active(tabs) == 5; }
bool heardVisible() { return tabs && lv_tabview_get_tab_active(tabs) == 1; }
bool messagesVisible() { return tabs && lv_tabview_get_tab_active(tabs) == 2; }

} // namespace ui
