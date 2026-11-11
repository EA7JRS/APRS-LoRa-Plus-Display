/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz LVGL 9.6 tactil: tarjetas, listas clave / valor, brujula con AZ y EL grandes y letra DejaVu Sans, como la pantalla
 * del rotor (APRS-LoRa-Plus-Rotor, lcd_ui.h). Dos paletas a elegir en Settings: la de la app Android (Theme.kt, por defecto)
 * y la del rotor (cabecera roja).
 * Se adapta a la pantalla de la placa: grande (1024x600), media (480x320 en horizontal o 480x640 en vertical) y
 * pequena (240x280). Textos en ingles; las fuentes llevan Latin-1 (src/fonts, tools/make_fonts.sh).
 */
#ifndef UI_RADIO   // la 7B v2 (waveshare_lcd7b_v2) usa la interfaz tipo emisora de src/ui7b/
#include "ui.h"
#include "board.h"
#include "tracker_link.h"
#include "ui_assets.h"
#include <lvgl.h>
#include <cctype>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <vector>

LV_FONT_DECLARE(dejavu_12)
LV_FONT_DECLARE(dejavu_14)
LV_FONT_DECLARE(dejavu_16)
LV_FONT_DECLARE(dejavu_20)
LV_FONT_DECLARE(dejavu_24)
LV_FONT_DECLARE(dejavu_bold_16)
LV_FONT_DECLARE(dejavu_bold_20)
LV_FONT_DECLARE(dejavu_bold_24)
LV_FONT_DECLARE(dejavu_bold_28)
LV_FONT_DECLARE(dejavu_num_32)
LV_FONT_DECLARE(dejavu_num_44)
LV_FONT_DECLARE(dejavu_num_60)

