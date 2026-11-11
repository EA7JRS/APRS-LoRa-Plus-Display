/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Nucleo de la interfaz tipo emisora (7B v2): paletas, piezas (Widgets.kt de la app), navegacion con pila, barra de estado,
 * teclas tactiles, menu lateral, teclado, avisos, ventanas de confirmacion y de alerta, y el arranque.
 */
#ifdef UI_RADIO
#include "r7.h"
#include "ui.h"
#include "board.h"
#include "net.h"
#include "settings.h"
#include "tracker_link.h"
#include "ui_assets.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

extern "C" {
#ifdef R7_SCALED
extern const lv_image_dsc_t aprs_sym22_0, aprs_sym22_1, aprs_sym22_2;
#define aprs_sym_0 aprs_sym22_0
#define aprs_sym_1 aprs_sym22_1
#define aprs_sym_2 aprs_sym22_2
#else
extern const lv_image_dsc_t aprs_sym_0, aprs_sym_1, aprs_sym_2;
#endif
}

namespace r7 {

// ------------------------------------------------------------------ paletas (Theme.kt)
static const Pal DARK = {0x131A22, 0x0B1016, 0x18222D, 0x263443, 0x161F29, 0x222F3C, 0x1E2A36, 0x2E3E4F, 0x202D3A, 0x3CE68A, 0x2F8F5D, 0x1D3A30,
                         0x28B5E1, 0xFFB347, 0xA8763A, 0xFF6B6B, 0xFF8A80, 0xE8EEF3, 0x9FB0BF, 0x6F8293, 0xC9D4DE, 0x8FA3B6, 0x4A5D70, 0xD9E1E8,
                         0x6A7F94, 0x000000, 0x3A4A5B, true};
static const Pal LIGHT = {0xF2F5F8, 0x0B1016, 0xFFFFFF, 0xD3DCE5, 0xEEF2F6, 0xD8E0E8, 0xE7EDF3, 0xC0CCD8, 0xE2EEF8, 0x0B9A52, 0x6CC79A, 0xDDF4E7,
                          0x0779AB, 0xB86200, 0xE0AE70, 0xD32F2F, 0xC62828, 0x14202B, 0x52616F, 0x7E8D9B, 0x2C3A47, 0x56687A, 0xA7B4C0, 0xE9EEF3,
                          0x7A8C9D, 0xFFFFFF, 0xB7C3CF, false};
const Pal* P = &DARK;
static const uint32_t S_COLORS[15] = {0x4FC3E0, 0x52D4C0, 0x5BDFA0, 0x72E688, 0x96EA74, 0xD4E650, 0xF2DE3E, 0xFFCF33, 0xFF9A2E,
                                      0xFF5A3C, 0xFF4636, 0xFF3A33, 0xFF3030, 0xFF2A2A, 0xFF2424};
static uint32_t dflt(uint32_t v, uint32_t d) { return v == 0xFFFFFFFF ? d : v; }

// ------------------------------------------------------------------ datos
proto::Model* M = nullptr;
rotp::Model RM;
DxStore DX;
static void (*themeCb)(int) = nullptr;

uint32_t nowMs() { return lv_tick_get(); }

std::string upper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }

bool ownPos(double& lat, double& lon) {
    if (!M || !M->gnss.ok || !M->gnss.fix) return false;
    lat = M->gnss.lat; lon = M->gnss.lon;
    return true;
}

std::string myCall() {
    if (M && M->info.ok && M->status.ok && M->status.profile >= 0 && M->status.profile < (int)M->info.profiles.size())
        return M->info.profiles[M->status.profile].call;
    if (M && M->info.ok && !M->info.profiles.empty()) return M->info.profiles[0].call;
    return cfg::get().dxCall;
}

