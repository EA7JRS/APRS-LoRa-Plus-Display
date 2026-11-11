/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Estaciones que oye el tracker (StationScreens.kt y WxScreen.kt de la app): S.LIST con orden y filtros, detalle con brujula,
 * meteorologia, mensaje, QSY y rotor; mapa de radar alrededor de la posicion propia (sin teselas) y estaciones meteorologicas.
 */
#ifdef UI_RADIO
#include "r7.h"
#include "net.h"
#include "settings.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace r7 {

static int extraAge() { return (int)((nowMs() - M->heardAtMs) / 1000); }
static const proto::Heard* findCall(const std::string& c) { for (const auto& h : M->heard) if (h.call == c) return &h; return nullptr; }

// ------------------------------------------------------------------ S.LIST
static int sSort = 0, sFilter = 0;          // orden: 0 reciente, 1 cercania, 2 nombre; filtro: 0 todas, 1 con posicion, 2 moviles, 3 meteo, 4 digis
static lv_obj_t *sSortC[3], *sFiltC[5], *sCount, *sEmpty;
static List sList;
static std::vector<int> sView;

static void onSort(lv_event_t* e) { sSort = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static void onFilt(lv_event_t* e) { sFilter = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static lv_obj_t* makeStRow(lv_obj_t* p) {
    lv_obj_t* r = row(p, 12);
    lv_obj_set_height(r, 62);
    lv_obj_set_style_pad_hor(r, 10, 0);
    lv_obj_set_style_radius(r, 10, 0);
    lv_obj_set_style_bg_color(r, C(P->panel), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, C(P->selected), LV_STATE_PRESSED);
    symbol(r);
    lv_obj_t* c = col(r, 0);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_width(c, 10);
    lv_obj_t* call = text(c, "", &app_bold_22, P->text);
    (void)call;
    lv_obj_t* sub = text(c, "", &app_text_18, P->muted);
    oneLine(sub);
    lv_obj_set_width(sub, LV_PCT(100));
    lv_obj_t* sig = text(r, "", &app_cond_18, P->subText);
    lv_obj_set_width(sig, 120);
    lv_obj_set_style_text_align(sig, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t* age = text(r, "", &app_cond_18, P->muted);
    lv_obj_set_width(age, 70);
    lv_obj_set_style_text_align(age, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t* d = text(r, "", &app_mono_20, P->text);
    lv_obj_set_width(d, 110);
    lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t* b = text(r, "", &app_mono_20, P->cyan);
    lv_obj_set_width(b, 56);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_RIGHT, 0);
    return r;
}
static void onStClick(lv_event_t* e) { int i = clickIndex(e); if (i >= 0 && i < (int)sView.size()) go(STATION, M->heard[sView[i]].call); }

static std::string stSub(const proto::Heard& h) {
    std::string s;
    if (h.wx.ok) {
        char b[80];
        int k = 0;
        if (h.wx.t > -900) k += snprintf(b + k, sizeof b - k, "%.0f\xC2\xB0" "C ", h.wx.t);
        if (h.wx.ws > -900) k += snprintf(b + k, sizeof b - k, "wind %.0f km/h ", h.wx.ws);
        if (h.wx.p > -900) snprintf(b + k, sizeof b - k, "%.0f hPa", h.wx.p);
        s = b;
    } else if (!h.cmt.empty()) s = h.cmt;
    else if (!h.sts.empty()) s = h.sts;
    else s = symbolName(h.symTable(), h.symCode());
    if (!h.kind.empty() && h.kind != "?") s = h.kind + " \xC2\xB7 " + s;
    return s;
}

static void buildSlist(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Stations", &tr);
    sCount = text(tr, "", &app_cond_18, P->muted);
    lv_obj_t* cr = row(root, 8);
    static const char* so[] = {"RECENT", "NEAREST", "NAME"};
    for (int i = 0; i < 3; i++) sSortC[i] = chip(cr, so[i], i == sSort, onSort, (void*)(intptr_t)i);
    lv_obj_t* sp = box(cr);
    lv_obj_set_flex_grow(sp, 1);
    static const char* fi[] = {"ALL", "POSITION", "MOBILE", "WX", "DIGI / iGATE"};
    for (int i = 0; i < 5; i++) sFiltC[i] = chip(cr, fi[i], i == sFilter, onFilt, (void*)(intptr_t)i, 40, P->amber);
    lv_obj_t* sa = scrollArea(root);
    sEmpty = wrap(sa, "", &app_text_20, P->muted);
    sList.create(sa, makeStRow, onStClick);
}
static void refreshSlist() {
    for (int i = 0; i < 3; i++) chipSet(sSortC[i], i == sSort);
    for (int i = 0; i < 5; i++) chipSet(sFiltC[i], i == sFilter, P->amber);
    double la, lo;
    const bool me = ownPos(la, lo);
    sView.clear();
    for (size_t i = 0; i < M->heard.size(); i++) {
        const proto::Heard& h = M->heard[i];
        bool ok = sFilter == 0 || (sFilter == 1 && h.hasPos) || (sFilter == 2 && (h.kind == "MOBILE" || h.spd > 0)) || (sFilter == 3 && h.wx.ok) ||
                  (sFilter == 4 && (h.kind == "DIGI" || h.kind == "IGATE"));
        if (ok) sView.push_back((int)i);
    }
    if (sSort == 1 && me)
        std::stable_sort(sView.begin(), sView.end(), [&](int a, int b) {
            const auto &x = M->heard[a], &y = M->heard[b];
            double dx = x.hasPos ? proto::distanceKm(la, lo, x.lat, x.lon) : 1e9, dy = y.hasPos ? proto::distanceKm(la, lo, y.lat, y.lon) : 1e9;
            return dx < dy;
        });
    if (sSort == 2) std::stable_sort(sView.begin(), sView.end(), [](int a, int b) { return M->heard[a].call < M->heard[b].call; });
    setTextf(sCount, "%d shown \xC2\xB7 %d heard \xC2\xB7 %lu packets", (int)sView.size(), M->heardTot, (unsigned long)M->heardPkts);
    show(sEmpty, sView.empty());
    setText(sEmpty, !linked() ? "Link the tracker (Settings) to see the stations it hears." : M->heard.empty() ? "Nothing heard yet." : "No station passes this filter.");
    sList.begin();
    const int ex = extraAge();
    char b[64];
    for (int i : sView) {
        const proto::Heard& h = M->heard[i];
        lv_obj_t* r = sList.next();
        symbolSet(lv_obj_get_child(r, 0), h.symTable(), h.symCode());
        lv_obj_t* c = lv_obj_get_child(r, 1);
        setText(lv_obj_get_child(c, 0), h.call.c_str());
        setText(lv_obj_get_child(c, 1), stSub(h).c_str());
        if (h.rssi) snprintf(b, sizeof b, "%d dBm", h.rssi); else b[0] = 0;
        setText(lv_obj_get_child(r, 2), b);
        setText(lv_obj_get_child(r, 3), fmtAge(h.age + ex).c_str());
        const bool pos = me && h.hasPos;
        setText(lv_obj_get_child(r, 4), pos ? fmtKm(proto::distanceKm(la, lo, h.lat, h.lon)).c_str() : h.hasPos ? "pos" : "-");
        setText(lv_obj_get_child(r, 5), pos ? proto::cardinal16(proto::bearingDeg(la, lo, h.lat, h.lon)) : "");
    }
    sList.end();
}
static void enterSlist() { if (linked()) { send(proto::cmdHeard(0, 12)); send(proto::cmdHeard(12, 12)); } }

// ------------------------------------------------------------------ detalle de una estacion
static lv_obj_t *dSym, *dCall, *dKind, *dNone, *dWxPanel, *dQsy, *dGo;
static Compass* dComp;
static lv_obj_t *dBrg, *dDst;
static Rows dRows, dWx;

static void onMsg(lv_event_t*) { go(THREAD, arg()); }
static void onRotor(lv_event_t*) {
    const proto::Heard* h = findCall(arg());
    double la, lo;
    if (!h || !h->hasPos || !ownPos(la, lo)) { error("This station has no position (or the tracker no GNSS fix)"); return; }
    if (cfg::get().rotorLink && net::rotorState() == net::Link::Linked) {
        net::rotorSend(rotp::goTo((float)proto::bearingDeg(la, lo, h->lat, h->lon), 0, false));
        notice("Rotor box: going to the station's bearing");
    } else {
        // el rotor del tracker: fuente 4 (ultima estacion oida) con el filtro de este indicativo
        send(proto::Cmd("tgt").num("src", 4).text("ff", h->call).str());
        notice("Tracker rotor: following this station (source 4)");
    }
}
static void doQsy(void*) {
    const proto::Heard* h = findCall(arg());
    if (h) send(proto::Cmd("qsy").text("c", h->cmt).text("name", h->call).str());
}
static void onQsy(lv_event_t*) { confirm("QSY", "The tracker's FM radio tunes to the frequency in this station's comment. From another mode it restarts in FM.", "QSY", doQsy); }
static void onFollow(lv_event_t*) {
    cfg::get().rotorFollow = 2;
    cfg::get().rotorFollowCall = arg();
    cfg::save();
    notice("The rotor box follows this station (Rotor > Follow)");
}

static void buildStation(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Station", &tr);
    button(tr, "MESSAGE", onMsg, nullptr, 0, 48, P->cyan);
    dQsy = button(tr, "QSY", onQsy, nullptr, 0, 48, P->green);
    button(tr, "ROTOR", onRotor, nullptr, 0, 48);
    dGo = button(tr, "FOLLOW", onFollow, nullptr, 0, 48);
    lv_obj_t* body = row(root, GAP);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_height(body, 10);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* left = panel(body, 12);
    lv_obj_set_size(left, 380, LV_PCT(100));
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* hr = row(left, 10);
    dSym = symbol(hr);
    lv_obj_t* hc = col(hr, 0);
    lv_obj_set_flex_grow(hc, 1);
    lv_obj_set_width(hc, 10);
    dCall = text(hc, "", &app_bold_34, P->text);
    dKind = text(hc, "", &app_text_18, P->muted);
    dComp = compass(left, 290);
    lv_obj_t* br = row(left, 8);
    lv_obj_set_flex_align(br, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    dBrg = text(br, "-", &app_mono_28, P->amber);
    dDst = text(br, "-", &app_mono_28, P->text);
    lv_obj_t* right = col(body, GAP);
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_width(right, 10);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_set_scrollable(right, true);
    lv_obj_t* ip = panel(right, 14);
    dNone = wrap(ip, "This station is not in the tracker's list any more.", &app_text_20, P->muted);
    dRows.create(ip, 150);
    dWxPanel = panel(right, 14);
    section(dWxPanel, "WEATHER");
    dWx.create(dWxPanel, 150);
}
static void refreshStation() {
    const proto::Heard* h = findCall(arg());
    setText(dCall, arg().c_str());
    show(dNone, !h);
    show(dWxPanel, h && h->wx.ok);
    show(dQsy, h && M->info.capFm && !h->cmt.empty() && (h->cmt.find("MHz") != std::string::npos || h->cmt.find("mhz") != std::string::npos));
    if (!h) { dRows.begin(); dRows.end(); dComp->set(0, nullptr, nullptr, nullptr); return; }
    symbolSet(dSym, h->symTable(), h->symCode());
    std::string kind = symbolName(h->symTable(), h->symCode());
    if (!h->kind.empty() && h->kind != "?") kind = h->kind + (kind.empty() ? "" : " \xC2\xB7 " + kind);
    setText(dKind, kind.c_str());
    double la, lo;
    const bool me = ownPos(la, lo);
    char b[160];
    float brg = 0;
    const bool pos = me && h->hasPos;
    if (pos) {
        brg = (float)proto::bearingDeg(la, lo, h->lat, h->lon);
        snprintf(b, sizeof b, "%s %s", fmtDeg(brg).c_str(), proto::cardinal16(brg));
        setText(dBrg, b);
        setText(dDst, fmtKm(proto::distanceKm(la, lo, h->lat, h->lon)).c_str());
    } else { setText(dBrg, "-"); setText(dDst, h->hasPos ? "no own fix" : "no position"); }
    float rot = RM.pos.az;
    const bool rotOk = RM.pos.ok && net::rotorState() == net::Link::Linked;
    float own = M->gnss.crs;
    dComp->set(0, pos ? &brg : nullptr, rotOk ? &rot : nullptr, M->gnss.ok && M->gnss.spd > 3 ? &own : nullptr);
    dRows.begin();
    const int ex = extraAge();
    dRows.add("HEARD", (fmtAge(h->age + ex) + " ago \xC2\xB7 " + std::to_string(h->n) + " packets").c_str());
    if (h->rssi) dRows.addf("SIGNAL", P->text, "%d dBm \xC2\xB7 SNR %.1f dB", h->rssi, h->snr);
    if (h->hasPos) {
        dRows.addf("POSITION", P->text, "%.5f  %.5f", h->lat, h->lon);
        dRows.add("LOCATOR", proto::locator(h->lat, h->lon).c_str());
        if (h->pa >= 0) dRows.add("POSITION AGE", fmtAge(h->pa + ex).c_str());
    }
    if (h->alt) dRows.addf("ALTITUDE", P->text, "%d m", h->alt);
    if (h->spd > 0) dRows.addf("SPEED", P->text, "%d km/h \xC2\xB7 %s", h->spd, fmtDeg(h->crs).c_str());
    if (!h->dev.empty()) dRows.add("DEVICE", h->dev.c_str());
    if (!h->sts.empty()) dRows.add("STATUS", h->sts.c_str(), h->sts.find("mergency") != std::string::npos ? P->red : P->text);
    if (!h->path.empty()) dRows.add("PATH", h->path.c_str());
    if (!h->cmt.empty()) dRows.add("COMMENT", h->cmt.c_str());
    dRows.add("SYMBOL", (std::string(1, h->symTable()) + h->symCode() + "  " + symbolName(h->symTable(), h->symCode())).c_str());
    dRows.end();
    if (h->wx.ok) {
        dWx.begin();
        if (h->wx.t > -900) dWx.addf("TEMPERATURE", tempColor(h->wx.t), "%.1f \xC2\xB0" "C", h->wx.t);
        if (h->wx.h > -900) dWx.addf("HUMIDITY", P->text, "%.0f %%", h->wx.h);
        if (h->wx.p > -900) dWx.addf("PRESSURE", P->text, "%.1f hPa", h->wx.p);
        if (h->wx.ws > -900) {
            int k = snprintf(b, sizeof b, "%.0f km/h", h->wx.ws);
            if (h->wx.wg > -900) k += snprintf(b + k, sizeof b - k, ", gusts %.0f", h->wx.wg);
            if (h->wx.wd > -900) snprintf(b + k, sizeof b - k, " from %s", proto::cardinal16(h->wx.wd));
            dWx.add("WIND", b);
        }
        if (h->wx.r > -900) dWx.addf("RAIN", P->text, "%.1f mm (1 h)", h->wx.r / 10.0f);
        dWx.end();
    }
}
static void enterStation() { if (linked()) send(proto::cmdHeard(0, 12)); }

// ------------------------------------------------------------------ mapa de radar
static lv_obj_t* mapObj;
static lv_obj_t *mRangeC[5], *mInfo;
static int mRange = 0;                      // 0 auto
static const int RANGES[] = {0, 5, 20, 50, 200};
static float mShownKm = 20;
struct MapPt { int x, y, idx; };
static std::vector<MapPt> mPts;

static void onRange(lv_event_t* e) { mRange = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static void mapDraw(lv_event_t* e) {
    lv_obj_t* o = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const int cx = (a.x1 + a.x2) / 2, cy = (a.y1 + a.y2) / 2, R = lv_area_get_height(&a) / 2 - 16;
    lv_draw_arc_dsc_t ad;
    lv_draw_arc_dsc_init(&ad);
    ad.center.x = cx; ad.center.y = cy; ad.start_angle = 0; ad.end_angle = 360; ad.width = 1; ad.color = C(P->panelBorder);
    lv_draw_label_dsc_t td;
    lv_draw_label_dsc_init(&td);
    td.font = &app_cond_16; td.color = C(P->faint);
    char b[32];
    for (int k = 1; k <= 4; k++) {
        ad.radius = R * k / 4;
        lv_draw_arc(layer, &ad);
        float km = mShownKm * k / 4;
        snprintf(b, sizeof b, km < 10 ? "%.1f km" : "%.0f km", km);
        td.text = b;
        lv_area_t la = {cx + 4, cy - R * k / 4 + 2, cx + 90, cy - R * k / 4 + 22};
        lv_draw_label(layer, &td, &la);
    }
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = C(P->subBorder); ld.width = 1;
    ld.p1.x = cx - R; ld.p1.y = cy; ld.p2.x = cx + R; ld.p2.y = cy; lv_draw_line(layer, &ld);
    ld.p1.x = cx; ld.p1.y = cy - R; ld.p2.x = cx; ld.p2.y = cy + R; lv_draw_line(layer, &ld);
    td.font = &app_bold_22; td.color = C(P->red); td.text = "N"; td.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t na = {cx - 12, cy - R - 16, cx + 12, cy - R + 8};
    lv_draw_label(layer, &td, &na);
    // rumbo del rotor
    if (RM.pos.ok && net::rotorState() == net::Link::Linked) {
        float t = RM.pos.az * (float)M_PI / 180;
        ld.color = C(P->cyan); ld.width = 3;
        ld.p1.x = cx; ld.p1.y = cy; ld.p2.x = cx + R * sinf(t); ld.p2.y = cy - R * cosf(t);
        lv_draw_line(layer, &ld);
    }
    // estaciones
    for (const MapPt& p : mPts) {
        const proto::Heard& h = M->heard[p.idx];
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        uint32_t col2 = h.wx.ok ? P->amber : h.kind == "MOBILE" || h.spd > 0 ? P->green : h.kind == "DIGI" || h.kind == "IGATE" ? P->cyan : P->text;
        d.bg_color = C(col2); d.radius = LV_RADIUS_CIRCLE; d.border_color = C(P->bg); d.border_width = 2;
        lv_area_t sa = {p.x - 8, p.y - 8, p.x + 8, p.y + 8};
        lv_draw_rect(layer, &d, &sa);
        td.font = &app_bold_18; td.color = C(P->text); td.align = LV_TEXT_ALIGN_LEFT; td.text = h.call.c_str();
        lv_area_t la = {p.x + 11, p.y - 12, p.x + 160, p.y + 10};
        lv_draw_label(layer, &td, &la);
        if (h.wx.ok && h.wx.t > -900) {
            snprintf(b, sizeof b, "%.0f\xC2\xB0", h.wx.t);
            td.font = &app_cond_16; td.color = C(tempColor(h.wx.t)); td.text = b;
            lv_area_t wa = {p.x + 11, p.y + 8, p.x + 80, p.y + 26};
            lv_draw_label(layer, &td, &wa);
        }
    }
    // posicion propia
    lv_draw_triangle_dsc_t tr;
    lv_draw_triangle_dsc_init(&tr);
    tr.color = C(P->green);
    float t = (M->gnss.ok ? M->gnss.crs : 0) * (float)M_PI / 180;
    auto pt = [&](float ang, float r, lv_point_precise_t& q) { q.x = cx + r * sinf(t + ang); q.y = cy - r * cosf(t + ang); };
    pt(0, 16, tr.p[0]); pt(2.4f, 11, tr.p[1]); pt(-2.4f, 11, tr.p[2]);
    lv_draw_triangle(layer, &tr);
}
static void mapClick(lv_event_t* e) {
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    int best = -1, bd = 40 * 40;
    for (const MapPt& m : mPts) { int d = (m.x - p.x) * (m.x - p.x) + (m.y - p.y) * (m.y - p.y); if (d < bd) { bd = d; best = m.idx; } }
    (void)e;
    if (best >= 0) go(STATION, M->heard[best].call);
}
static void buildMap(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Map \xC2\xB7 radar", &tr);
    static const char* rn[] = {"AUTO", "5 km", "20 km", "50 km", "200 km"};
    for (int i = 0; i < 5; i++) mRangeC[i] = chip(tr, rn[i], i == mRange, onRange, (void*)(intptr_t)i);
    lv_obj_t* body = row(root, GAP);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_height(body, 10);
    lv_obj_t* mp = panel(body, 0);
    lv_obj_set_height(mp, LV_PCT(100));
    lv_obj_set_flex_grow(mp, 1);
    lv_obj_set_width(mp, 10);
    mapObj = box(mp);
    lv_obj_set_size(mapObj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_clickable(mapObj, true);
    lv_obj_add_event_cb(mapObj, mapDraw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(mapObj, mapClick, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* ip = panel(body, 14);
    lv_obj_set_size(ip, 240, LV_PCT(100));
    section(ip, "LEGEND");
    text(ip, "\xE2\x80\xA2 mobile", &app_text_20, P->green);
    text(ip, "\xE2\x80\xA2 digi / iGate", &app_text_20, P->cyan);
    text(ip, "\xE2\x80\xA2 weather", &app_text_20, P->amber);
    text(ip, "\xE2\x80\xA2 fixed", &app_text_20, P->text);
    text(ip, "\xE2\x80\x94 rotor", &app_text_20, P->cyan);
    mInfo = wrap(ip, "", &app_text_18, P->muted);
}
static void refreshMap() {
    for (int i = 0; i < 5; i++) chipSet(mRangeC[i], i == mRange);
    double la, lo;
    mPts.clear();
    if (!ownPos(la, lo)) { setText(mInfo, "Needs the tracker's GNSS position."); lv_obj_invalidate(mapObj); return; }
    // escala automatica: la que deja dentro tres cuartas partes de las estaciones (las lejanas no aplastan a las cercanas)
    std::vector<double> ds;
    for (const auto& h : M->heard) if (h.hasPos) ds.push_back(proto::distanceKm(la, lo, h.lat, h.lon));
    std::sort(ds.begin(), ds.end());
    double far = ds.empty() ? 1 : std::max(1.0, ds[(ds.size() * 3) / 4 < ds.size() ? (ds.size() * 3) / 4 : ds.size() - 1]);
    if (mRange) mShownKm = (float)RANGES[mRange];
    else { static const float steps[] = {2, 5, 10, 20, 50, 100, 200, 500, 1000}; mShownKm = 1000; for (float s : steps) if (far * 1.1 <= s) { mShownKm = s; break; } }
    lv_area_t a;
    lv_obj_get_coords(mapObj, &a);
    const int cx = (a.x1 + a.x2) / 2, cy = (a.y1 + a.y2) / 2, R = lv_area_get_height(&a) / 2 - 16;
    int out = 0;
    for (size_t i = 0; i < M->heard.size(); i++) {
        const proto::Heard& h = M->heard[i];
        if (!h.hasPos) continue;
        double d = proto::distanceKm(la, lo, h.lat, h.lon), b = proto::bearingDeg(la, lo, h.lat, h.lon) * M_PI / 180;
        if (d > mShownKm) { out++; continue; }
        float r = (float)(d / mShownKm * R);
        mPts.push_back({cx + (int)(r * sin(b)), cy - (int)(r * cos(b)), (int)i});
    }
    setTextf(mInfo, "%d on the map%s. Touch a station for its details. Without map tiles: a radar around the tracker.", (int)mPts.size(),
             out ? (", " + std::to_string(out) + " farther").c_str() : "");
    lv_obj_invalidate(mapObj);
}

// ------------------------------------------------------------------ estaciones meteorologicas
static int wSort = 0;                       // 0 distancia, 1 hora, 2 temperatura, 3 viento
static lv_obj_t* wSortC[4];
static lv_obj_t* wEmpty;
static List wList;
static std::vector<int> wView;
static void onWSort(lv_event_t* e) { wSort = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static lv_obj_t* makeWxRow(lv_obj_t* p) {
    lv_obj_t* r = row(p, 14);
    lv_obj_set_height(r, 66);
    lv_obj_set_style_pad_hor(r, 12, 0);
    lv_obj_set_style_radius(r, 10, 0);
    lv_obj_set_style_bg_color(r, C(P->panel), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_t* c = col(r, 0);
    lv_obj_set_width(c, 240);
    text(c, "", &app_bold_22, P->text);
    text(c, "", &app_text_18, P->muted);
    lv_obj_t* t = text(r, "", &app_mono_40, P->text);
    lv_obj_set_width(t, 150);
    lv_obj_t* d = col(r, 0);
    lv_obj_set_flex_grow(d, 1);
    lv_obj_set_width(d, 10);
    text(d, "", &app_text_20, P->subText);
    text(d, "", &app_text_18, P->muted);
    return r;
}
static void onWxClick(lv_event_t* e) { int i = clickIndex(e); if (i >= 0 && i < (int)wView.size()) go(STATION, M->heard[wView[i]].call); }
static void buildWx(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Weather stations", &tr);
    static const char* so[] = {"DIST", "TIME", "TEMP", "WIND"};
    for (int i = 0; i < 4; i++) wSortC[i] = chip(tr, so[i], i == wSort, onWSort, (void*)(intptr_t)i);
    lv_obj_t* sa = scrollArea(root);
    wEmpty = wrap(sa, "No weather station heard yet.", &app_text_20, P->muted);
    wList.create(sa, makeWxRow, onWxClick);
}
static void refreshWx() {
    for (int i = 0; i < 4; i++) chipSet(wSortC[i], i == wSort);
    double la, lo;
    const bool me = ownPos(la, lo);
    wView.clear();
    for (size_t i = 0; i < M->heard.size(); i++) if (M->heard[i].wx.ok) wView.push_back((int)i);
    auto dist = [&](int i) { const auto& h = M->heard[i]; return me && h.hasPos ? proto::distanceKm(la, lo, h.lat, h.lon) : 1e9; };
    if (wSort == 0) std::stable_sort(wView.begin(), wView.end(), [&](int a, int b) { return dist(a) < dist(b); });
    if (wSort == 2) std::stable_sort(wView.begin(), wView.end(), [](int a, int b) { return M->heard[a].wx.t > M->heard[b].wx.t; });
    if (wSort == 3) std::stable_sort(wView.begin(), wView.end(), [](int a, int b) { return M->heard[a].wx.ws > M->heard[b].wx.ws; });
    show(wEmpty, wView.empty());
    wList.begin();
    char b[128];
    const int ex = extraAge();
    for (int i : wView) {
        const proto::Heard& h = M->heard[i];
        const proto::HeardWx& w = h.wx;
        lv_obj_t* r = wList.next();
        lv_obj_t* c = lv_obj_get_child(r, 0);
        setText(lv_obj_get_child(c, 0), h.call.c_str());
        std::string s = fmtAge(h.age + ex);
        if (me && h.hasPos) s = fmtKm(proto::distanceKm(la, lo, h.lat, h.lon)) + " \xC2\xB7 " + s;
        setText(lv_obj_get_child(c, 1), s.c_str());
        lv_obj_t* t = lv_obj_get_child(r, 1);
        if (w.t > -900) { snprintf(b, sizeof b, "%.1f\xC2\xB0", w.t); setText(t, b); setColor(t, tempColor(w.t)); } else { setText(t, "-"); setColor(t, P->off); }
        lv_obj_t* d = lv_obj_get_child(r, 2);
        int k = 0;
        b[0] = 0;
        if (w.ws > -900) k += snprintf(b + k, sizeof b - k, "Wind %.0f km/h%s%s", w.ws, w.wd > -900 ? " " : "", w.wd > -900 ? proto::cardinal16(w.wd) : "");
        if (w.wg > -900) k += snprintf(b + k, sizeof b - k, ", gusts %.0f", w.wg);
        setText(lv_obj_get_child(d, 0), b);
        k = 0; b[0] = 0;
        if (w.h > -900) k += snprintf(b + k, sizeof b - k, "Hum %.0f %%   ", w.h);
        if (w.p > -900) k += snprintf(b + k, sizeof b - k, "%.1f hPa   ", w.p);
        if (w.r > -900) snprintf(b + k, sizeof b - k, "Rain %.1f mm", w.r / 10.0f);
        setText(lv_obj_get_child(d, 1), b);
    }
    wList.end();
}

void initStations() {
    registerScreen(SLIST, {"Stations", false, buildSlist, refreshSlist, enterSlist, nullptr});
    registerScreen(STATION, {"Station", false, buildStation, refreshStation, enterStation, nullptr});
    registerScreen(MAP, {"Map", true, buildMap, refreshMap, enterSlist, nullptr});
    registerScreen(WX, {"Weather", false, buildWx, refreshWx, enterSlist, nullptr});
}

} // namespace r7
#endif