namespace ui {

// ---------- paletas: la de la app Android (Theme.kt, colores del logo) y la de la pantalla del rotor (lcd_ui.h) ----------
struct Pal {
    uint32_t bg, card, cardBorder, fg, mut, acc, red, green, yel, line, field, logo, needle;
    uint32_t hdr, hdrLine, hdrTitle, sub, on, off;                 // cabecera: fondo, raya, titulo, version, enlazado / no
    uint32_t btn, btnBorder, btnText, btnOn, btnAcc, btnAccText;    // botones (pulsado, principal)
    uint32_t sel, selText;                                          // pestana elegida
    bool capsTitles;                                                // titulos de tarjeta en mayusculas espaciadas (app)
};
static const Pal PAL_APP = {   // fondo gris azulado, verde neon y cian del logo; ambar para lo elegido
    0x131A22, 0x18222D, 0x263443, 0xE8EEF3, 0x9FB0BF, 0x28B5E1, 0xFF6B6B, 0x3CE68A, 0xFFB347, 0x263443, 0x0B1016, 0x28B5E1, 0xFF6B6B,
    0x131A22, 0x263443, 0xE8EEF3, 0x28B5E1, 0x3CE68A, 0xFFB347,
    0x1E2A36, 0x2E3E4F, 0xE8EEF3, 0x2E3E4F, 0x28B5E1, 0x131A22,
    0x1E2A36, 0xFFB347,
    true};
static const Pal PAL_ROTOR = { // fondo oscuro, tarjetas, azul; cabecera roja
    0x10151c, 0x19212b, 0x19212b, 0xe6edf3, 0x8b98a5, 0x4aa3ff, 0xff5d5d, 0x3fb950, 0xffd23f, 0x33475c, 0x0d1218, 0x28b5e1, 0xff5d5d,
    0xb21d1d, 0x7a1414, 0xffffff, 0xf0d0d0, 0xffffff, 0xe6a0a0,
    0x263241, 0x263241, 0xffffff, 0x4aa3ff, 0x2f6fb0, 0xffffff,
    0x14324a, 0x4aa3ff,
    false};
static const Pal* P = &PAL_APP;
static int themeIdx = 0;
static void (*themeCb)(int) = nullptr;

static lv_color_t C(uint32_t v) { return lv_color_hex(v); }

// ---------- disposicion segun el tamano de la pantalla ----------
enum class Size { S, M, L };
struct Layout {
    Size sz;
    bool portrait;
    const lv_font_t *fBase, *fTable, *fTitle, *fSub, *fBold, *fNum, *fMono, *fTab, *fBtn;
    int topH, tabBar;                     // alto de la cabecera; ancho (izquierda) o alto (abajo) de las pestanas
    lv_dir_t tabPos;
    bool tabIcons;                        // pestanas con iconos en vez de texto
    int btnW, btnH, ddW, kbH, iconPx, compass, pad, cellPad, radius, keyW;
};
static Layout L;

static Layout makeLayout(int w, int h) {
    Layout l{};
    const bool portrait = h > w;
    if (w >= 800)
        l = {Size::L, false, &dejavu_20, &dejavu_16, &dejavu_bold_28, &dejavu_20, &dejavu_bold_24, &dejavu_num_60, &lv_font_unscii_16, &dejavu_20, &dejavu_bold_20,
             64, 150, LV_DIR_LEFT, false, 220, 56, 250, 250, 40, 380, 14, 6, 10, 190};
    else if (w >= 400 && portrait)        // 480 x 640: pestanas con iconos abajo
        l = {Size::M, true, &dejavu_16, &dejavu_14, &dejavu_bold_20, &dejavu_14, &dejavu_bold_20, &dejavu_num_44, &lv_font_unscii_16, &dejavu_20, &dejavu_bold_16,
             46, 52, LV_DIR_BOTTOM, true, 150, 46, 200, h * 2 / 5, 30, 300, 10, 4, 8, 130};
    else if (w >= 400)                    // 480 x 320: pestanas con texto a la izquierda
        l = {Size::M, false, &dejavu_16, &dejavu_14, &dejavu_bold_20, &dejavu_14, &dejavu_bold_20, &dejavu_num_32, &lv_font_unscii_16, &dejavu_14, &dejavu_bold_16,
             44, 96, LV_DIR_LEFT, false, 140, 42, 170, h / 2, 30, 200, 8, 3, 8, 120};
    else                                  // 240 x 280
        l = {Size::S, portrait, &dejavu_14, &dejavu_12, &dejavu_bold_16, &dejavu_12, &dejavu_bold_16, &dejavu_num_32, &lv_font_unscii_8, &dejavu_16, &dejavu_14,
             36, 38, LV_DIR_BOTTOM, true, 96, 34, 120, h * 11 / 20, 24, 150, 4, 2, 6, 98};
    if (w >= 800 && h < 600) {           // 800 x 480 (Touch-LCD-7): la disposicion grande, mas ajustada
        l.topH = 56; l.tabBar = 132; l.compass = 290; l.kbH = 220; l.btnW = 190; l.btnH = 50; l.pad = 10; l.keyW = 160;
        l.fNum = &dejavu_num_44; l.fTitle = &dejavu_bold_24;
    }
    return l;
}

static proto::Model* M = nullptr;
static uint32_t drawnRev = 0xFFFFFFFF;
static lv_obj_t *scr, *tabs, *tabDash, *tabHeard, *tabMsgs, *tabSens, *tabSat, *tabMirror, *tabPages, *tabSet;
static lv_obj_t* kb;

// cabecera
static lv_obj_t *topLink1, *topLink2, *topInfo;
// estado
static lv_obj_t *ddMode, *swDigi;
// heard
static lv_obj_t* tblHeard;
// mensajes
static lv_obj_t *tblMsgs, *taTo, *taText, *lblSendRes;
// sat / rotor
static lv_obj_t *compass, *needle, *lblAz, *lblEl, *lblSatInfo;
// espejo
static lv_obj_t *lblMirrorTitle, *lblMirror;
// dapnet
static lv_obj_t* tblPages;
// ajustes
static lv_obj_t *taPin, *taPeer, *lblPinRes, *lblBack;
static int backlight = 100;

// ---------- estilos ----------
static lv_style_t stCard, stBtn, stBtnPr, stBtnAcc, stField, stTable, stTableItems, stKey, stVal, stTabBtn, stTabBtnOn, stTitle;

static void initStyles() {
    lv_style_init(&stCard);
    lv_style_set_bg_color(&stCard, C(P->card));
    lv_style_set_bg_opa(&stCard, LV_OPA_COVER);
    lv_style_set_border_color(&stCard, C(P->cardBorder));
    lv_style_set_border_width(&stCard, 1);
    lv_style_set_radius(&stCard, L.radius);
    lv_style_set_pad_all(&stCard, L.pad);
    lv_style_set_pad_gap(&stCard, L.pad);
    lv_style_set_text_color(&stCard, C(P->fg));

    lv_style_init(&stBtn);
    lv_style_set_bg_color(&stBtn, C(P->btn));
    lv_style_set_bg_opa(&stBtn, LV_OPA_COVER);
    lv_style_set_radius(&stBtn, L.radius);
    lv_style_set_shadow_width(&stBtn, 0);
    lv_style_set_border_width(&stBtn, 1);
    lv_style_set_border_color(&stBtn, C(P->btnBorder));
    lv_style_set_text_color(&stBtn, C(P->btnText));
    lv_style_set_text_font(&stBtn, L.fBtn);
    lv_style_init(&stBtnPr);
    lv_style_set_bg_color(&stBtnPr, C(P->btnOn));
    lv_style_init(&stBtnAcc);                                   // boton principal (enviar, cambiar modo)
    lv_style_set_bg_color(&stBtnAcc, C(P->btnAcc));
    lv_style_set_border_color(&stBtnAcc, C(P->btnAcc));
    lv_style_set_text_color(&stBtnAcc, C(P->btnAccText));

    lv_style_init(&stField);
    lv_style_set_bg_color(&stField, C(P->field));
    lv_style_set_bg_opa(&stField, LV_OPA_COVER);
    lv_style_set_border_color(&stField, C(P->line));
    lv_style_set_border_width(&stField, 1);
    lv_style_set_radius(&stField, L.radius);
    lv_style_set_text_color(&stField, C(P->fg));

    lv_style_init(&stTable);
    lv_style_set_bg_color(&stTable, C(P->card));
    lv_style_set_bg_opa(&stTable, LV_OPA_COVER);
    lv_style_set_border_width(&stTable, 0);
    lv_style_set_radius(&stTable, L.radius);
    lv_style_set_pad_all(&stTable, L.sz == Size::L ? 6 : 2);
    lv_style_init(&stTableItems);
    lv_style_set_bg_opa(&stTableItems, LV_OPA_TRANSP);
    lv_style_set_text_color(&stTableItems, C(P->fg));
    lv_style_set_text_font(&stTableItems, L.fTable);
    lv_style_set_border_color(&stTableItems, C(P->line));
    lv_style_set_border_width(&stTableItems, 1);
    lv_style_set_border_side(&stTableItems, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_pad_top(&stTableItems, L.cellPad);
    lv_style_set_pad_bottom(&stTableItems, L.cellPad);
    lv_style_set_pad_left(&stTableItems, L.cellPad + 4);
    lv_style_set_pad_right(&stTableItems, L.cellPad + 2);

    lv_style_init(&stKey);
    lv_style_set_text_color(&stKey, C(P->mut));
    lv_style_init(&stVal);
    lv_style_set_text_color(&stVal, C(P->fg));
    lv_style_init(&stTitle);                                    // titulo de tarjeta ("Acerca de" en el rotor)
    lv_style_set_text_color(&stTitle, C(P->mut));
    lv_style_set_text_font(&stTitle, L.fBold);

    lv_style_init(&stTabBtn);
    lv_style_set_text_color(&stTabBtn, C(P->mut));
    lv_style_set_bg_opa(&stTabBtn, LV_OPA_TRANSP);
    lv_style_init(&stTabBtnOn);
    lv_style_set_text_color(&stTabBtnOn, C(P->selText));
    lv_style_set_border_color(&stTabBtnOn, C(P->selText));
    lv_style_set_bg_color(&stTabBtnOn, C(P->sel));
    lv_style_set_bg_opa(&stTabBtnOn, LV_OPA_COVER);
}

// ---------- piezas ----------
static void setText(lv_obj_t* l, const char* t) {                  // solo repinta si cambia
    if (strcmp(lv_label_get_text(l), t) != 0) lv_label_set_text(l, t);
}
static void setColor(lv_obj_t* l, uint32_t c) { lv_obj_set_style_text_color(l, C(c), 0); }

static lv_obj_t* label(lv_obj_t* parent, const char* t, const lv_font_t* f = nullptr) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, t);
    if (f) lv_obj_set_style_text_font(l, f, 0);
    return l;
}
static lv_obj_t* wrapLabel(lv_obj_t* parent, const char* t, const lv_font_t* f = nullptr) {
    lv_obj_t* l = label(parent, t, f);
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    return l;
}
static lv_obj_t* plain(lv_obj_t* parent) {                       // contenedor sin fondo ni margenes
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, LV_PCT(100), LV_SIZE_CONTENT);
    return o;
}
static lv_obj_t* card(lv_obj_t* parent, const char* title = nullptr) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_add_style(c, &stCard, 0);
    lv_obj_set_size(c, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(c, false);
    if (title) {
        if (P->capsTitles) {                                        // como SectionLabel de la app: MAYUSCULAS espaciadas
            std::string t(title);
            for (char& ch : t) ch = (char)toupper((unsigned char)ch);
            lv_obj_t* l = label(c, t.c_str(), L.fSub);
            lv_obj_set_style_text_color(l, C(P->mut), 0);
            lv_obj_set_style_text_letter_space(l, 2, 0);
        } else {
            lv_obj_add_style(label(c, title), &stTitle, 0);
        }
    }
    return c;
}
// fila que reparte su contenido y pasa a la linea siguiente si no cabe
static lv_obj_t* row(lv_obj_t* parent) {
    lv_obj_t* r = plain(parent);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(r, L.pad, 0);
    return r;
}