// hora: NTP si la hay; si no, la ultima UTC del GNSS del tracker mas lo transcurrido
static int64_t gnssEpoch = 0;
static uint32_t gnssEpochAt = 0;
static std::string gnssUtcSeen;
static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}
bool utcNow(int& y, int& mo, int& d, int& h, int& mi, int& s) {
    int64_t ep = 0;
    if (net::timeValid()) ep = (int64_t)time(nullptr);
    else {
        if (M && M->gnss.ok && M->gnss.utc.size() >= 19 && M->gnss.utc != gnssUtcSeen) {
            int Y, Mo, D, Hh, Mi, Ss;
            if (sscanf(M->gnss.utc.c_str(), "%d-%d-%dT%d:%d:%d", &Y, &Mo, &D, &Hh, &Mi, &Ss) == 6) {
                gnssEpoch = daysFromCivil(Y, Mo, D) * 86400 + Hh * 3600 + Mi * 60 + Ss;
                gnssEpochAt = nowMs();
            }
            gnssUtcSeen = M->gnss.utc;
        }
        if (!gnssEpoch) return false;
        ep = gnssEpoch + (nowMs() - gnssEpochAt) / 1000;
    }
    int64_t days = ep / 86400, rem = ep % 86400;
    h = (int)(rem / 3600); mi = (int)(rem / 60 % 60); s = (int)(rem % 60);
    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = (unsigned)(days - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    mo = (int)(mp < 10 ? mp + 3 : mp - 9);
    y = (int)(yoe + era * 400 + (mo <= 2));
    return true;
}

// ------------------------------------------------------------------ piezas
static void baseObj(lv_obj_t* o) {
    lv_obj_remove_style_all(o);
    lv_obj_set_scrollable(o, false);
}
lv_obj_t* box(lv_obj_t* p) {
    lv_obj_t* o = lv_obj_create(p);
    baseObj(o);
    lv_obj_set_size(o, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return o;
}
lv_obj_t* col(lv_obj_t* p, int gap) {
    lv_obj_t* o = box(p);
    lv_obj_set_width(o, LV_PCT(100));
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(o, gap, 0);
    return o;
}
lv_obj_t* row(lv_obj_t* p, int gap) {
    lv_obj_t* o = box(p);
    lv_obj_set_width(o, LV_PCT(100));
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(o, gap, 0);
    return o;
}
lv_obj_t* panel(lv_obj_t* p, int pad, uint32_t bg, uint32_t border) {
    lv_obj_t* o = box(p);
    lv_obj_set_width(o, LV_PCT(100));
    lv_obj_set_style_bg_color(o, C(dflt(bg, P->panel)), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, C(dflt(border, P->panelBorder)), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_radius(o, RADIUS, 0);
    lv_obj_set_style_pad_all(o, pad, 0);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(o, 6, 0);
    return o;
}
lv_obj_t* text(lv_obj_t* p, const char* t, const lv_font_t* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, C(dflt(color, P->text)), 0);
    return l;
}
lv_obj_t* wrap(lv_obj_t* p, const char* t, const lv_font_t* f, uint32_t color) {
    lv_obj_t* l = text(p, t, f, dflt(color, P->muted));
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_WRAP);
    return l;
}
lv_obj_t* section(lv_obj_t* p, const char* t) {
    lv_obj_t* l = text(p, upper(t).c_str(), &app_cond_16, P->muted);
    lv_obj_set_style_text_letter_space(l, 2, 0);
    return l;
}
void setText(lv_obj_t* l, const char* t) { if (l && strcmp(lv_label_get_text(l), t) != 0) lv_label_set_text(l, t); }
void setTextf(lv_obj_t* l, const char* fmt, ...) {
    char b[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    setText(l, b);
}
void setColor(lv_obj_t* l, uint32_t c) { lv_obj_set_style_text_color(l, C(c), 0); }
void show(lv_obj_t* o, bool on) { if (o && lv_obj_is_hidden(o) == on) lv_obj_set_hidden(o, !on); }
void grow(lv_obj_t* o, int g) { lv_obj_set_flex_grow(o, g); }
void oneLine(lv_obj_t* l) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    (lv_obj_set_height)(l, lv_font_get_line_height(lv_obj_get_style_text_font(l, LV_PART_MAIN)));   // puntos reales
}

void Key::set(const char* l, const char* s) {
    setText(lab, l);
    if (sub) { setText(sub, s ? s : ""); show(sub, s && *s); }
}
void Key::style(uint32_t color, uint32_t border, uint32_t subColor, uint32_t bg) {
    lv_obj_set_style_border_color(obj, C(border), 0);
    setColor(lab, color);
    if (sub) setColor(sub, dflt(subColor, P->muted));
    lv_obj_set_style_bg_color(obj, C(dflt(bg, P->key)), 0);
}
Key key(lv_obj_t* p, const char* label, const char* sub, int w, int h, lv_event_cb_t cb, void* ud, const lv_font_t* f) {
    Key k;
    k.obj = lv_button_create(p);
    lv_obj_remove_style_all(k.obj);
    lv_obj_set_size(k.obj, w, h);
    lv_obj_set_style_bg_color(k.obj, C(P->key), 0);
    lv_obj_set_style_bg_opa(k.obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(k.obj, C(P->selected), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(k.obj, C(P->keyBorder), 0);
    lv_obj_set_style_border_width(k.obj, 1, 0);
    lv_obj_set_style_radius(k.obj, 12, 0);
    lv_obj_set_flex_flow(k.obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(k.obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(k.obj, 0, 0);
    k.lab = text(k.obj, label, f, P->text);
    k.sub = text(k.obj, sub ? sub : "", &app_cond_16, P->muted);
    show(k.sub, sub && *sub);
    if (cb) lv_obj_add_event_cb(k.obj, cb, LV_EVENT_CLICKED, ud);
    return k;
}
lv_obj_t* chip(lv_obj_t* p, const char* label, bool sel, lv_event_cb_t cb, void* ud, int h, uint32_t color) {
    lv_obj_t* c = lv_button_create(p);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, LV_SIZE_CONTENT, h);
    lv_obj_set_style_radius(c, h / 2, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_pad_hor(c, 16, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_t* l = text(c, label, &app_bold_18, P->text);
    lv_obj_set_style_text_letter_space(l, 1, 0);
    lv_obj_center(l);
    chipSet(c, sel, color);
    if (cb) lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, ud);
    return c;
}
void chipSet(lv_obj_t* c, bool sel, uint32_t color) {
    uint32_t col = dflt(color, P->cyan);
    lv_obj_set_style_bg_color(c, C(sel ? col : P->bg), 0);
    lv_obj_set_style_bg_opa(c, sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(c, C(sel ? col : P->keyBorder), 0);
    setColor(lv_obj_get_child(c, 0), sel ? P->bg : P->text);
}
lv_obj_t* button(lv_obj_t* p, const char* label, lv_event_cb_t cb, void* ud, int w, int h, uint32_t color) {
    Key k = key(p, label, nullptr, w ? w : 10, h, cb, ud, &app_bold_22);
    if (!w) { lv_obj_set_width(k.obj, LV_SIZE_CONTENT); lv_obj_set_style_pad_hor(k.obj, 22, 0); lv_obj_set_style_min_width(k.obj, 110, 0); }
    if (color != 0xFFFFFFFF) buttonColor(k.obj, color);
    return k.obj;
}
void buttonColor(lv_obj_t* b, uint32_t color) {
    lv_obj_set_style_border_color(b, C(color), 0);
    setColor(lv_obj_get_child(b, 0), color);
}
lv_obj_t* lamp(lv_obj_t* p, int size) {
    lv_obj_t* l = box(p);
    lv_obj_set_size(l, size, size);
    lv_obj_set_style_radius(l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(l, C(P->off), 0);
    return l;
}
void lampSet(lv_obj_t* l, bool on, uint32_t color) { lv_obj_set_style_bg_color(l, C(on ? dflt(color, P->green) : P->off), 0); }

Led led(lv_obj_t* p, const char* label, uint32_t color) {
    Led d;
    d.color = color;
    lv_obj_t* c = box(p);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    d.halo = lamp(c, 20);
    lv_obj_set_style_bg_opa(d.halo, LV_OPA_TRANSP, 0);
    d.dot = lamp(d.halo, 12);
    lv_obj_center(d.dot);
    d.lab = text(c, label, &app_cond_16, P->muted);
    return d;
}
void Led::set(bool on, bool ring) {
    lv_obj_set_style_bg_color(halo, C(color), 0);
    lv_obj_set_style_bg_opa(halo, on ? LV_OPA_40 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(dot, C(on ? color : P->off), 0);
    lv_obj_set_style_border_width(dot, on && ring ? 2 : 0, 0);
    lv_obj_set_style_border_color(dot, lv_color_white(), 0);
    setColor(lab, on ? color : P->muted);
}

SMeter smeter(lv_obj_t* p, int w) {
    SMeter m;
    m.obj = row(p, 12);
    lv_obj_set_width(m.obj, w);
    lv_obj_t* left = col(m.obj, 2);
    grow(left);
    lv_obj_set_width(left, 10);
    lv_obj_t* bar = row(left, 3);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    for (int i = 0; i < 15; i++) {
        if (i == 9) {                                           // raya blanca entre S9 y los dB de encima
            lv_obj_t* t = box(bar);
            lv_obj_set_size(t, 3, 22);
            lv_obj_set_style_bg_color(t, C(P->text), 0);
            lv_obj_set_style_bg_opa(t, LV_OPA_80, 0);
            lv_obj_set_style_margin_hor(t, 3, 0);
        }
        lv_obj_t* s = box(bar);
        lv_obj_set_height(s, i < 9 ? 14 : 18);
        lv_obj_set_flex_grow(s, 1);
        lv_obj_set_style_radius(s, 3, 0);
        lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
        m.seg[i] = s;
    }
    lv_obj_t* sc = row(left, 0);                                // escala 1 3 5 7 9 +20 +40 +60
    static const char* lbl[] = {"1", "3", "5", "7", "9", "+20", "+40", "+60"};
    for (int i = 0; i < 8; i++) {
        lv_obj_t* l = text(sc, lbl[i], &app_cond_16, i == 4 ? S_COLORS[8] : P->faint);
        lv_obj_set_flex_grow(l, i < 4 ? 2 : i == 4 ? 1 : 2);
        lv_obj_set_width(l, 10);
        if (i > 4) lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_obj_t* right = col(m.obj, 0);
    lv_obj_set_width(right, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(right, 110, 0);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    m.reading = text(right, "S-", &app_mono_28, P->off);
    m.extra = text(right, "", &app_cond_16, P->muted);
    m.set(nullptr, nullptr);
    return m;
}
void SMeter::set(const proto::SReading* r, const char* ex) {
    int lit = r ? r->segments : 0;
    for (int i = 0; i < 15; i++) {
        lv_obj_set_style_bg_color(seg[i], C(S_COLORS[i]), 0);
        lv_obj_set_style_bg_opa(seg[i], (lv_opa_t)(i < lit ? LV_OPA_COVER : 38), 0);
    }
    setText(reading, r ? r->label.c_str() : "S-");
    setColor(reading, !r || lit == 0 ? P->off : S_COLORS[lit - 1]);
    setText(extra, ex ? ex : "");
}

Tile tile(lv_obj_t* p, const char* label, const char* unit, int w) {
    Tile t;
    t.obj = panel(p, 12);
    lv_obj_set_width(t.obj, w);
    lv_obj_set_style_pad_row(t.obj, 2, 0);
    section(t.obj, label);
    lv_obj_t* r = row(t.obj, 6);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    t.val = text(r, "-", &app_mono_28, P->text);
    t.unit = text(r, unit ? unit : "", &app_text_18, P->muted);
    return t;
}
void Tile::set(const char* v, uint32_t color) { setText(val, v); setColor(val, dflt(color, P->text)); }

void Rows::create(lv_obj_t* p, int kw) { box = col(p, 4); keyW = kw; }
void Rows::add(const char* k, const char* v, uint32_t color) {
    if (n == r.size()) {
        lv_obj_t* o = row(box, 8);
        lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_t* kl = text(o, "", &app_cond_18, P->muted);
        lv_obj_set_width(kl, keyW);
        lv_obj_t* vl = text(o, "", font, P->text);
        lv_obj_set_flex_grow(vl, 1);
        lv_obj_set_width(vl, 10);
        lv_label_set_long_mode(vl, LV_LABEL_LONG_MODE_WRAP);
        r.push_back(o);
    }
    lv_obj_t* o = r[n++];
    show(o, true);
    setText(lv_obj_get_child(o, 0), k);
    setText(lv_obj_get_child(o, 1), v);
    setColor(lv_obj_get_child(o, 1), dflt(color, P->text));
}
void Rows::addf(const char* k, uint32_t color, const char* fmt, ...) {
    char b[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    add(k, b, color);
}
void Rows::end() { for (size_t i = n; i < r.size(); i++) show(r[i], false); }

static void onBack(lv_event_t*) { back(); }
lv_obj_t* header(lv_obj_t* root, const char* title, lv_obj_t** trailing) {
    lv_obj_t* h = row(root, 8);
    lv_obj_set_height(h, 56);
    lv_obj_t* b = lv_button_create(h);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 56, 52);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_set_style_bg_color(b, C(P->selected), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_t* a = text(b, "\xE2\x80\xB9", &app_bold_34, P->text);      // ‹
    lv_obj_center(a);
    lv_obj_add_event_cb(b, onBack, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* t = text(h, upper(title).c_str(), &app_bold_26, P->text);
    lv_obj_set_style_text_letter_space(t, 1, 0);
    oneLine(t);
    lv_obj_set_flex_grow(t, 1);
    lv_obj_set_width(t, 10);
    lv_obj_t* tr = row(h, 8);
    lv_obj_set_width(tr, LV_SIZE_CONTENT);
    if (trailing) *trailing = tr;
    return h;
}
lv_obj_t* scrollArea(lv_obj_t* root) {
    lv_obj_t* s = col(root, GAP);
    lv_obj_set_flex_grow(s, 1);
    lv_obj_set_height(s, 10);
    lv_obj_set_scrollable(s, true);
    lv_obj_set_scroll_dir(s, LV_DIR_VER);
    lv_obj_set_style_pad_right(s, 4, 0);
    lv_obj_set_style_pad_bottom(s, 8, 0);
    return s;
}

// ------------------------------------------------------------------ simbolos APRS
static const lv_image_dsc_t* sheet(int i) { return i == 0 ? &aprs_sym_0 : i == 1 ? &aprs_sym_1 : &aprs_sym_2; }
lv_obj_t* symbol(lv_obj_t* p) {
    lv_obj_t* b = box(p);
    lv_obj_set_size(b, 34, 34);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_bg_color(b, C(P->symbolBg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    for (int i = 0; i < 2; i++) {
        lv_obj_t* im = lv_image_create(b);
        (lv_obj_set_size)(im, SYM, SYM);
        lv_obj_center(im);
        lv_obj_set_hidden(im, true);
    }
    return b;
}
static void cellOf(lv_obj_t* im, int sh, char c) {
    int i = c - '!';
    lv_image_set_src(im, sheet(sh));
    lv_image_set_inner_align(im, LV_IMAGE_ALIGN_TOP_LEFT);
    (lv_obj_set_size)(im, SYM, SYM);
    lv_image_set_offset_x(im, -(i % 16) * SYM);
    lv_image_set_offset_y(im, -(i / 16) * SYM);
    lv_obj_set_hidden(im, false);
}
void symbolSet(lv_obj_t* s, char table, char code) {
    lv_obj_t* base = lv_obj_get_child(s, 0), *ov = lv_obj_get_child(s, 1);
    if (code < '!' || code > '~') { lv_obj_set_hidden(base, true); lv_obj_set_hidden(ov, true); return; }
    cellOf(base, table == '/' ? 0 : 1, code);
    bool overlay = table != '/' && table != '\\' && ((table >= '0' && table <= '9') || (table >= 'A' && table <= 'Z') || (table >= 'a' && table <= 'j'));
    if (overlay) cellOf(ov, 2, table); else lv_obj_set_hidden(ov, true);
}
const char* symbolName(char table, char code) {
    struct N { char c; const char* n; };
    static const N P1[] = {{'!', "Police"}, {'#', "Digipeater"}, {'$', "Phone"}, {'%', "DX cluster"}, {'&', "HF gateway"}, {'\'', "Small aircraft"},
        {'(', "Mobile satellite station"}, {'*', "Snowmobile"}, {'+', "Red Cross"}, {',', "Boy Scouts"}, {'-', "House"}, {'.', "X"}, {'/', "Red dot"},
        {':', "Fire"}, {';', "Campground"}, {'<', "Motorcycle"}, {'=', "Railroad engine"}, {'>', "Car"}, {'?', "File server"}, {'@', "Hurricane"},
        {'A', "Aid station"}, {'B', "BBS"}, {'C', "Canoe"}, {'E', "Eyeball"}, {'F', "Tractor"}, {'G', "Grid square"}, {'H', "Hotel"}, {'I', "TCP/IP"},
        {'K', "School"}, {'L', "PC user"}, {'M', "MacAPRS"}, {'N', "NTS station"}, {'O', "Balloon"}, {'P', "Police"}, {'R', "Recreational vehicle"},
        {'S', "Space shuttle"}, {'T', "SSTV"}, {'U', "Bus"}, {'V', "ATV"}, {'W', "Weather service"}, {'X', "Helicopter"}, {'Y', "Sailboat"},
        {'Z', "Windows"}, {'[', "Person"}, {'\\', "DF station"}, {']', "Post office"}, {'^', "Large aircraft"}, {'_', "Weather station"},
        {'`', "Satellite dish"}, {'a', "Ambulance"}, {'b', "Bicycle"}, {'c', "Incident command"}, {'d', "Fire station"}, {'e', "Horse"},
        {'f', "Fire truck"}, {'g', "Glider"}, {'h', "Hospital"}, {'i', "IOTA"}, {'j', "Jeep"}, {'k', "Truck"}, {'l', "Laptop"}, {'m', "Mic-E repeater"},
        {'n', "Node"}, {'o', "EOC"}, {'p', "Dog"}, {'q', "Grid square"}, {'r', "Repeater"}, {'s', "Ship"}, {'t', "Truck stop"}, {'u', "Semi truck"},
        {'v', "Van"}, {'w', "Water station"}, {'x', "X / Unix"}, {'y', "House with yagi"}, {'z', "Shelter"}};
    static const N P2[] = {{'!', "Emergency"}, {'#', "Digipeater"}, {'&', "Gateway"}, {'\'', "Crash site"}, {'(', "Cloudy"}, {'*', "Snow"},
        {'+', "Church"}, {'-', "House (HF)"}, {'>', "Car"}, {'A', "Box"}, {'D', "Drizzle"}, {'F', "Freezing rain"}, {'H', "Haze"}, {'L', "Lighthouse"},
        {'N', "Navigation buoy"}, {'O', "Rocket"}, {'P', "Parking"}, {'R', "Restaurant"}, {'S', "Satellite"}, {'T', "Thunderstorm"}, {'U', "Sunny"},
        {'W', "Weather station"}, {'[', "Wall cloud"}, {'^', "Aircraft"}, {'_', "Weather station"}, {'a', "ARES / RACES"}, {'b', "Blowing dust"},
        {'e', "Sleet"}, {'f', "Funnel cloud"}, {'g', "Gale"}, {'h', "Store"}, {'j', "Work zone"}, {'k', "SUV"}, {'s', "Ship"}, {'t', "Tornado"},
        {'u', "Truck"}, {'v', "Van"}, {'w', "Flooding"}, {'y', "Skywarn"}};
    const N* t = table == '/' ? P1 : P2;
    size_t n = table == '/' ? sizeof P1 / sizeof *P1 : sizeof P2 / sizeof *P2;
    for (size_t i = 0; i < n; i++) if (t[i].c == code) return t[i].n;
    return "";
}

// ------------------------------------------------------------------ listas
void List::create(lv_obj_t* p, lv_obj_t* (*mk)(lv_obj_t*), lv_event_cb_t click) { box = col(p, 4); make = mk; onClick = click; }
lv_obj_t* List::next() {
    if (n == rows.size()) {
        lv_obj_t* r = make(box);
        if (onClick) { lv_obj_set_clickable(r, true); lv_obj_add_event_cb(r, onClick, LV_EVENT_CLICKED, nullptr); }
        rows.push_back(r);
    }
    lv_obj_t* r = rows[n];
    lv_obj_set_user_data(r, (void*)(intptr_t)n);
    n++;
    show(r, true);
    return r;
}
void List::end() { for (size_t i = n; i < rows.size(); i++) show(rows[i], false); }
int clickIndex(lv_event_t* e) { return (int)(intptr_t)lv_obj_get_user_data(lv_event_get_current_target_obj(e)); }

// ------------------------------------------------------------------ brujula
static void compassDraw(lv_event_t* e) {
    lv_obj_t* o = lv_event_get_target_obj(e);
    Compass* c = (Compass*)lv_event_get_user_data(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const int cx = (a.x1 + a.x2) / 2, cy = (a.y1 + a.y2) / 2, R = lv_area_get_width(&a) / 2 - 4;
    auto pt = [&](float deg, float r, lv_point_precise_t& p) {      // deg: rumbo verdadero, girado con lo que va arriba
        float t = (deg - c->up) * (float)M_PI / 180.0f;
        p.x = cx + r * sinf(t); p.y = cy - r * cosf(t);
    };
    lv_draw_arc_dsc_t ad;
    lv_draw_arc_dsc_init(&ad);
    ad.center.x = cx; ad.center.y = cy; ad.start_angle = 0; ad.end_angle = 360;
    ad.color = C(P->panelBorder); ad.width = 2; ad.radius = R; lv_draw_arc(layer, &ad);
    ad.radius = R * 2 / 3; ad.color = C(P->subBorder); lv_draw_arc(layer, &ad);
    ad.radius = R / 3; lv_draw_arc(layer, &ad);
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    for (int d = 0; d < 360; d += 10) {                             // marcas cada 10 grados; las de 30 mas largas
        bool major = d % 30 == 0;
        ld.color = C(major ? P->muted : P->faint); ld.width = major ? 3 : 1;
        pt((float)d, (float)R, ld.p1); pt((float)d, R - (major ? 16.0f : 8.0f), ld.p2);
        lv_draw_line(layer, &ld);
    }
    lv_draw_label_dsc_t td;
    lv_draw_label_dsc_init(&td);
    td.align = LV_TEXT_ALIGN_CENTER;
    static const char* card[] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; i++) {
        lv_point_precise_t p;
        pt(i * 90.0f, R - 36.0f, p);
        td.font = &app_bold_26; td.color = C(i == 0 ? P->red : P->text); td.text = card[i];
        lv_area_t la = {(int32_t)p.x - 20, (int32_t)p.y - 15, (int32_t)p.x + 20, (int32_t)p.y + 15};
        lv_draw_label(layer, &td, &la);
    }
    auto needle = [&](float deg, uint32_t color, float r0, float r1, int w) {
        ld.color = C(color); ld.width = w; ld.round_end = 1; ld.round_start = 1;
        pt(deg, r0, ld.p1); pt(deg, r1, ld.p2);
        lv_draw_line(layer, &ld);
        lv_draw_triangle_dsc_t tr;
        lv_draw_triangle_dsc_init(&tr);
        tr.color = C(color);
        pt(deg, r1 + 14, tr.p[0]); pt(deg - 6, r1 - 10, tr.p[1]); pt(deg + 6, r1 - 10, tr.p[2]);
        lv_draw_triangle(layer, &tr);
    };
    if (c->hasRotor) needle(c->rotor, P->cyan, 0, R * 0.70f, 5);
    if (c->hasTarget) needle(c->target, P->amber, R * 0.15f, R * 0.80f, 6);
    if (c->hasOwn) {                                                // flecha verde en el centro: rumbo propio
        lv_draw_triangle_dsc_t tr;
        lv_draw_triangle_dsc_init(&tr);
        tr.color = C(P->green);
        pt(c->own, 26, tr.p[0]); pt(c->own + 140, 18, tr.p[1]); pt(c->own - 140, 18, tr.p[2]);
        lv_draw_triangle(layer, &tr);
    } else {
        lv_draw_rect_dsc_t rd;
        lv_draw_rect_dsc_init(&rd);
        rd.bg_color = C(P->text); rd.radius = LV_RADIUS_CIRCLE;
        lv_area_t d = {cx - 6, cy - 6, cx + 6, cy + 6};
        lv_draw_rect(layer, &rd, &d);
    }
}
Compass* compass(lv_obj_t* p, int size) {
    Compass* c = new Compass();
    c->obj = box(p);
    lv_obj_set_size(c->obj, size, size);
    lv_obj_add_event_cb(c->obj, compassDraw, LV_EVENT_DRAW_MAIN, c);
    return c;
}
void Compass::set(float u, const float* t, const float* r, const float* o) {
    bool ch = u != up || (t != nullptr) != hasTarget || (r != nullptr) != hasRotor || (o != nullptr) != hasOwn ||
              (t && *t != target) || (r && *r != rotor) || (o && *o != own);
    up = u; hasTarget = t; hasRotor = r; hasOwn = o;
    if (t) target = *t;
    if (r) rotor = *r;
    if (o) own = *o;
    if (ch) lv_obj_invalidate(obj);
}

// ------------------------------------------------------------------ formato
std::string fmtAge(int s) {
    char b[24];
    if (s < 0) return "-";
    if (s < 60) snprintf(b, sizeof b, "%ds", s);
    else if (s < 3600) snprintf(b, sizeof b, "%d min", s / 60);
    else if (s < 86400) snprintf(b, sizeof b, "%d h", s / 3600);
    else snprintf(b, sizeof b, "%d d", s / 86400);
    return b;
}
std::string fmtKm(double km) {
    char b[24];
    if (km < 0) return "-";
    if (km < 10) snprintf(b, sizeof b, "%.1f km", km); else snprintf(b, sizeof b, "%.0f km", km);
    return b;
}
std::string fmtDeg(double d) { char b[16]; snprintf(b, sizeof b, "%03.0f\xC2\xB0", fmod(d + 360.0, 360.0)); return b; }
uint32_t tempColor(float c) { return c >= 30 ? P->red : c >= 24 ? P->amber : c <= 5 ? P->cyan : P->text; }

Freq freq(lv_obj_t* p, const lv_font_t* big, const lv_font_t* small) {
    Freq f;
    f.obj = row(p, 2);
    lv_obj_set_width(f.obj, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(f.obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    f.main = text(f.obj, "---.---", big, P->off);
    f.sub = text(f.obj, "", small, P->muted);
    lv_obj_set_style_pad_bottom(f.sub, big == &app_mono_64 ? 8 : 4, 0);
    return f;
}
void Freq::set(uint32_t hz, uint32_t color) {
    if (!hz) { setText(main, "---.---"); setColor(main, P->off); setText(sub, ""); return; }
    setText(main, proto::mhz(hz).c_str());
    setColor(main, dflt(color, P->text));
    setText(sub, proto::subKhz(hz).c_str());
}

// ------------------------------------------------------------------ pantallas y navegacion
static lv_obj_t *scr, *content, *keysBar, *statusBar;
static ScreenDef defs[SCR_COUNT];
static lv_obj_t* roots[SCR_COUNT];
struct Entry { Scr s; std::string arg; };
static std::vector<Entry> stack;
static bool forceRefresh = true;

void registerScreen(Scr s, const ScreenDef& d) { defs[s] = d; }
Scr top() { return stack.empty() ? RADIO : stack.back().s; }
const std::string& arg() { static std::string none; return stack.empty() ? none : stack.back().arg; }
void requestRefresh() { forceRefresh = true; }

static void layoutCurrent();
static void refreshStatus();
static void showTop(Scr prev) {
    Scr s = top();
    if (prev != s && prev < SCR_COUNT && roots[prev]) { show(roots[prev], false); if (defs[prev].leave) defs[prev].leave(); }
    if (!roots[s]) {
        lv_obj_t* r = lv_obj_create(content);
        baseObj(r);
        lv_obj_set_size(r, W - 2 * PAD, CONTENT_FULL);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(r, GAP, 0);
        lv_obj_set_style_pad_top(r, 4, 0);
        roots[s] = r;
        if (defs[s].build) defs[s].build(r);
    }
    show(roots[s], true);
    layoutCurrent();
    if (defs[s].enter) defs[s].enter();
    forceRefresh = true;
    if (statusBar && M) { refreshStatus(); refreshTouchKeys(); }
}
void go(Scr s, const std::string& a) {
    Scr prev = top();
    if (s == RADIO) { stack.clear(); stack.push_back({RADIO, ""}); showTop(prev); return; }
    if (!stack.empty() && stack.back().s == s && stack.back().arg == a) return;
    if (!stack.empty() && stack.back().s == FMENU) stack.pop_back();              // el menu [F] se cierra al usar una tecla
    for (size_t i = 0; i < stack.size(); i++) if (stack[i].s == s) { stack.erase(stack.begin() + i); break; }
    stack.push_back({s, a});
    while (stack.size() > 12) stack.erase(stack.begin());
    if (prev == s) { if (defs[s].leave) defs[s].leave(); if (defs[s].enter) defs[s].enter(); forceRefresh = true; return; }
    showTop(prev);
}
void back() {
    if (stack.size() <= 1) { go(RADIO); return; }
    Scr prev = top();
    stack.pop_back();
    showTop(prev);
}

// ------------------------------------------------------------------ teclado
static lv_obj_t* kb = nullptr;
static int kbH = 270;
static void layoutCurrent() {
    Scr s = top();
    const bool keys = defs[s].keys;
    const bool kbOn = kb && !lv_obj_is_hidden(kb);
    show(keysBar, keys && !kbOn);
    int h = kbOn ? H - STATUS_H - kbH : keys ? CONTENT_H : CONTENT_FULL;
    lv_obj_set_height(content, h);
    if (roots[s]) lv_obj_set_height(roots[s], h);
}
static void kbEvent(lv_event_t* e) {
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_READY || c == LV_EVENT_CANCEL) {
        lv_obj_t* ta = lv_keyboard_get_textarea(kb);
        if (ta) lv_obj_remove_state(ta, LV_STATE_FOCUSED);
        lv_keyboard_set_textarea(kb, nullptr);
        lv_obj_set_hidden(kb, true);
        layoutCurrent();
        if (ta && c == LV_EVENT_READY) lv_obj_send_event(ta, LV_EVENT_READY, nullptr);
    }
}
void keyboardFor(lv_obj_t* ta) {
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_hidden(kb, false);
    layoutCurrent();
    lv_obj_update_layout(scr);
    lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
}
static void fieldEvent(lv_event_t* e) {
    lv_event_code_t c = lv_event_get_code(e);
    lv_obj_t* ta = lv_event_get_target_obj(e);
    if (c == LV_EVENT_FOCUSED || c == LV_EVENT_CLICKED) keyboardFor(ta);
    if (c == LV_EVENT_VALUE_CHANGED && lv_obj_get_user_data(ta)) {     // en mayusculas (indicativos)
        const char* t = lv_textarea_get_text(ta);
        bool low = false;
        for (const char* p = t; *p; p++) if (*p >= 'a' && *p <= 'z') low = true;
        if (low) { std::string u = upper(t); uint32_t cur = lv_textarea_get_cursor_pos(ta); lv_textarea_set_text(ta, u.c_str()); lv_textarea_set_cursor_pos(ta, cur); }
    }
}
lv_obj_t* field(lv_obj_t* p, const char* placeholder, int w, int maxLen, const char* accepted, bool upperCase) {
    lv_obj_t* ta = lv_textarea_create(p);
    lv_obj_set_style_bg_color(ta, C(P->dark ? P->scrim : P->panel), 0);
    lv_obj_set_style_border_color(ta, C(P->keyBorder), 0);
    lv_obj_set_style_border_color(ta, C(P->green), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 10, 0);
    lv_obj_set_style_text_color(ta, C(P->text), 0);
    lv_obj_set_style_text_font(ta, &app_text_20, 0);
    lv_obj_set_style_pad_ver(ta, 12, 0);
    lv_obj_set_style_text_color(ta, C(P->faint), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_max_length(ta, maxLen);
    if (accepted) lv_textarea_set_accepted_chars(ta, accepted);
    if (w) lv_obj_set_width(ta, w);
    lv_obj_set_user_data(ta, (void*)(intptr_t)(upperCase ? 1 : 0));
    lv_obj_add_event_cb(ta, fieldEvent, LV_EVENT_ALL, nullptr);
    return ta;
}

// ------------------------------------------------------------------ avisos y confirmaciones
static lv_obj_t* banner = nullptr;
static uint32_t bannerAt = 0;
static void bannerClick(lv_event_t*) { show(banner, false); }
static void showBanner(const char* t, uint32_t bg) {
    setText(lv_obj_get_child(banner, 0), t);
    lv_obj_set_style_bg_color(banner, C(bg), 0);
    show(banner, true);
    lv_obj_move_foreground(banner);
    bannerAt = nowMs();
}
void notice(const char* t) { showBanner(t, P->cyan); }
void error(const char* t) { showBanner(t, P->redSoft); }

struct Confirm { lv_obj_t* back; void (*cb)(void*); void* ud; };
static Confirm cf = {nullptr, nullptr, nullptr};
static void cfClose() { if (cf.back) { lv_obj_delete(cf.back); cf.back = nullptr; } }
static void cfOk(lv_event_t*) { auto cb = cf.cb; void* ud = cf.ud; cfClose(); if (cb) cb(ud); }
static void cfNo(lv_event_t*) { cfClose(); }
void confirm(const char* title, const char* body, const char* ok, void (*cb)(void*), void* ud, bool danger) {
    cfClose();
    cf.cb = cb; cf.ud = ud;
    cf.back = lv_obj_create(lv_layer_top());
    baseObj(cf.back);
    lv_obj_set_size(cf.back, W, H);
    lv_obj_set_style_bg_color(cf.back, C(P->scrim), 0);
    lv_obj_set_style_bg_opa(cf.back, LV_OPA_70, 0);
    lv_obj_set_clickable(cf.back, true);
    lv_obj_t* p = panel(cf.back, 22);
    lv_obj_set_width(p, 560);
    lv_obj_center(p);
    lv_obj_set_style_pad_row(p, 14, 0);
    text(p, title, &app_bold_26, danger ? P->amber : P->text);
    wrap(p, body, &app_text_20, P->subText);
    lv_obj_t* r = row(p, 12);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    button(r, "CANCEL", cfNo, nullptr, 170, 56);
    button(r, ok, cfOk, nullptr, 200, 56, danger ? P->amber : P->green);
}

bool linked() { return trk::state() == trk::State::Linked; }
void send(const std::string& line) {
    if (!linked()) { error("The tracker is not linked (Settings > Tracker)"); return; }
    trk::send(line);
}

// ------------------------------------------------------------------ barra de estado
static lv_obj_t *stCall, *stBle, *stGps, *stBatt, *stClock, *stDisp, *stRot, *stWifi, *stWifiL, *stRotL;
static void onMenu(lv_event_t*) { openSideMenu(); }
static void onDisp(lv_event_t*) { if (top() == DISP) back(); else go(DISP); }
static void onLink(lv_event_t*) { go(SETTINGS); }
static void buildStatus() {
    statusBar = lv_obj_create(scr);
    baseObj(statusBar);
    lv_obj_set_size(statusBar, W, STATUS_H);
    lv_obj_set_flex_flow(statusBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(statusBar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(statusBar, 6, 0);
    lv_obj_set_style_pad_column(statusBar, 14, 0);
    lv_obj_t* m = lv_button_create(statusBar);
    lv_obj_remove_style_all(m);
    lv_obj_set_size(m, 52, 48);
    lv_obj_set_style_radius(m, 10, 0);
    lv_obj_set_style_bg_color(m, C(P->selected), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(m, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_t* ml = text(m, LV_SYMBOL_LIST, &R7_ICON_L, P->text);
    lv_obj_center(ml);
    lv_obj_add_event_cb(m, onMenu, LV_EVENT_CLICKED, nullptr);
    stCall = text(statusBar, "NOCALL", &app_bold_26, P->text);
    lv_obj_set_flex_grow(stCall, 1);
    lv_obj_set_width(stCall, 10);
    auto lampLabel = [](const char* t, lv_obj_t** l, lv_obj_t** lab, lv_event_cb_t cb) {
        lv_obj_t* r = box(statusBar);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(r, 6, 0);
        lv_obj_set_height(r, 44);
        *l = lamp(r, 11);
        lv_obj_t* tl = text(r, t, &app_cond_18, P->muted);
        if (lab) *lab = tl;
        if (cb) { lv_obj_set_clickable(r, true); lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, nullptr); }
    };
    lampLabel("BLE", &stBle, nullptr, onLink);
    lampLabel("GPS", &stGps, nullptr, nullptr);
    lampLabel("ROT", &stRot, &stRotL, nullptr);
    lampLabel("WiFi", &stWifi, &stWifiL, onLink);
    stBatt = text(statusBar, "", &app_semi_20, P->text);
    stClock = text(statusBar, "--:--Z", &app_semi_20, P->text);
    stDisp = lv_button_create(statusBar);
    lv_obj_remove_style_all(stDisp);
    lv_obj_set_size(stDisp, LV_SIZE_CONTENT, 40);
    lv_obj_set_style_pad_hor(stDisp, 12, 0);
    lv_obj_set_style_radius(stDisp, 8, 0);
    lv_obj_set_style_border_width(stDisp, 1, 0);
    lv_obj_set_style_border_color(stDisp, C(P->keyBorder), 0);
    lv_obj_t* dl = text(stDisp, "DISP", &app_bold_18, P->text);
    lv_obj_center(dl);
    lv_obj_add_event_cb(stDisp, onDisp, LV_EVENT_CLICKED, nullptr);
}
static void refreshStatus() {
    const bool lk = linked();
    std::string call = myCall();
    setText(stCall, call.empty() ? "NOCALL" : call.c_str());
    lampSet(stBle, lk || trk::state() == trk::State::Pairing, lk ? P->green : P->cyan);
    lampSet(stGps, M->gnss.ok && M->gnss.fix);
    net::Link rl = net::rotorState();
    const bool rotOn = cfg::get().rotorLink != 0;
    show(lv_obj_get_parent(stRot), rotOn);
    lampSet(stRot, rl == net::Link::Linked || rl == net::Link::Connecting, rl == net::Link::Linked ? P->green : P->cyan);
    net::Wifi w = net::wifi();
    show(lv_obj_get_parent(stWifi), w.configured);
    lampSet(stWifi, w.connected);
    char b[32] = "";
    if (lk && M->sensors.battPct >= 0) snprintf(b, sizeof b, "%d%%", M->sensors.battPct);
    else if (lk && M->status.batt > 0) snprintf(b, sizeof b, "%.2fV", M->status.batt);
    setText(stBatt, b);
    setColor(stBatt, M->sensors.battPct >= 0 && M->sensors.battPct < 20 ? P->amber : P->text);
    int y, mo, d, h, mi, s;
    if (utcNow(y, mo, d, h, mi, s)) snprintf(b, sizeof b, "%02d:%02dZ", h, mi); else snprintf(b, sizeof b, "--:--Z");
    setText(stClock, b);
    const bool dOn = top() == DISP;
    lv_obj_set_style_border_color(stDisp, C(dOn ? P->amber : P->keyBorder), 0);
    setColor(lv_obj_get_child(stDisp, 0), dOn ? P->amber : P->text);
}

// ------------------------------------------------------------------ teclas tactiles [F] + 5
static Key tk[5];
static lv_obj_t* fBtn;
static void onF(lv_event_t*) { if (top() == FMENU) back(); else go(FMENU); }
static void onTouchKey(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    const FKey* k = fkey(cfg::get().keys[i]);
    if (k && k->run) k->run();
}
static void buildKeys() {
    keysBar = lv_obj_create(scr);
    baseObj(keysBar);
    lv_obj_set_size(keysBar, W, KEYS_H);
    lv_obj_set_pos(keysBar, 0, H - KEYS_H);
    lv_obj_set_flex_flow(keysBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_hor(keysBar, PAD, 0);
    lv_obj_set_style_pad_top(keysBar, 6, 0);
    lv_obj_set_style_pad_bottom(keysBar, 10, 0);
    lv_obj_set_style_pad_column(keysBar, 8, 0);
    fBtn = lv_button_create(keysBar);
    lv_obj_remove_style_all(fBtn);
    lv_obj_set_size(fBtn, 80, KEYS_H - 16);
    lv_obj_set_style_radius(fBtn, 12, 0);
    lv_obj_set_style_bg_opa(fBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(fBtn, C(P->cyan), 0);
    lv_obj_t* fl = text(fBtn, "F", &app_bold_34, P->bg);
    lv_obj_center(fl);
    lv_obj_add_event_cb(fBtn, onF, LV_EVENT_CLICKED, nullptr);
    for (int i = 0; i < 5; i++) {
        tk[i] = key(keysBar, "", nullptr, 10, KEYS_H - 16, onTouchKey, (void*)(intptr_t)i, &app_bold_22);
        lv_obj_set_flex_grow(tk[i].obj, 1);
    }
}
void refreshTouchKeys() {
    lv_obj_set_style_bg_color(fBtn, C(top() == FMENU ? P->amber : P->cyan), 0);
    for (int i = 0; i < 5; i++) {
        const std::string& id = cfg::get().keys[i];
        const FKey* k = fkey(id);
        int lit = 0;
        std::string sub = fkeySub(id, lit);
        Scr s = top();
        bool active = (id == "SLIST" && s == SLIST) || (id == "MSG" && (s == MSGS || s == THREAD)) || (id == "MAP" && s == MAP) ||
                      (id == "DISP" && s == DISP) || (id == "DX" && s == DXC) || (id == "WX" && s == WX) || (id == "SAT" && s == SAT) ||
                      (id == "ROTOR" && s == ROTOR) || (id == "AIS" && s == AIS);
        const char* shown = (id == "BCN") ? "TX" : (id == "MSG" && lit == 2) ? sub.c_str() : nullptr;
        tk[i].set(k ? k->label : id.c_str(), shown);
        tk[i].style(active ? P->amber : P->text, active ? P->amber : P->keyBorder, lit == 2 ? P->amber : P->muted);
    }
}

// ------------------------------------------------------------------ menu lateral
static lv_obj_t* side = nullptr;
static void sideClose() { if (side) { lv_obj_delete(side); side = nullptr; } }
static void sideScrim(lv_event_t* e) { if (lv_event_get_target_obj(e) == side) sideClose(); }
static void sideGo(lv_event_t* e) { Scr s = (Scr)(intptr_t)lv_event_get_user_data(e); sideClose(); go(s); }
static void sideTheme(lv_event_t* e) {
    int t = (int)(intptr_t)lv_event_get_user_data(e);
    if (t == cfg::get().theme) return;
    cfg::get().theme = t;
    cfg::save();
    if (themeCb) themeCb(t);
}
static void sideItem(lv_obj_t* p, const char* label, const char* right, Scr s, bool active) {
    lv_obj_t* it = row(p, 8);
    lv_obj_set_height(it, 48);
    lv_obj_set_style_pad_hor(it, 12, 0);
    lv_obj_set_style_radius(it, 10, 0);
    lv_obj_set_style_bg_color(it, C(P->selected), 0);
    lv_obj_set_style_bg_opa(it, active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(it, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_clickable(it, true);
    lv_obj_add_event_cb(it, sideGo, LV_EVENT_CLICKED, (void*)(intptr_t)s);
    lv_obj_t* l = text(it, label, &app_text_20, active ? P->amber : P->text);
    lv_obj_set_flex_grow(l, 1);
    if (right && *right) text(it, right, &app_cond_18, P->muted);
}
void openSideMenu() {
    sideClose();
    side = lv_obj_create(lv_layer_top());
    baseObj(side);
    lv_obj_set_size(side, W, H);
    lv_obj_set_style_bg_color(side, C(P->scrim), 0);
    lv_obj_set_style_bg_opa(side, LV_OPA_70, 0);
    lv_obj_set_clickable(side, true);
    lv_obj_add_event_cb(side, sideScrim, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* p = lv_obj_create(side);
    baseObj(p);
    lv_obj_set_size(p, 380, H);
    lv_obj_set_style_bg_color(p, C(P->bg), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(p, 16, 0);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(p, 2, 0);
    lv_obj_set_scrollable(p, true);
    lv_obj_set_clickable(p, true);
    lv_obj_t* h = row(p, 12);
    lv_obj_t* ic = lv_image_create(h);
    lv_image_set_src(ic, &img_app_icon);
    lv_obj_t* hc = col(h, 0);
    lv_obj_set_width(hc, LV_SIZE_CONTENT);
    lv_obj_t* t = text(hc, "APRS LoRa+", &app_bold_26, P->text);
    (void)t;
#ifdef R7_SCALED
    text(hc, "Display " DISPLAY_VERSION " \xC2\xB7 LCD-7 v2", &app_text_18, P->muted);
#else
    text(hc, "Display " DISPLAY_VERSION " \xC2\xB7 7B v2", &app_text_18, P->muted);
#endif
    const Scr s = top();
    char b[24];
    lv_obj_set_style_margin_top(section(p, "OPERATE"), 12, 0);
    sideItem(p, "Radio", "", RADIO, s == RADIO);
    sideItem(p, "Map (radar)", "", MAP, s == MAP);
    snprintf(b, sizeof b, "%d", (int)M->heard.size());
    sideItem(p, "Stations", b, SLIST, s == SLIST);
    int wx = 0; for (auto& x : M->heard) if (x.wx.ok) wx++;
    snprintf(b, sizeof b, "%d", wx);
    sideItem(p, "Weather stations", b, WX, s == WX);
    snprintf(b, sizeof b, "%d", M->msgUnread);
    sideItem(p, "Messages", M->msgUnread ? b : "", MSGS, s == MSGS);
    sideItem(p, "DAPNET pager", "", PAGER, s == PAGER);
    snprintf(b, sizeof b, "%d", (int)DX.spots.size());
    sideItem(p, "DX cluster", net::dxState() == net::Link::Linked ? b : "", DXC, s == DXC);
    lv_obj_set_style_margin_top(section(p, "DEVICES"), 12, 0);
    sideItem(p, "Tracker", M->info.ok ? M->info.board.c_str() : trk::stateText(), TRACKER, s == TRACKER);
    sideItem(p, "Rotor box", cfg::get().rotorLink ? net::rotorStateText() : "off", ROTOR, s == ROTOR);
    sideItem(p, "Settings", "", SETTINGS, s == SETTINGS);
    lv_obj_set_style_margin_top(section(p, "RADIO TOOLS"), 12, 0);
    sideItem(p, "Satellites", M->sat.ok ? M->sat.name.c_str() : "", SAT, s == SAT);
    sideItem(p, "Sensors", "", SENSORS, s == SENSORS);
    sideItem(p, "AIS ships", "", AIS, s == AIS);
    sideItem(p, "Remote screen", "", REMOTE, s == REMOTE);
    sideItem(p, "Tracker configuration", "", CONFIG, s == CONFIG);
    sideItem(p, "Firmware", M->info.ok ? M->info.fw.c_str() : "", FIRMWARE, s == FIRMWARE);
    sideItem(p, "About", "", ABOUT, s == ABOUT);
    lv_obj_set_style_margin_top(section(p, "THEME"), 12, 0);
    lv_obj_t* tr = row(p, 8);
    chip(tr, "DARK", cfg::get().theme == 0, sideTheme, (void*)(intptr_t)0);
    chip(tr, "LIGHT", cfg::get().theme == 1, sideTheme, (void*)(intptr_t)1);
    wrap(p, "The display restarts to apply it.", &app_text_18, P->muted);
}

// ------------------------------------------------------------------ alerta de emergencia (AlertOverlay de la app)
static lv_obj_t* alert = nullptr;
static int alertEq = -1;
static int alertClosedEq = -2;
static bool alertEai = false;
static void alertClose(lv_event_t*) { if (alert) { lv_obj_delete(alert); alert = nullptr; } alertClosedEq = M->leds.eq; }
static void alertSilence(lv_event_t*) { trk::send(proto::Cmd("alarm").text("s", "off").str()); }
static void alertStop(lv_event_t*) { trk::send(proto::Cmd("eai").text("s", "stop").str()); }
static void alertOk(lv_event_t*) { trk::send(proto::Cmd("eai").text("s", "ok").str()); }
static void alertStation(lv_event_t*) {
    std::string c = M->leds.ec;
    if (alert) { lv_obj_delete(alert); alert = nullptr; }
    alertClosedEq = M->leds.eq;
    go(STATION, c);
}
static void buildAlert(bool eai) {
    if (alert) lv_obj_delete(alert);
    alertEai = eai;
    alert = lv_obj_create(lv_layer_top());
    baseObj(alert);
    lv_obj_set_size(alert, W, H);
    lv_obj_set_style_bg_color(alert, C(P->scrim), 0);
    lv_obj_set_style_bg_opa(alert, LV_OPA_80, 0);
    lv_obj_set_clickable(alert, true);
    lv_obj_t* p = panel(alert, 24, P->panel, P->red);
    lv_obj_set_style_border_width(p, 3, 0);
    lv_obj_set_width(p, 640);
    lv_obj_center(p);
    lv_obj_set_style_pad_row(p, 12, 0);
    text(p, "", &app_bold_34, P->red);
    text(p, "", &app_bold_26, P->text);
    wrap(p, "", &app_text_20, P->subText);
    lv_obj_t* r = row(p, 10);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (eai) {
        button(r, "I'M OK", alertOk, nullptr, 150, 60, P->green);
        button(r, "STOP EAI", alertStop, nullptr, 170, 60, P->amber);
    } else {
        button(r, "STATION", alertStation, nullptr, 150, 60, P->cyan);
        button(r, "SILENCE", alertSilence, nullptr, 150, 60, P->amber);
    }
    button(r, "CLOSE", alertClose, nullptr, 140, 60);
}
static void refreshAlert() {
    const proto::Leds& l = M->leds;
    const proto::Eai& a = M->eai;
    const bool eaiOn = a.ok && (a.sos > 0 || a.fm || a.md);
    if (eaiOn) {
        if (!alert || !alertEai) buildAlert(true);
        lv_obj_t* p = lv_obj_get_child(alert, 0);
        setText(lv_obj_get_child(p, 0), a.md ? "MAN DOWN" : "EMERGENCY (EAI)");
        char b[96];
        if (a.sos > 0) snprintf(b, sizeof b, "SOS beacon %s \xC2\xB7 %d sent", a.sos == 1 ? "countdown" : "on air", a.sent);
        else if (a.fm) snprintf(b, sizeof b, "Voice EAI \xC2\xB7 %d s left", a.fmLeft);
        else snprintf(b, sizeof b, "No movement: EAI in %d s", a.mdLeft);
        setText(lv_obj_get_child(p, 1), b);
        setText(lv_obj_get_child(p, 2), a.by.empty() ? "Touch I'M OK if you are fine, or STOP EAI to end it." : ("Started by " + a.by).c_str());
        return;
    }
    if (alert && alertEai) { lv_obj_delete(alert); alert = nullptr; }
    if (l.ok && l.emg && l.eq != alertClosedEq) {
        if (!alert || alertEq != l.eq) { buildAlert(false); alertEq = l.eq; }
        lv_obj_t* p = lv_obj_get_child(alert, 0);
        const char* kind = l.ek == "mob" ? "MAN OVERBOARD" : l.ek == "ais_sos" ? "AIS DISTRESS" : l.ek == "ais_cpa" ? "COLLISION RISK" : "EMERGENCY";
        setText(lv_obj_get_child(p, 0), kind);
        setText(lv_obj_get_child(p, 1), l.ec.empty() ? "-" : l.ec.c_str());
        setText(lv_obj_get_child(p, 2), (l.et + (l.em ? "  (message)" : "  (beacon)")).c_str());
    }
}

// ------------------------------------------------------------------ arranque y bucle
static uint32_t drawnRev = 0xFFFFFFFF, drawnRotRev = 0xFFFFFFFF, drawnDx = 0xFFFFFFFF;
static uint32_t lastTick = 0;

static void showSplash(lv_obj_t* mainScr) {
    lv_obj_t* sp = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(sp, lv_color_hex(SPLASH_BG_RGB), 0);
    lv_obj_set_style_bg_opa(sp, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(sp, false);
    lv_obj_t* img = lv_image_create(sp);
    lv_image_set_src(img, &img_splash);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, -18);
    lv_obj_t* ver = lv_label_create(sp);
    lv_label_set_text(ver, "Display " DISPLAY_VERSION);
    lv_obj_set_style_text_font(ver, &app_text_20, 0);
    lv_obj_set_style_text_color(ver, lv_color_hex(0x28B5E1), 0);
    (lv_obj_align)(ver, LV_ALIGN_CENTER, 0, px(-18) + img_splash.header.h / 2 + px(18));
    lv_screen_load(sp);
    lv_obj_fade_in(img, 700, 0);
    lv_obj_fade_in(ver, 700, 300);
    lv_screen_load_anim(mainScr, LV_SCREEN_LOAD_ANIM_FADE_IN, 500, 2200, true);
}

void beginRadio(proto::Model* model, void (*onTheme)(int)) {
    M = model;
    themeCb = onTheme;
    P = cfg::get().theme == 1 ? &LIGHT : &DARK;
    lv_display_t* d = lv_display_get_default();
    lv_display_set_theme(d, lv_theme_default_init(d, C(P->cyan), C(P->red), P->dark, &app_text_20));
    lv_obj_set_style_text_font(lv_layer_top(), &app_text_20, 0);

    scr = lv_obj_create(nullptr);
    baseObj(scr);
    lv_obj_set_style_bg_color(scr, C(P->bg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, C(P->text), 0);
    lv_obj_set_style_text_font(scr, &app_text_20, 0);
    buildStatus();
    content = lv_obj_create(scr);
    baseObj(content);
    lv_obj_set_pos(content, PAD, STATUS_H);
    lv_obj_set_size(content, W - 2 * PAD, CONTENT_H);
    buildKeys();

    kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(kb, W, kbH);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb, C(P->panel), 0);
    lv_obj_set_style_bg_color(kb, C(P->key), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, C(P->text), LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, C(P->keyBorder), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, &R7_ICON_M, LV_PART_ITEMS);
    lv_obj_add_event_cb(kb, kbEvent, LV_EVENT_ALL, nullptr);
    lv_obj_set_hidden(kb, true);

    banner = lv_obj_create(lv_layer_top());
    baseObj(banner);
    lv_obj_set_size(banner, W - 2 * PAD, LV_SIZE_CONTENT);
    lv_obj_set_pos(banner, PAD, STATUS_H);
    lv_obj_set_style_pad_all(banner, 12, 0);
    lv_obj_set_style_radius(banner, 10, 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    lv_obj_t* bl = text(banner, "", &app_semi_20, P->bg);
    lv_obj_set_width(bl, LV_PCT(100));
    lv_label_set_long_mode(bl, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_clickable(banner, true);
    lv_obj_add_event_cb(banner, bannerClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_hidden(banner, true);

    initRadio(); initStations(); initMessages(); initTracker(); initSat(); initRotor(); initTools(); initDx();
    stack.clear();
    stack.push_back({RADIO, ""});
    showTop(SCR_COUNT);
    refreshStatus();
    refreshTouchKeys();
    showSplash(scr);
}

void updateRadio() {
    if (!M) return;
    const uint32_t t = nowMs();
    pumpLinks();
    rotorFollowStep();
    catDopplerStep();
    fastStep();
    if (banner && !lv_obj_is_hidden(banner) && t - bannerAt > 7000) show(banner, false);
    // un error del tracker se ensena una vez
    static uint32_t errSeen = 0;
    if (M->errSeq != errSeen) { errSeen = M->errSeq; if (!M->lastError.empty()) error(M->lastError.c_str()); }
    static uint32_t rotErr = 0;
    if (RM.errSeq != rotErr) { rotErr = RM.errSeq; if (!RM.lastError.empty()) error(("Rotor box: " + RM.lastError).c_str()); }
    const bool tick = t - lastTick >= 1000;
    if (tick) { lastTick = t; refreshStatus(); refreshTouchKeys(); tick1s(); }
    const bool changed = M->revision != drawnRev || RM.revision != drawnRotRev || DX.seq != drawnDx;
    if (changed || forceRefresh || tick) {
        drawnRev = M->revision; drawnRotRev = RM.revision; drawnDx = DX.seq;
        forceRefresh = false;
        refreshAlert();
        Scr s = top();
        if (defs[s].refresh) defs[s].refresh();
        if (changed) { refreshStatus(); refreshTouchKeys(); }
    }
}

bool visible(Scr s) { return top() == s; }

} // namespace r7

// --------------------------------------------------------------------------- ui.h
namespace ui {
void begin(proto::Model* model, int theme, void (*onThemeChange)(int)) { (void)theme; r7::beginRadio(model, onThemeChange); }
void update() { r7::updateRadio(); }
bool mirrorVisible() { return r7::top() == r7::REMOTE; }
bool heardVisible() { return r7::top() == r7::SLIST || r7::top() == r7::RADIO || r7::top() == r7::MAP || r7::top() == r7::WX; }
bool messagesVisible() { return r7::top() == r7::MSGS || r7::top() == r7::THREAD; }
}
#endif
