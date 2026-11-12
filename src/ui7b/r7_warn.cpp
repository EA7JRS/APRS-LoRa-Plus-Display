/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Avisos meteorologicos oficiales (plan del plotter: Meteo > Avisos y su ventana): lista de los que tocan a la zona, ventana
 * emergente cuando entra uno nuevo del nivel elegido o mas (franja amarilla, naranja o roja, como la de las emergencias) y
 * su ajuste (origen, clave de AEMET o pais y zona de Meteoalarm, nivel desde el que salta la ventana).
 */
#ifdef UI_RADIO
#include "r7.h"
#include "net7.h"
#include "settings.h"
#include "wxwarn.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <set>

namespace r7 {

static net7::Warnings W_;                    // la ultima copia de lo que tiene la red
static uint32_t wSeq = 0xFFFFFFFF;
static std::set<std::string> wShown;        // avisos cuya ventana ya salio (o se cerro)
static std::vector<int> wQueue;             // indices de W_.list que esperan su ventana
static lv_obj_t *pop = nullptr, *pHead, *pTitle, *pSub, *pName, *pBody, *pNext;
static int popIdx = -1;

static std::string keyOf(const wxw::Warning& w) { return w.event + "|" + w.area + "|" + w.onset; }
int warnCount() { return (int)W_.list.size(); }

static std::string span(const wxw::Warning& w) {
    std::string s = w.onset.empty() ? "now" : wxw::shortTime(w.onset);
    if (!w.expires.empty()) s += " to " + wxw::shortTime(w.expires);
    return s;
}
static std::string where(const wxw::Warning& w) {
    if (!w.hasArea) return w.area;
    if (w.inside) return w.area + " \xC2\xB7 you are inside";
    return w.area + " \xC2\xB7 " + fmtKm(w.distKm) + " away";
}

// ------------------------------------------------------------------ ventana emergente
static void popDrop() { if (pop) { lv_obj_delete(pop); pop = nullptr; } popIdx = -1; }
static void popShow(int idx);
static void popClose(lv_event_t*) {
    popDrop();
    if (!wQueue.empty()) { int i = wQueue.front(); wQueue.erase(wQueue.begin()); popShow(i); }
}
static void popAll(lv_event_t*) { popDrop(); wQueue.clear(); go(WXWARN); }
static void popShow(int idx) {
    if (idx < 0 || idx >= (int)W_.list.size()) return;
    const wxw::Warning& w = W_.list[idx];
    wShown.insert(keyOf(w));
    popDrop();
    popIdx = idx;
    pop = lv_obj_create(lv_layer_top());
    baseObj(pop);
    lv_obj_set_size(pop, W, H);
    lv_obj_set_style_bg_color(pop, C(P->scrim), 0);
    lv_obj_set_style_bg_opa(pop, LV_OPA_80, 0);
    lv_obj_set_clickable(pop, true);
    lv_obj_t* p = panel(pop, 0, P->panel, P->panel);
    lv_obj_set_width(p, 720);
    lv_obj_center(p);
    lv_obj_set_style_pad_row(p, 0, 0);
    lv_obj_set_style_radius(p, 18, 0);
    lv_obj_set_style_clip_corner(p, true, 0);
    pHead = col(p, 2);
    lv_obj_set_width(pHead, LV_PCT(100));
    lv_obj_set_style_bg_color(pHead, C(wxw::levelColor(w.level)), 0);
    lv_obj_set_style_bg_opa(pHead, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(pHead, 22, 0);
    lv_obj_set_style_pad_ver(pHead, 12, 0);
    pTitle = text(pHead, "WEATHER WARNING", &app_bold_34, 0x131A22);
    lv_obj_set_style_text_letter_space(pTitle, 1, 0);
    char b[96];
    snprintf(b, sizeof b, "%s \xC2\xB7 %s level%s%s", w.source.c_str(), wxw::levelName(w.level), w.phen.empty() ? "" : " \xC2\xB7 ", w.phen.c_str());
    pSub = text(pHead, b, &app_text_18, 0x131A22);
    lv_obj_t* body = col(p, 8);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_style_pad_hor(body, 22, 0);
    lv_obj_set_style_pad_top(body, 14, 0);
    lv_obj_set_style_pad_bottom(body, 18, 0);
    pName = wrap(body, w.event.c_str(), &app_bold_26, P->text);
    std::string t = "Area:  " + where(w) + "\nIn force:  " + span(w);
    if (!w.prob.empty()) t += "  \xC2\xB7  probability " + w.prob;
    if (!w.desc.empty()) t += "\n" + w.desc;
    if (!w.instr.empty()) t += "\n" + w.instr;
    if (t.size() > 420) { t.resize(417); t += "..."; }
    pBody = wrap(body, t.c_str(), &app_text_20, P->subText);
    lv_obj_t* r = row(body, 10);
    lv_obj_set_style_margin_top(r, 8, 0);
    lv_obj_t* k = button(r, "ALL WARNINGS", popAll, nullptr, 0, 64, P->cyan);
    lv_obj_set_flex_grow(k, 1);
    snprintf(b, sizeof b, wQueue.empty() ? "CLOSE" : "CLOSE (%d more)", (int)wQueue.size());
    pNext = button(r, b, popClose, nullptr, 0, 64);
    lv_obj_set_flex_grow(pNext, 1);
}

void warnStep() {
    static uint32_t last = 0;
    if (nowMs() - last < 1000 && last) return;
    last = nowMs() | 1;
    const uint32_t seq = net7::warnings().seq;
    if (seq != wSeq) {
        W_ = net7::warnings();
        wSeq = W_.seq;
        popDrop();
        wQueue.clear();
        const int minLevel = cfg::get().warnPopup;
        for (size_t i = 0; i < W_.list.size(); i++) {
            const wxw::Warning& w = W_.list[i];
            const bool here = !w.hasArea || w.inside;          // los de zonas cercanas se ven en la lista, sin ventana
            if (minLevel > 0 && w.level >= minLevel && here && !wShown.count(keyOf(w))) wQueue.push_back((int)i);
        }
        requestRefresh();
    }
    if (pop && alertOpen()) { if (popIdx >= 0) wQueue.insert(wQueue.begin(), popIdx); wShown.erase(keyOf(W_.list[popIdx])); popDrop(); }   // la emergencia manda
    if (!pop && !alertOpen() && !wQueue.empty()) { int i = wQueue.front(); wQueue.erase(wQueue.begin()); popShow(i); }
}

// ------------------------------------------------------------------ pantalla
static List wl;
static lv_obj_t *wEmpty, *wStatus, *wSetup, *wListBox, *wSrcC[3], *wPopC[4], *wKey, *wCountry, *wArea, *wKeyRow, *wMaRow, *wHelp;
static bool wSetupOpen = false;

static lv_obj_t* makeRow(lv_obj_t* p) {
    lv_obj_t* r = row(p, 14);
    lv_obj_set_height(r, 78);
    lv_obj_set_style_pad_hor(r, 12, 0);
    lv_obj_set_style_radius(r, 10, 0);
    lv_obj_set_style_bg_color(r, C(P->panel), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_t* bar = box(r);
    lv_obj_set_size(bar, 10, 54);
    lv_obj_set_style_radius(bar, 5, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_t* c = col(r, 0);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_width(c, 10);
    oneLine(text(c, "", &app_bold_22, P->text));
    oneLine(text(c, "", &app_text_18, P->subText));
    lv_obj_t* t = col(r, 0);
    lv_obj_set_width(t, 250);
    lv_obj_set_flex_align(t, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    text(t, "", &app_bold_18, P->text);
    text(t, "", &app_cond_18, P->muted);
    return r;
}
static void onRow(lv_event_t* e) { int i = clickIndex(e); if (i >= 0 && i < (int)W_.list.size()) { wQueue.clear(); popShow(i); } }
static void onRefresh(lv_event_t*) { net7::warnFetch(); notice("Asking for the warnings..."); }
static void onSetup(lv_event_t*) { wSetupOpen = !wSetupOpen; requestRefresh(); }
static void onSrc(lv_event_t* e) { cfg::get().warnSrc = (int)(intptr_t)lv_event_get_user_data(e); cfg::save(); net7::warnFetch(); requestRefresh(); }
static void onPop(lv_event_t* e) { cfg::get().warnPopup = (int)(intptr_t)lv_event_get_user_data(e); cfg::save(); requestRefresh(); }
static void onSave(lv_event_t*) {
    cfg::Settings& st = cfg::get();
    std::string k = lv_textarea_get_text(wKey);
    if (!k.empty()) { st.aemetKey = k; lv_textarea_set_text(wKey, ""); }
    std::string c = lv_textarea_get_text(wCountry);
    if (!c.empty()) st.maCountry = c;
    st.maArea = lv_textarea_get_text(wArea);
    cfg::save();
    net7::warnFetch();
    notice("Saved");
    requestRefresh();
}
static void buildWarn(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Weather warnings", &tr);
    chip(tr, "REFRESH", false, onRefresh);
    chip(tr, "SETUP", false, onSetup, nullptr, 40, P->amber);
    lv_obj_t* sa = scrollArea(root);
    wStatus = wrap(sa, "", &app_text_18, P->muted);
    // ajuste
    wSetup = panel(sa, 14);
    lv_obj_set_width(wSetup, LV_PCT(100));
    lv_obj_set_style_pad_row(wSetup, 10, 0);
    section(wSetup, "SOURCE");
    lv_obj_t* r = row(wSetup, 8);
    static const char* sn[] = {"OFF", "AEMET (SPAIN)", "METEOALARM (EUROPE)"};
    for (int i = 0; i < 3; i++) wSrcC[i] = chip(r, sn[i], false, onSrc, (void*)(intptr_t)i);
    wHelp = wrap(wSetup, "", &app_text_18, P->muted);
    wKeyRow = row(wSetup, 8);
    wKey = field(wKeyRow, "AEMET OpenData key (empty = keep)", 0, 400);
    lv_obj_set_flex_grow(wKey, 1);
    wMaRow = row(wSetup, 8);
    wCountry = field(wMaRow, "Country (spain, portugal, france...)", 300, 24);
    wArea = field(wMaRow, "Area names, with commas (Sevilla, Estrecho)", 0, 80);
    lv_obj_set_flex_grow(wArea, 1);
    section(wSetup, "POP-UP WINDOW FROM");
    r = row(wSetup, 8);
    static const char* pn[] = {"NEVER", "YELLOW", "ORANGE", "RED"};
    static const uint32_t pc[] = {0xFFFFFFFF, 0xFFD84D, 0xFF9A3C, 0xFF5A5A};
    for (int i = 0; i < 4; i++) wPopC[i] = chip(r, pn[i], false, onPop, (void*)(intptr_t)i, 40, pc[i]);
    button(r, "SAVE", onSave, nullptr, 140, 48, P->green);
    wEmpty = wrap(sa, "", &app_text_20, P->muted);
    wListBox = col(sa, 8);
    lv_obj_set_width(wListBox, LV_PCT(100));
    wl.create(wListBox, makeRow, onRow);
}
static void refreshWarn() {
    const cfg::Settings& st = cfg::get();
    W_ = net7::warnings();
    setText(wStatus, W_.status.empty() ? (st.warnSrc ? "Waiting for the first download (needs WiFi)." : "Off: choose a source in SETUP.") : W_.status.c_str());
    show(wSetup, wSetupOpen || !st.warnSrc);
    for (int i = 0; i < 3; i++) chipSet(wSrcC[i], st.warnSrc == i);
    static const uint32_t pc[] = {0xFFFFFFFF, 0xFFD84D, 0xFF9A3C, 0xFF5A5A};
    for (int i = 0; i < 4; i++) chipSet(wPopC[i], st.warnPopup == i, pc[i]);
    show(wKeyRow, st.warnSrc == 1);
    show(wMaRow, st.warnSrc != 0);
    show(wCountry, st.warnSrc == 2);
    lv_textarea_set_placeholder_text(wKey, st.aemetKey.empty() ? "AEMET OpenData key (free, opendata.aemet.es)" : "AEMET key saved (empty = keep)");
    if (!lv_obj_has_state(wArea, LV_STATE_FOCUSED) && strlen(lv_textarea_get_text(wArea)) == 0 && !st.maArea.empty()) lv_textarea_set_text(wArea, st.maArea.c_str());
    if (!lv_obj_has_state(wCountry, LV_STATE_FOCUSED) && strlen(lv_textarea_get_text(wCountry)) == 0) lv_textarea_set_text(wCountry, st.maCountry.c_str());
    setText(wHelp, st.warnSrc == 1 ? "Official AEMET warnings for the zone you are in (and those within 30 km), from the tracker's position. Needs your own free key. "
                                     "Area names are only used while there is no position."
                 : st.warnSrc == 2 ? "Meteoalarm, the European weather services' warnings. It has no zone outlines: write the names of your zones as Meteoalarm calls them."
                 : "Official weather warnings (yellow, orange, red) with a pop-up window when a new one comes in for your area.");
    show(wEmpty, W_.list.empty() && st.warnSrc != 0);
    setText(wEmpty, "No warnings in force for your area.");
    wl.begin();
    for (const wxw::Warning& w : W_.list) {
        lv_obj_t* r = wl.next();
        lv_obj_set_style_bg_color(lv_obj_get_child(r, 0), C(wxw::levelColor(w.level)), 0);
        lv_obj_t* c = lv_obj_get_child(r, 1);
        setText(lv_obj_get_child(c, 0), w.event.c_str());
        setText(lv_obj_get_child(c, 1), where(w).c_str());
        lv_obj_t* t = lv_obj_get_child(r, 2);
        setText(lv_obj_get_child(t, 0), span(w).c_str());
        char b[48];
        snprintf(b, sizeof b, "%s%s%s", wxw::levelName(w.level), w.prob.empty() ? "" : " \xC2\xB7 ", w.prob.c_str());
        setText(lv_obj_get_child(t, 1), b);
        setColor(lv_obj_get_child(t, 1), wxw::levelColor(w.level));
    }
    wl.end();
}

void initWarn() {
    registerScreen(WXWARN, {"Weather warnings", false, buildWarn, refreshWarn, nullptr, nullptr});
}

} // namespace r7
#endif