static lv_obj_t* button(lv_obj_t* parent, const char* t, lv_event_cb_t cb, void* ud = nullptr, int w = 0, bool accent = false) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_add_style(b, &stBtn, 0);
    lv_obj_add_style(b, &stBtnPr, LV_STATE_PRESSED);
    if (accent) lv_obj_add_style(b, &stBtnAcc, 0);
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

// Lista clave / valor (como la pantalla de informacion del rotor). Las filas se crean al vuelo y las que sobran se esconden.
struct Kv {
    lv_obj_t* box = nullptr;
    std::vector<lv_obj_t*> rows;
    size_t n = 0;
    void create(lv_obj_t* parent, const char* title = nullptr) { box = card(parent, title); lv_obj_set_style_pad_row(box, L.sz == Size::S ? 2 : L.pad / 2, 0); }
    void begin() { n = 0; }
    void add(const char* k, const char* v, uint32_t color = P->fg) {
        if (n == rows.size()) {
            lv_obj_t* r = plain(box);
            lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
            lv_obj_set_style_pad_column(r, L.pad, 0);
            lv_obj_t* kl = label(r, "");
            lv_obj_add_style(kl, &stKey, 0);
            lv_obj_set_width(kl, L.keyW);
            lv_label_set_long_mode(kl, LV_LABEL_LONG_MODE_CLIP);
            lv_obj_set_height(kl, lv_font_get_line_height(L.fBase));
            lv_obj_t* vl = label(r, "");
            lv_obj_add_style(vl, &stVal, 0);
            lv_obj_set_flex_grow(vl, 1);
            lv_label_set_long_mode(vl, LV_LABEL_LONG_MODE_WRAP);
            rows.push_back(r);
        }
        lv_obj_t* r = rows[n++];
        lv_obj_set_hidden(r, false);
        setText(lv_obj_get_child(r, 0), k);
        lv_obj_t* vl = lv_obj_get_child(r, 1);
        setText(vl, v);
        setColor(vl, color);
    }
    void addf(const char* k, uint32_t color, const char* fmt, ...) LV_FORMAT_ATTRIBUTE(4, 5) {
        char b[160];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(b, sizeof b, fmt, ap);
        va_end(ap);
        add(k, b, color);
    }
    void end() { for (size_t i = n; i < rows.size(); i++) lv_obj_set_hidden(rows[i], true); }
};
static Kv kvDash, kvSens, kvNet, kvSet;

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
// fila de titulos en gris, como las claves del rotor
static void tableDraw(lv_event_t* e) {
    lv_draw_task_t* t = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t* base = (lv_draw_dsc_base_t*)lv_draw_task_get_draw_dsc(t);
    if (base->part != LV_PART_ITEMS || base->id1 != 0) return;
    if (lv_draw_task_get_type(t) == LV_DRAW_TASK_TYPE_LABEL) lv_draw_task_get_label_dsc(t)->color = C(P->mut);
}
static lv_obj_t* table(lv_obj_t* parent) {
    lv_obj_t* t = lv_table_create(parent);
    lv_obj_add_style(t, &stTable, 0);
    lv_obj_add_style(t, &stTableItems, LV_PART_ITEMS);
    lv_obj_add_event_cb(t, tableDraw, LV_EVENT_DRAW_TASK_ADDED, nullptr);
    lv_obj_set_send_draw_task_events(t, true);
    return t;
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
    lv_obj_add_style(ta, &stField, 0);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_max_length(ta, maxLen);
    if (accepted) lv_textarea_set_accepted_chars(ta, accepted);
    if (w) lv_obj_set_width(ta, w);
    lv_obj_add_event_cb(ta, fieldFocus, LV_EVENT_ALL, nullptr);
    return ta;
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
    lv_obj_add_style(mbMode, &stCard, 0);
    lv_obj_set_style_pad_all(mbMode, 0, 0);
    lv_obj_t* t = lv_msgbox_add_title(mbMode, "Change tracker mode");
    lv_obj_set_style_text_font(t, L.fBold, 0);
    lv_obj_t* hdr = lv_msgbox_get_header(mbMode);
    lv_obj_set_style_bg_color(hdr, C(P->hdr), 0);
    lv_obj_set_style_text_color(hdr, C(P->hdrTitle), 0);
    lv_msgbox_add_text(mbMode, "The tracker restarts in the selected mode.");
    lv_obj_set_style_bg_color(lv_msgbox_get_footer(mbMode), C(P->card), 0);
    lv_obj_set_width(mbMode, board::W - 20 < 520 ? board::W - 20 : 520);
    const int bw = L.sz == Size::S ? 90 : 160;
    lv_obj_t* bOk = lv_msgbox_add_footer_button(mbMode, "Switch");
    lv_obj_t* bNo = lv_msgbox_add_footer_button(mbMode, "Cancel");
    for (lv_obj_t* b : {bOk, bNo}) {
        lv_obj_add_style(b, &stBtn, 0);
        lv_obj_add_style(b, &stBtnPr, LV_STATE_PRESSED);
        lv_obj_set_size(b, bw, L.btnH);
    }
    lv_obj_add_style(bOk, &stBtnAcc, 0);
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
static void showBacklight() {
    char b[32];
    snprintf(b, sizeof b, "Backlight %d %%", backlight);
    lv_label_set_text(lblBack, b);
}
static void onBack(lv_event_t* e) {                              // - / + de 10 en 10, como el brillo del rotor
    backlight += (int)(intptr_t)lv_event_get_user_data(e);
    backlight = backlight < 10 ? 10 : backlight > 100 ? 100 : backlight;
    board::setBacklight(backlight);
    showBacklight();
}

// Paleta: se guarda en la pantalla y se aplica al arrancar (main reinicia)
static void onTheme(lv_event_t* e) {
    const int t = (int)(intptr_t)lv_event_get_user_data(e);
    if (t != themeIdx && themeCb) themeCb(t);
}

static void onTab(lv_event_t*) {
    // al abrir cada pestaña se piden sus datos al momento
    uint32_t t = lv_tabview_get_tab_active(tabs);
    if (t == 1) trk::send(proto::cmdHeard(0, 12));
    if (t == 2) trk::send(proto::cmdMsgs(0, 10));
    if (t == 5) trk::send(proto::cmdScreen(2)); else trk::send(proto::cmdScreen(0));
    if (t == 6) trk::send(proto::cmdSimple("dapnet"));
}

// ---------- construccion ----------
static void page(lv_obj_t* tab, lv_flex_flow_t flow = LV_FLEX_FLOW_COLUMN) {
    lv_obj_set_style_bg_color(tab, C(P->bg), 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(tab, flow);
    lv_obj_set_style_pad_all(tab, L.pad, 0);
    lv_obj_set_style_pad_gap(tab, L.pad, 0);
}

static void buildDash() {
    page(tabDash, LV_FLEX_FLOW_ROW_WRAP);
    kvDash.create(tabDash, "Tracker");
    lv_obj_t* ctl = card(tabDash, "Control");
    if (L.sz == Size::L) {                                        // dos columnas, como la informacion del rotor
        lv_obj_set_width(kvDash.box, LV_PCT(58));
        lv_obj_set_width(ctl, LV_PCT(39));
    }
    lv_obj_t* r = row(ctl);
    button(r, "Beacon now", onBeacon);
    lv_obj_t* r1 = row(ctl);
    label(r1, "Digipeater");
    swDigi = lv_switch_create(r1);
    lv_obj_set_style_bg_color(swDigi, C(P->btn), 0);
    lv_obj_add_event_cb(swDigi, onDigi, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_t* r2 = row(ctl);
    ddMode = lv_dropdown_create(r2);
    lv_obj_set_width(ddMode, L.ddW);
    lv_obj_add_style(ddMode, &stBtn, 0);
    lv_obj_set_style_text_font(ddMode, L.fBase, 0);
    lv_obj_set_style_text_font(ddMode, L.sz == Size::L ? &lv_font_montserrat_20 : L.sz == Size::M ? &lv_font_montserrat_16 : &lv_font_montserrat_14, LV_PART_INDICATOR);
    lv_obj_t* list = lv_dropdown_get_list(ddMode);
    lv_obj_set_style_bg_color(list, C(P->card), 0);
    lv_obj_set_style_text_color(list, C(P->fg), 0);
    lv_obj_set_style_border_color(list, C(P->line), 0);
    lv_dropdown_set_options(ddMode, "-");
    button(r2, "Switch mode", onModeSet, nullptr, 0, true);
}

static void buildHeard() {
    page(tabHeard);
    tblHeard = table(tabHeard);
    lv_obj_set_size(tblHeard, LV_PCT(100), LV_PCT(100));
}

static void buildMsgs() {
    page(tabMsgs);
    tblMsgs = table(tabMsgs);
    lv_obj_set_size(tblMsgs, LV_PCT(100), L.sz == Size::L && board::H >= 600 ? 300 : LV_PCT(48));
    lv_obj_add_event_cb(tblMsgs, onTblMsgs, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_t* c = card(tabMsgs);
    if (L.sz == Size::L) {
        lv_obj_t* r = row(c);
        taTo = field(r, "To (EA7XXX-9)", 200, 9);
        taText = field(r, "Message (max 67)", 400, 67);
        button(r, "Send", onSend, nullptr, 120, true);
    } else {                                                      // texto a todo el ancho; debajo destino y boton
        taText = field(c, "Message (max 67)", 0, 67);
        lv_obj_set_width(taText, LV_PCT(100));
        lv_obj_t* r = row(c);
        taTo = field(r, L.sz == Size::S ? "To" : "To (EA7XXX-9)", L.sz == Size::M ? 160 : 110, 9);
        button(r, "Send", onSend, nullptr, L.sz == Size::M ? 100 : 70, true);
    }
    lblSendRes = wrapLabel(c, "");
    lv_obj_add_style(lblSendRes, &stKey, 0);
}

static void buildSens() {
    page(tabSens, LV_FLEX_FLOW_ROW_WRAP);
    kvSens.create(tabSens, "Tracker sensors");
    kvNet.create(tabSens, "Internet weather");
    if (L.sz == Size::L) {
        lv_obj_set_width(kvSens.box, LV_PCT(48));
        lv_obj_set_width(kvNet.box, LV_PCT(49));
    }
}

// Brujula como la del rotor: N en rojo, E S W en blanco, grados en gris cada 30, aguja roja
static void buildCompass(lv_obj_t* parent) {
    compass = lv_scale_create(parent);
    lv_obj_set_size(compass, L.compass, L.compass);
    lv_scale_set_mode(compass, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(compass, 0, 360);
    lv_scale_set_angle_range(compass, 360);
    lv_scale_set_rotation(compass, 270);
    lv_scale_set_total_tick_count(compass, 73);                 // cada 5 grados
    lv_scale_set_major_tick_every(compass, 6);                  // con numero cada 30
    static const char* txt[] = {"N", "30", "60", "E", "120", "150", "S", "210", "240", "W", "300", "330", "", nullptr};
    lv_scale_set_text_src(compass, txt);
    lv_scale_set_label_show(compass, true);
    const int k = L.compass / 150;                              // 1 en la pequena, 2 en la grande
    lv_obj_set_style_bg_color(compass, C(P->card), 0);
    lv_obj_set_style_bg_opa(compass, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(compass, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_arc_color(compass, C(P->line), 0);
    lv_obj_set_style_arc_width(compass, 2 * k, 0);
    lv_obj_set_style_line_color(compass, C(P->line), LV_PART_ITEMS);
    lv_obj_set_style_length(compass, 5 * k, LV_PART_ITEMS);
    lv_obj_set_style_line_width(compass, 1 + k / 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(compass, C(P->mut), LV_PART_INDICATOR);
    lv_obj_set_style_length(compass, 10 * k, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(compass, 2 * k, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(compass, C(P->mut), LV_PART_INDICATOR);
    lv_obj_set_style_text_font(compass, L.fSub, LV_PART_INDICATOR);
    lv_obj_set_style_pad_radial(compass, 4 * k, LV_PART_INDICATOR);
    static lv_style_t stN, stCard4;
    lv_style_init(&stN);
    lv_style_set_text_color(&stN, C(P->red));
    lv_style_set_text_font(&stN, L.fBold);
    lv_style_set_line_color(&stN, C(P->red));
    lv_style_init(&stCard4);
    lv_style_set_text_color(&stCard4, C(P->fg));
    lv_style_set_text_font(&stCard4, L.fBold);
    lv_scale_section_t* sN = lv_scale_add_section(compass);
    lv_scale_set_section_range(compass, sN, 0, 0);
    lv_scale_set_section_style_indicator(compass, sN, &stN);
    for (int a : {90, 180, 270}) {
        lv_scale_section_t* s = lv_scale_add_section(compass);
        lv_scale_set_section_range(compass, s, a, a);
        lv_scale_set_section_style_indicator(compass, s, &stCard4);
    }
    needle = lv_line_create(compass);
    lv_obj_set_style_line_color(needle, C(P->needle), 0);
    lv_obj_set_style_line_width(needle, 3 * k, 0);
    lv_obj_set_style_line_rounded(needle, true, 0);
    lv_scale_set_line_needle_value(compass, needle, L.compass / 2 - 18 * k, 0);
    lv_obj_t* dot = lv_obj_create(compass);                     // eje de la aguja
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 8 * k + 4, 8 * k + 4);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(dot, C(P->fg), 0);
    lv_obj_set_style_border_width(dot, 2 * k, 0);
    lv_obj_set_style_bg_color(dot, C(P->card), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_center(dot);
}

static lv_obj_t* bigValue(lv_obj_t* parent, const char* key, lv_obj_t** value) {
    lv_obj_t* r = plain(parent);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, L.pad * 2, 0);
    lv_obj_t* k = label(r, key, L.fSub);
    lv_obj_add_style(k, &stKey, 0);
    *value = label(r, "--", L.fNum);
    lv_obj_add_style(*value, &stVal, 0);
    return r;
}

static void buildSat() {
    page(tabSat, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(tabSat, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    buildCompass(tabSat);
    lv_obj_t* right = plain(tabSat);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, L.pad, 0);
    const bool side = L.sz == Size::L || (L.sz == Size::M && !L.portrait);
    lv_obj_set_width(right, side ? board::W - L.tabBar - L.compass - 5 * L.pad : LV_PCT(100));
    if (side) lv_obj_set_flex_grow(right, 1);
    bigValue(right, "AZ", &lblAz);
    lv_obj_t* sep = lv_obj_create(right);                       // raya entre AZ y EL, como en el rotor
    lv_obj_remove_style_all(sep);
    lv_obj_set_size(sep, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(sep, C(P->line), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    bigValue(right, "EL", &lblEl);
    lblSatInfo = wrapLabel(right, "SAT mode inactive");
    lv_obj_add_style(lblSatInfo, &stKey, 0);
}

static void buildMirror() {
    page(tabMirror);
    lv_obj_t* c = card(tabMirror);
    lblMirrorTitle = wrapLabel(c, "Tracker screen (mirror)", L.fBold);
    lblMirror = label(c, "Open this tab with the tracker linked", L.fMono);
}

static void buildPages() {
    page(tabPages);
    tblPages = table(tabPages);
    lv_obj_set_size(tblPages, LV_PCT(100), LV_PCT(100));
}

static void buildSet() {
    page(tabSet, LV_FLEX_FLOW_ROW_WRAP);
    kvSet.create(tabSet, "Link");
    lv_obj_t* c = card(tabSet, "Tracker");
    if (L.sz == Size::L) {
        lv_obj_set_width(kvSet.box, LV_PCT(45));
        lv_obj_set_width(c, LV_PCT(52));
    }
    lv_obj_add_style(wrapLabel(c, "Control PIN (WebUI > Bluetooth > Control PIN, default 123456)"), &stKey, 0);
    taPin = field(c, "PIN", L.sz == Size::L ? 220 : 130, 6, "0123456789");
    char b[12]; snprintf(b, sizeof b, "%06lu", (unsigned long)trk::pin()); lv_textarea_set_text(taPin, b);
    lv_obj_add_style(wrapLabel(c, "Address (empty = first tracker heard)"), &stKey, 0);
    taPeer = field(c, "AA:BB:CC:DD:EE:FF", L.sz == Size::L ? 320 : L.sz == Size::M ? 240 : 200, 17);
    lv_textarea_set_text(taPeer, trk::peerAddress().c_str());
    lv_obj_t* r = row(c);
    button(r, L.sz == Size::S ? "Save" : "Save and reconnect", onSavePin, nullptr, L.sz == Size::L ? 260 : 0, true);
    button(r, L.sz == Size::S ? "Forget" : "Forget pairing", onForget);
    button(r, "Reconnect", onReconnect);
    lblPinRes = wrapLabel(c, "");
    setColor(lblPinRes, P->red);

    lv_obj_t* tc = card(tabSet, "Theme");                       // paleta de la app Android o de la pantalla del rotor
    lv_obj_t* tr = row(tc);
    static const char* names[] = {"App", "Rotor"};
    for (int i = 0; i < 2; i++) {
        lv_obj_t* tb = button(tr, names[i], onTheme, (void*)(intptr_t)i, L.sz == Size::S ? 90 : 140);
        if (i == themeIdx) { lv_obj_set_style_border_color(tb, C(P->selText), 0); lv_obj_set_style_text_color(tb, C(P->selText), 0); }
    }
    lv_obj_add_style(wrapLabel(tc, "The display restarts to apply it"), &stKey, 0);
    if (L.sz == Size::L) lv_obj_set_width(tc, LV_PCT(45));

    lv_obj_t* bc = card(tabSet);                                // - Backlight 100 % +, como el brillo del rotor
    lv_obj_t* br = row(bc);
    lv_obj_set_flex_align(br, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    button(br, LV_SYMBOL_MINUS, onBack, (void*)(intptr_t)-10, L.btnH * 2);
    lblBack = label(br, "");
    button(br, LV_SYMBOL_PLUS, onBack, (void*)(intptr_t)10, L.btnH * 2);
    showBacklight();
}

// Cabecera roja como la del rotor: icono, "APRS LoRa+" en negrita y debajo "Display <version>"; a la derecha el enlace BLE
static void buildHeader() {
    lv_obj_t* top = lv_obj_create(scr);
    lv_obj_remove_style_all(top);
    lv_obj_set_size(top, board::W, L.topH);
    lv_obj_set_style_bg_color(top, C(P->hdr), 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(top, C(P->hdrLine), 0);
    lv_obj_set_style_border_width(top, L.sz == Size::L ? 2 : 1, 0);
    lv_obj_set_style_border_side(top, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_hor(top, L.pad, 0);
    lv_obj_set_scrollable(top, false);

    lv_obj_t* icon = lv_image_create(top);                    // icono de la app, como en las WebUI y en el rotor
    lv_image_set_src(icon, &img_app_icon);
    if (L.iconPx != 40) {
        lv_image_set_scale(icon, 256 * L.iconPx / 40);
        lv_obj_set_size(icon, L.iconPx, L.iconPx);
        lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CENTER);
    }
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
    const int tx = L.iconPx + L.pad + (L.sz == Size::L ? 6 : 0);
    lv_obj_t* title = label(top, "APRS LoRa+", L.fTitle);
    lv_obj_set_style_text_color(title, C(P->hdrTitle), 0);
    lv_obj_t* sub = label(top, "Display " DISPLAY_VERSION, L.fSub);
    lv_obj_set_style_text_color(sub, C(P->sub), 0);
    const int th = lv_font_get_line_height(L.fTitle), sh = lv_font_get_line_height(L.fSub);
    const int y0 = (L.topH - th - sh) / 2;
    lv_obj_set_pos(title, tx, y0 - 1);
    lv_obj_set_pos(sub, tx, y0 + th - 3);

    topLink1 = label(top, "", L.fSub);                          // nombre del tracker (o estado)
    topLink2 = label(top, "BLE", L.fSub);                       // "BLE ●" enlazado
    if (L.sz == Size::S) {
        lv_obj_set_hidden(topLink1, true);
        lv_obj_align(topLink2, LV_ALIGN_RIGHT_MID, 0, 0);
    } else {
        lv_obj_align(topLink1, LV_ALIGN_TOP_RIGHT, 0, y0);
        lv_obj_align(topLink2, LV_ALIGN_BOTTOM_RIGHT, 0, -y0);
    }
    topInfo = label(top, "", L.fSub);                           // bateria del tracker
    lv_obj_set_style_text_color(topInfo, C(P->sub), 0);
    if (L.sz == Size::L) lv_obj_align(topInfo, LV_ALIGN_CENTER, 120, 0);
    else if (L.sz == Size::M) lv_obj_align(topInfo, LV_ALIGN_CENTER, 20, 0);
    else lv_obj_set_hidden(topInfo, true);
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
    lv_obj_set_style_text_color(ver, C(P->logo), 0);
    lv_obj_align(ver, LV_ALIGN_CENTER, 0, -L.topH / 2 + hShown / 2 + 4 + lv_font_get_line_height(L.fBase) / 2);
    lv_screen_load(sp);
    lv_obj_fade_in(img, 700, 0);
    lv_obj_fade_in(ver, 700, 300);
    lv_screen_load_anim(mainScr, LV_SCREEN_LOAD_ANIM_FADE_IN, 500, 2200, true);
}

void begin(proto::Model* model, int theme, void (*onThemeChange)(int)) {
    M = model;
    themeIdx = theme == THEME_ROTOR ? THEME_ROTOR : THEME_APP;
    P = themeIdx == THEME_ROTOR ? &PAL_ROTOR : &PAL_APP;
    themeCb = onThemeChange;
    L = makeLayout(board::W, board::H);
    pickColumns();
    // tema oscuro con el azul del rotor y la letra de esta pantalla; encima, los estilos de arriba
    lv_display_t* d = lv_display_get_default();
    lv_display_set_theme(d, lv_theme_default_init(d, C(P->acc), C(P->red), true, L.fBase));
    lv_obj_set_style_text_font(lv_layer_top(), L.fBase, 0);     // la ventana de modo y el teclado viven en la capa superior
    initStyles();

    scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, C(P->bg), 0);
    lv_obj_set_style_text_color(scr, C(P->fg), 0);
    lv_obj_set_style_text_font(scr, L.fBase, 0);
    buildHeader();

    tabs = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(tabs, L.tabPos);
    lv_tabview_set_tab_bar_size(tabs, L.tabBar);
    lv_obj_set_size(tabs, board::W, board::H - L.topH);
    lv_obj_set_pos(tabs, 0, L.topH);
    lv_obj_set_style_bg_color(tabs, C(P->bg), 0);
    lv_obj_t* bar = lv_tabview_get_tab_bar(tabs);
    lv_obj_set_style_bg_color(bar, C(P->card), 0);
    lv_obj_set_style_text_font(bar, L.fTab, 0);
    struct { const char* text; const char* icon; lv_obj_t** tab; } T[] = {
        {"Status", LV_SYMBOL_HOME, &tabDash}, {"Heard", LV_SYMBOL_LIST, &tabHeard}, {"Messages", LV_SYMBOL_ENVELOPE, &tabMsgs},
        {"Sensors", LV_SYMBOL_BATTERY_FULL, &tabSens}, {"SAT / Rotor", LV_SYMBOL_GPS, &tabSat}, {"Mirror", LV_SYMBOL_EYE_OPEN, &tabMirror},
        {"DAPNET", LV_SYMBOL_BELL, &tabPages}, {"Settings", LV_SYMBOL_SETTINGS, &tabSet}};
    for (auto& t : T) *t.tab = lv_tabview_add_tab(tabs, L.tabIcons ? t.icon : t.text);
    for (uint32_t i = 0; i < lv_obj_get_child_count(bar); i++) {
        lv_obj_t* b = lv_obj_get_child(bar, i);
        lv_obj_add_style(b, &stTabBtn, 0);
        lv_obj_add_style(b, &stTabBtnOn, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(tabs, onTab, LV_EVENT_VALUE_CHANGED, nullptr);

    kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(kb, board::W, L.kbH);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, C(P->card), 0);
    lv_obj_set_style_bg_color(kb, C(P->btn), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, C(P->btnText), LV_PART_ITEMS);
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
static std::string modeName(int id) {
    for (size_t i = 0; i < M->info.modeIds.size() && i < M->info.modeNames.size(); i++)
        if (M->info.modeIds[i] == id) return M->info.modeNames[i];
    char b[16]; snprintf(b, sizeof b, "%d", id);
    return b;
}

void update() {
    if (!M) return;
    char b[512];
    const trk::State s = trk::state();
    const bool linked = s == trk::State::Linked;

    // cabecera: nombre del tracker arriba, "BLE ●" debajo (blanco enlazado, rosa si no), como WiFi / BLE en el rotor
    setText(topLink1, linked ? trk::peerName().c_str() : trk::stateText());
    setColor(topLink1, linked ? P->hdrTitle : P->off);
    setText(topLink2, linked ? "BLE " "\xE2\x97\x8F" : "BLE");
    setColor(topLink2, linked ? P->on : P->off);

    // ajustes: enlace
    kvSet.begin();
    kvSet.add("BLE", trk::stateText(), linked ? P->green : P->yel);
    kvSet.add("Tracker", linked ? trk::peerName().c_str() : "-");
    kvSet.add("Board", M->info.ok ? M->info.board.c_str() : "-");
    kvSet.add("Firmware", M->info.ok ? M->info.fw.c_str() : "-");
    kvSet.add("Display", BOARD_ENV " " DISPLAY_VERSION);
    kvSet.end();

    if (M->revision == drawnRev) return;
    drawnRev = M->revision;

    if (M->status.ok && M->status.batt > 0) snprintf(b, sizeof b, "%.2f V", M->status.batt); else b[0] = 0;
    setText(topInfo, b);

    // Estado: lista clave / valor
    kvDash.begin();
    if (M->gnss.ok) {
        kvDash.addf("GNSS", M->gnss.fix ? P->green : P->yel, "%s, %d sats", M->gnss.fix ? "fix" : "no fix", M->gnss.sats);
        kvDash.addf("Position", P->fg, "%.5f, %.5f", M->gnss.lat, M->gnss.lon);
        kvDash.addf("Altitude", P->fg, "%.0f m", M->gnss.alt);
        kvDash.addf("Speed", P->fg, "%.0f km/h", M->gnss.spd);
        kvDash.addf("HDOP", P->fg, "%.1f", M->gnss.hdop);
        if (!M->gnss.utc.empty()) kvDash.add("UTC", M->gnss.utc.c_str());
    } else {
        kvDash.add("GNSS", "-", P->mut);
    }
    if (M->status.ok) {
        kvDash.add("Mode", modeName(M->status.mode).c_str());
        kvDash.addf("Profile", P->fg, "%d  (LoRa %d)", M->status.profile, M->status.lora);
        std::string fl;
        if (M->status.smart) fl += "SmartBeacon ";
        if (M->status.digi) fl += "digi ";
        if (!fl.empty()) kvDash.add("Active", fl.c_str());
        if (M->status.sos) kvDash.add("SOS", "on", P->red);
        kvDash.addf("Uptime", P->fg, "%luh %02lum", (unsigned long)(M->status.up / 3600), (unsigned long)(M->status.up / 60 % 60));
        if (M->status.digi) lv_obj_add_state(swDigi, LV_STATE_CHECKED); else lv_obj_remove_state(swDigi, LV_STATE_CHECKED);
    }
    if (!M->lastError.empty()) kvDash.add("Error", M->lastError.c_str(), P->red);
    kvDash.end();
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

    // Sensores del tracker y tiempo de internet (la app lo baja de OpenWeatherMap / IQAir y lo manda al tracker)
    {
        const proto::Sensors& sn = M->sensors;
        kvSens.begin();
        if (sn.ok) {
            if (sn.batt > 0) kvSens.addf("Battery", P->fg, "%.2f V", sn.batt);
            if (sn.battPct >= 0) kvSens.addf("Charge", P->fg, "%d %%", sn.battPct);
            if (sn.temp > -900) kvSens.addf("Temperature", P->fg, "%.1f \xC2\xB0" "C", sn.temp);
            if (sn.hum >= 0) kvSens.addf("Humidity", P->fg, "%.0f %%", sn.hum);
            if (sn.press > 0) kvSens.addf("Pressure", P->fg, "%.1f hPa", sn.press);
            if (sn.qnh > 0) kvSens.addf("QNH", P->fg, "%.1f hPa%s", sn.qnh, sn.qnhSrc == "net" ? "  (internet)" : sn.qnhSrc == "man" ? "  (manual)" : "");
            if (sn.baroAlt > -900) kvSens.addf("Baro altitude", P->fg, "%.0f m", sn.baroAlt);
            if (sn.heading >= 0) kvSens.addf("Heading", P->fg, "%.0f\xC2\xB0", sn.heading);
        } else {
            kvSens.add("-", "No sensor data yet", P->mut);
        }
        kvSens.end();
        const proto::NetWx& n = sn.net;
        lv_obj_set_hidden(kvNet.box, !(sn.ok && n.ok));
        if (sn.ok && n.ok) {
            kvNet.begin();
            kvNet.addf("Source", P->fg, "%s%s%s, %u min ago", n.src.c_str(), n.place.empty() ? "" : " \xC2\xB7 ", n.place.c_str(), (unsigned)(n.age / 60));
            if (!n.desc.empty()) kvNet.add("Sky", n.desc.c_str());
            if (n.t > -900) kvNet.addf("Temperature", P->fg, "%.1f \xC2\xB0" "C", n.t);
            if (n.h >= 0) kvNet.addf("Humidity", P->fg, "%.0f %%", n.h);
            if (n.p > 0) {
                if (n.d3 > -900) kvNet.addf("Pressure", P->fg, "%.1f hPa (sea level), %+.1f in 3 h", n.p, n.d3);
                else kvNet.addf("Pressure", P->fg, "%.1f hPa (sea level)", n.p);
            }
            if (!n.forecast.empty()) kvNet.add("Forecast", n.forecast.c_str());
            if (n.ws >= 0) {
                char w[64];
                int k = snprintf(w, sizeof w, "%.1f m/s", n.ws);
                if (n.wg >= 0) k += snprintf(w + k, sizeof w - k, ", gusts %.1f", n.wg);
                if (n.wd >= 0) snprintf(w + k, sizeof w - k, " from %d\xC2\xB0", n.wd);
                kvNet.add("Wind", w);
            }
            if (n.rain >= 0) kvNet.addf("Rain", P->fg, "%.1f mm/h", n.rain);
            if (n.clouds >= 0) kvNet.addf("Clouds", P->fg, "%d %%", n.clouds);
            if (n.aqiUs >= 0) kvNet.addf("Air quality", P->fg, "US AQI %d%s%s", n.aqiUs, n.pollutant.empty() ? "" : " ", n.pollutant.c_str());
            else if (n.aqiOwm > 0) {
                static const char* names[] = {"", "good", "fair", "moderate", "poor", "very poor"};
                kvNet.addf("Air quality", P->fg, "%d/5 %s", n.aqiOwm, names[n.aqiOwm > 5 ? 5 : n.aqiOwm]);
            }
            if (n.pm25 >= 0) kvNet.addf("PM2.5", P->fg, "%.1f \xC2\xB5g/m\xC2\xB3", n.pm25);
            if (n.pm10 >= 0) kvNet.addf("PM10", P->fg, "%.1f \xC2\xB5g/m\xC2\xB3", n.pm10);
            kvNet.end();
        }
    }

    // SAT / rotor: brujula y AZ / EL grandes
    {
        bool have = false;
        float az = 0, el = 0;
        std::string info;
        if (M->sat.ok) {
            have = true; az = M->sat.az; el = M->sat.el;
            snprintf(b, sizeof b, "%s\nRange %.0f km, max el %d\xC2\xB0", M->sat.name.c_str(), M->sat.rng, M->sat.maxEl);
            info = b;
        }
        if (M->rot.ok) {
            static const char* srcs[] = {"satellite", "APRS compass", "fixed target", "external", "last APRS heard", "last CW beacon"};
            snprintf(b, sizeof b, "Rotor: %s %s%s", M->rot.src >= 0 && M->rot.src < 6 ? srcs[M->rot.src] : "?", M->rot.name.c_str(), M->rot.trk ? " (tracking)" : "");
            if (!info.empty()) info += "\n";
            info += b;
            if (!have) { have = true; az = M->rot.az; el = M->rot.el; }
        }
        if (have) {
            snprintf(b, sizeof b, "%.1f\xC2\xB0", az); setText(lblAz, b);
            snprintf(b, sizeof b, "%.1f\xC2\xB0", el); setText(lblEl, b);
            int a = (int)lroundf(az) % 360; if (a < 0) a += 360;
            lv_scale_set_line_needle_value(compass, needle, L.compass / 2 - 18 * (L.compass / 150), a);
            setText(lblSatInfo, info.c_str());
        } else {
            setText(lblAz, "--"); setText(lblEl, "--");
            setText(lblSatInfo, "SAT mode inactive");
        }
    }

    // Espejo
    if (M->screen.ok) {
        snprintf(b, sizeof b, "%s   %s", M->screen.title.c_str(), M->screen.right.c_str());
        setText(lblMirrorTitle, b);
        std::string t;
        for (const std::string& r : M->screen.rows) { t += r; t += '\n'; }
        setText(lblMirror, t.c_str());
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
#endif
