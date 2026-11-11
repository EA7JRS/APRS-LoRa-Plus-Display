/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Estaciones que oye el tracker (StationScreens.kt y WxScreen.kt de la app): S.LIST con orden y filtros, detalle con brujula,
 * meteorologia, mensaje, QSY y rotor; mapa (teselas de OpenStreetMap, IGN, Esri y OpenSeaMap, o radar alrededor de la posicion propia) y estaciones meteorologicas.
 */
#ifdef UI_RADIO
#include "r7.h"
#include "net.h"
#include "maptiles.h"
#include "wxcalc.h"
#include <map>
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
        if (h.wx.t > -900) k += snprintf(b + k, sizeof b - k, "%s ", fmtTemp(h.wx.t, 0).c_str());
        if (h.wx.ws > -900) k += snprintf(b + k, sizeof b - k, "wind %s F%d ", fmtSpeed(h.wx.ws).c_str(), wxc::beaufort(wxc::kmhToKn(h.wx.ws)));
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
    if (h->spd > 0) dRows.addf("SPEED", P->text, "%s \xC2\xB7 %s", fmtSpeed(h->spd).c_str(), fmtDeg(h->crs).c_str());
    if (!h->dev.empty()) dRows.add("DEVICE", h->dev.c_str());
    if (!h->sts.empty()) dRows.add("STATUS", h->sts.c_str(), h->sts.find("mergency") != std::string::npos ? P->red : P->text);
    if (!h->path.empty()) dRows.add("PATH", h->path.c_str());
    if (!h->cmt.empty()) dRows.add("COMMENT", h->cmt.c_str());
    dRows.add("SYMBOL", (std::string(1, h->symTable()) + h->symCode() + "  " + symbolName(h->symTable(), h->symCode())).c_str());
    dRows.end();
    if (h->wx.ok) {
        dWx.begin();
        const wxc::Unit su = wxc::speedUnit(cfg::get().units);
        if (h->wx.t > -900) dWx.add("TEMPERATURE", fmtTemp(h->wx.t).c_str(), tempColor(h->wx.t));
        int fk = 0;
        const float feels = wxc::feelsLikeC(h->wx.t, h->wx.h, h->wx.ws, &fk), dew = wxc::dewPointC(h->wx.t, h->wx.h);
        if (wxc::has(feels)) dWx.addf("FEELS LIKE", tempColor(feels), "%s (%s)", fmtTemp(feels).c_str(), fk == 1 ? "wind chill" : "heat index");
        if (h->wx.h > -900) dWx.addf("HUMIDITY", P->text, "%.0f %%", h->wx.h);
        if (wxc::has(dew)) dWx.addf("DEW POINT", P->text, "%s \xC2\xB7 cloud base ~%.0f m", fmtTemp(dew).c_str(), wxc::cloudBaseM(h->wx.t, dew));
        if (h->wx.p > -900) {
            float d3;
            if (wxTrend(h->call, d3)) dWx.addf("PRESSURE", d3 <= -3.5f ? P->amber : P->text, "%.1f hPa \xC2\xB7 %s (%+.1f in 3 h)", h->wx.p, wxc::PressureTrend::name(d3), d3);
            else dWx.addf("PRESSURE", P->text, "%.1f hPa", h->wx.p);
        }
        if (h->wx.ws > -900) {
            int k = snprintf(b, sizeof b, "%.0f %s", h->wx.ws * su.k, su.label);
            if (h->wx.wg > -900) k += snprintf(b + k, sizeof b - k, ", gusts %.0f", h->wx.wg * su.k);
            if (h->wx.wd > -900) snprintf(b + k, sizeof b - k, " from %s", proto::cardinal16(h->wx.wd));
            dWx.add("WIND", b);
            const int f = wxc::beaufort(wxc::kmhToKn(h->wx.ws));
            dWx.addf("BEAUFORT", f >= 8 ? P->red : f >= 6 ? P->amber : P->text, "Force %d \xC2\xB7 %s", f, wxc::beaufortName(f));
        }
        if (h->wx.r > -900) dWx.addf("RAIN", P->text, "%.1f mm (1 h)", h->wx.r / 10.0f);
        dWx.end();
    }
}
static void enterStation() { if (linked()) send(proto::cmdHeard(0, 12)); }

// ------------------------------------------------------------------ mapa
// Con teselas (maptiles.h: OpenStreetMap, IGN, oscuro, Esri con la clave del usuario, y OpenSeaMap encima) o, sin ellas,
// el radar alrededor del tracker. Encima: estaciones con su simbolo, barcos AIS, posicion propia y haz del rotor.
// El dibujo va en puntos reales de la pantalla (no pasa por px()).
constexpr int RADAR = mapt::LAYER_COUNT;      // cfg.mapLayer: 0..4 capas de teselas, 5 radar
static lv_obj_t* mapObj;
static lv_obj_t *mRangeC[5], *mInfo;
static int mRange = 0;                      // 0 auto
static const int RANGES[] = {0, 5, 20, 50, 200};
static float mShownKm = 20;
struct MapPt { int x, y, idx; };
static std::vector<MapPt> mPts;

static void onRange(lv_event_t* e) { mRange = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static void radarDraw(lv_event_t* e) {
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
static void refreshRadar() {
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


// ---- teselas ----
static lv_obj_t *mLayerC, *mSeaC, *mZoomOut, *mZoomIn, *mMyC, *mLegend;
static double mLat = 40.0, mLon = -3.7;       // centro
static bool mFollow = true, mCentered = false;
static int mDrag = 0;
static uint32_t mSeq = 0;
static bool tiles() { return cfg::get().mapLayer != RADAR; }
static int zoomMax() { return mapt::layer(cfg::get().mapLayer).maxZoom; }

static int textW(const char* t, const lv_font_t* f) {
    int w = 0;
    for (const char* c = t; *c; c++) w += lv_font_get_glyph_width(f, (uint8_t)*c, (uint8_t)c[1]);
    return w;
}
// rotulo con fondo, para que se lea encima del mapa
static void tag(lv_layer_t* layer, int x, int y, const char* t, const lv_font_t* f, uint32_t color) {
    const int w = textW(t, f), h = lv_font_get_line_height(f);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = C(P->bg); d.bg_opa = LV_OPA_70; d.radius = 4;
    lv_area_t ba = {x - 3, y - 1, x + w + 3, y + h};
    lv_draw_rect(layer, &d, &ba);
    lv_draw_label_dsc_t td;
    lv_draw_label_dsc_init(&td);
    td.font = f; td.color = C(color); td.text = t; td.text_local = 1;
    lv_area_t la = {x, y, x + w + 8, y + h};
    lv_draw_label(layer, &td, &la);
}

static void tilesDraw(lv_event_t* e) {
    lv_obj_t* o = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const int w = lv_area_get_width(&a), h = lv_area_get_height(&a);
    const cfg::Settings& st = cfg::get();
    const int z = st.mapZoom, n = 1 << z;
    double cx, cy;
    mapt::toWorld(mLat, mLon, z, cx, cy);
    const double x0 = cx - w / 2.0, y0 = cy - h / 2.0;
    lv_draw_rect_dsc_t miss;
    lv_draw_rect_dsc_init(&miss);
    miss.bg_color = C(P->subPanel); miss.border_color = C(P->subBorder); miss.border_width = 1;
    for (int ty = (int)floor(y0 / mapt::TILE); ty <= (int)floor((y0 + h - 1) / mapt::TILE); ty++)
        for (int tx = (int)floor(x0 / mapt::TILE); tx <= (int)floor((x0 + w - 1) / mapt::TILE); tx++) {
            const int sx = a.x1 + (int)floor(tx * (double)mapt::TILE - x0), sy = a.y1 + (int)floor(ty * (double)mapt::TILE - y0);
            lv_area_t ta = {sx, sy, sx + mapt::TILE - 1, sy + mapt::TILE - 1};
            const lv_image_dsc_t* img = nullptr;
            if (ty >= 0 && ty < n) {
                mapt::Key k;
                k.layer = (uint8_t)st.mapLayer; k.z = (uint8_t)z; k.sea = st.mapSea; k.x = (uint32_t)(((tx % n) + n) % n); k.y = (uint32_t)ty;
                img = mapt::get(k);
            }
            if (img) {
                lv_draw_image_dsc_t id;
                lv_draw_image_dsc_init(&id);
                id.src = img;
                lv_draw_image(layer, &id, &ta);
            } else lv_draw_rect(layer, &miss, &ta);
        }
    auto project = [&](double la, double lo, int& x, int& y) {
        double wx, wy;
        mapt::toWorld(la, lo, z, wx, wy);
        double dx = wx - cx;
        const double world = mapt::TILE * (double)n;
        if (dx > world / 2) dx -= world; else if (dx < -world / 2) dx += world;
        x = a.x1 + w / 2 + (int)lround(dx); y = a.y1 + h / 2 + (int)lround(wy - cy);
        return x > a.x1 - 40 && x < a.x2 + 40 && y > a.y1 - 40 && y < a.y2 + 40;
    };
    char b[48];
    double ola, olo;
    const bool own = ownPos(ola, olo);
    int ox = 0, oy = 0;
    const bool ownIn = own && project(ola, olo, ox, oy);
    // haz del rotor
    if (own && RM.pos.ok && net::rotorState() == net::Link::Linked) {
        project(ola, olo, ox, oy);
        const float t = RM.pos.az * (float)M_PI / 180, len = (float)(w > h ? w : h);
        lv_draw_line_dsc_t ld;
        lv_draw_line_dsc_init(&ld);
        ld.color = C(P->cyan); ld.width = 3; ld.opa = LV_OPA_80;
        ld.p1.x = ox; ld.p1.y = oy; ld.p2.x = ox + len * sinf(t); ld.p2.y = oy - len * cosf(t);
        lv_draw_line(layer, &ld);
    }
    // barcos AIS
    for (const proto::Vessel& v : M->ais) {
        int x, y;
        if (!v.hasPos || !project(v.lat, v.lon, x, y)) continue;
        lv_draw_triangle_dsc_t tr;
        lv_draw_triangle_dsc_init(&tr);
        tr.color = C(v.danger ? P->red : P->amber);
        const float t = (v.cog >= 0 ? v.cog : v.hdg >= 0 ? (float)v.hdg : 0) * (float)M_PI / 180;
        auto pt = [&](float ang, float r, lv_point_precise_t& q) { q.x = x + r * sinf(t + ang); q.y = y - r * cosf(t + ang); };
        pt(0, 13, tr.p[0]); pt(2.5f, 9, tr.p[1]); pt(-2.5f, 9, tr.p[2]);
        lv_draw_triangle(layer, &tr);
        if (!v.name.empty()) tag(layer, x + 12, y - 8, v.name.c_str(), &app_cond_16, v.danger ? P->red : P->amber);
    }
    // estaciones
    mPts.clear();
    for (size_t i = 0; i < M->heard.size(); i++) {
        const proto::Heard& hd = M->heard[i];
        int x, y;
        if (!hd.hasPos || !project(hd.lat, hd.lon, x, y)) continue;
        mPts.push_back({x, y, (int)i});
        drawSymbol(layer, x, y, hd.symTable(), hd.symCode());
        tag(layer, x + SYM / 2 + 6, y - SYM / 2, hd.call.c_str(), &app_bold_18, P->text);
        if (hd.wx.ok && hd.wx.t > -900) {
            snprintf(b, sizeof b, "%.0f\xC2\xB0", wxc::tempShown(hd.wx.t, cfg::get().units));
            tag(layer, x + SYM / 2 + 6, y - SYM / 2 + lv_font_get_line_height(&app_bold_18) + 1, b, &app_cond_16, tempColor(hd.wx.t));
        }
    }
    // posicion propia
    if (ownIn) {
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = C(P->bg); d.radius = LV_RADIUS_CIRCLE;
        lv_area_t ca = {ox - 13, oy - 13, ox + 13, oy + 13};
        lv_draw_rect(layer, &d, &ca);
        lv_draw_triangle_dsc_t tr;
        lv_draw_triangle_dsc_init(&tr);
        tr.color = C(P->green);
        const float t = (M->gnss.ok ? M->gnss.crs : 0) * (float)M_PI / 180;
        auto pt = [&](float ang, float r, lv_point_precise_t& q) { q.x = ox + r * sinf(t + ang); q.y = oy - r * cosf(t + ang); };
        pt(0, 12, tr.p[0]); pt(2.4f, 9, tr.p[1]); pt(-2.4f, 9, tr.p[2]);
        lv_draw_triangle(layer, &tr);
    }
    // escala
    const double mpp = mapt::metersPerPixel(mLat, z);
    // en metros / km, o en millas (nauticas o terrestres) segun las unidades elegidas
    static const double NICE[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, 2000000};
    const int usys = cfg::get().units;
    const double unitM = usys == 1 ? 1852.0 : usys == 2 ? 1609.344 : 1.0;      // metros por unidad de la escala (/1000 en millas)
    double len = NICE[0];
    for (double v : NICE) if (v * (usys ? unitM / 1000 : 1) / mpp <= w / 5.0) len = v;
    const int bar = (int)(len * (usys ? unitM / 1000 : 1) / mpp);
    lv_draw_rect_dsc_t sb;
    lv_draw_rect_dsc_init(&sb);
    sb.bg_color = C(P->text); sb.border_color = C(P->bg); sb.border_width = 1;
    lv_area_t sa = {a.x1 + 10, a.y2 - 12, a.x1 + 10 + bar, a.y2 - 8};
    lv_draw_rect(layer, &sb, &sa);
    if (usys) snprintf(b, sizeof b, len >= 1000 ? "%.0f %s" : "%.2g %s", len / 1000, usys == 1 ? "nm" : "mi");
    else if (len >= 1000) snprintf(b, sizeof b, "%.0f km", len / 1000); else snprintf(b, sizeof b, "%.0f m", len);
    tag(layer, a.x1 + 10, a.y2 - 14 - lv_font_get_line_height(&app_cond_16), b, &app_cond_16, P->text);
    // de quien es el mapa y como va la descarga
    std::string at = mapt::layer(st.mapLayer).attribution;
    if (st.mapSea) at += " \xC2\xB7 OpenSeaMap";
    tag(layer, a.x2 - textW(at.c_str(), &app_cond_16) - 8, a.y2 - lv_font_get_line_height(&app_cond_16) - 4, at.c_str(), &app_cond_16, P->text);
    const int pend = mapt::pending();
    const char* why = st.mapLayer == mapt::ESRI && st.arcgisKey.empty() ? "Esri needs your ArcGIS key (Settings)"
        : !net::wifi().connected ? (net::sdOk() ? "No WiFi: only the tiles saved on the microSD" : "No WiFi and no microSD: no tiles") : nullptr;
    if (why) tag(layer, a.x1 + 10, a.y1 + 8, why, &app_cond_18, P->amber);
    else if (pend) { snprintf(b, sizeof b, "loading %d", pend); tag(layer, a.x1 + 10, a.y1 + 8, b, &app_cond_16, P->muted); }
    snprintf(b, sizeof b, "z%d", z);
    tag(layer, a.x2 - textW(b, &app_cond_16) - 8, a.y1 + 8, b, &app_cond_16, P->muted);
}

static void mapDraw(lv_event_t* e) { if (tiles()) tilesDraw(e); else radarDraw(e); }
static void mapPress(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) { mDrag = 0; return; }
    if (!tiles()) return;
    lv_point_t v;
    lv_indev_get_vect(lv_indev_active(), &v);
    if (!v.x && !v.y) return;
    mDrag += abs(v.x) + abs(v.y);
    if (mDrag < 8) return;                                      // un toque no mueve el mapa
    double cx, cy;
    const int z = cfg::get().mapZoom;
    mapt::toWorld(mLat, mLon, z, cx, cy);
    mapt::toLatLon(cx - v.x, cy - v.y, z, mLat, mLon);
    if (mLon < -180) mLon += 360; else if (mLon > 180) mLon -= 360;
    mFollow = false;
    chipSet(mMyC, false);
    lv_obj_invalidate(mapObj);
}
static void mapClick(lv_event_t* e) {
    (void)e;
    if (mDrag >= 8) return;
    lv_point_t p;
    lv_indev_get_point(lv_indev_active(), &p);
    int best = -1, bd = 40 * 40;
    for (const MapPt& m : mPts) { int d = (m.x - p.x) * (m.x - p.x) + (m.y - p.y) * (m.y - p.y); if (d < bd) { bd = d; best = m.idx; } }
    if (best >= 0) go(STATION, M->heard[best].call);
}
static void onLayer(lv_event_t*) {
    cfg::Settings& st = cfg::get();
    st.mapLayer = (st.mapLayer + 1) % (RADAR + 1);
    if (st.mapLayer == mapt::ESRI && st.arcgisKey.empty()) st.mapLayer = RADAR;      // sin clave no hay Esri
    if (tiles() && st.mapZoom > zoomMax()) st.mapZoom = zoomMax();
    cfg::save();
    mapt::forget();
    if (tiles()) net::mapBegin();
    requestRefresh();
}
static void onSea(lv_event_t*) { cfg::get().mapSea = !cfg::get().mapSea; cfg::save(); mapt::forget(); requestRefresh(); }
static void onZoom(lv_event_t* e) {
    cfg::Settings& st = cfg::get();
    int z = st.mapZoom + (int)(intptr_t)lv_event_get_user_data(e);
    z = z < 3 ? 3 : z > zoomMax() ? zoomMax() : z;
    if (z == st.mapZoom) return;
    st.mapZoom = z;
    cfg::save();
    mapt::forget();
    requestRefresh();
}
static void onMy(lv_event_t*) { mFollow = true; requestRefresh(); }
static void mapTimer(lv_timer_t*) {
    if (top() != MAP || !tiles() || mapt::seq() == mSeq) return;
    mSeq = mapt::seq();
    lv_obj_invalidate(mapObj);
}
void mapCenterOn(double lat, double lon) {
    mLat = lat; mLon = lon; mCentered = true; mFollow = false;
    if (!tiles()) { cfg::get().mapLayer = mapt::OSM; cfg::save(); }
    if (cfg::get().mapZoom < 13) cfg::get().mapZoom = 13;
}
static void enterMap() { enterSlist(); if (tiles()) net::mapBegin(); }
static void buildMap(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Map", &tr);
    static const char* rn[] = {"AUTO", "5 km", "20 km", "50 km", "200 km"};
    for (int i = 0; i < 5; i++) mRangeC[i] = chip(tr, rn[i], i == mRange, onRange, (void*)(intptr_t)i);
    mSeaC = chip(tr, "SEA", false, onSea, nullptr, 40, P->cyan);
    mZoomOut = chip(tr, " - ", false, onZoom, (void*)(intptr_t)-1);
    mZoomIn = chip(tr, " + ", false, onZoom, (void*)(intptr_t)1);
    mMyC = chip(tr, "MY", true, onMy, nullptr, 40, P->green);
    mLayerC = chip(tr, "OSM", true, onLayer, nullptr, 40, P->amber);
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
    lv_obj_add_event_cb(mapObj, mapPress, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(mapObj, mapPress, LV_EVENT_PRESSING, nullptr);
    lv_obj_t* ip = panel(body, 14);
    mLegend = ip;
    lv_obj_set_size(ip, 240, LV_PCT(100));
    section(ip, "LEGEND");
    text(ip, "\xE2\x80\xA2 mobile", &app_text_20, P->green);
    text(ip, "\xE2\x80\xA2 digi / iGate", &app_text_20, P->cyan);
    text(ip, "\xE2\x80\xA2 weather", &app_text_20, P->amber);
    text(ip, "\xE2\x80\xA2 fixed", &app_text_20, P->text);
    text(ip, "\xE2\x80\x94 rotor", &app_text_20, P->cyan);
    mInfo = wrap(ip, "", &app_text_18, P->muted);
    lv_timer_create(mapTimer, 250, nullptr);
}
static void refreshMap() {
    const cfg::Settings& st = cfg::get();
    const bool t = tiles();
    for (int i = 0; i < 5; i++) show(mRangeC[i], !t);
    show(mSeaC, t); show(mZoomOut, t); show(mZoomIn, t); show(mMyC, t); show(mLegend, !t);
    setText(lv_obj_get_child(mLayerC, 0), t ? mapt::layer(st.mapLayer).name : "RADAR");
    if (!t) { refreshRadar(); return; }
    chipSet(mSeaC, st.mapSea, P->cyan);
    double la, lo;
    if (ownPos(la, lo) && (mFollow || !mCentered)) { mLat = la; mLon = lo; mCentered = true; }
    chipSet(mMyC, mFollow, P->green);
    lv_obj_invalidate(mapObj);
}

// ------------------------------------------------------------------ estaciones meteorologicas
// Tendencia de la presion de cada estacion (hasta 24): una serie por indicativo, alimentada con cada paquete nuevo.
static std::map<std::string, wxc::PressureTrend> wTrend;
static std::map<std::string, int> wSeen;                // n de paquetes ya contado, para no repetir la misma lectura
void wxTrack() {
    static uint32_t last = 0;
    if (nowMs() - last < 20000 && last) return;
    last = nowMs();
    const uint32_t nowS = nowMs() / 1000;
    for (const proto::Heard& h : M->heard) {
        if (!h.wx.ok || h.wx.p < -900) continue;
        auto it = wSeen.find(h.call);
        if (it == wSeen.end() && wTrend.size() >= 24) continue;
        if (it != wSeen.end() && it->second == h.n) continue;
        wSeen[h.call] = h.n;
        const uint32_t age = (uint32_t)(h.age + (int)((nowMs() - M->heardAtMs) / 1000));
        wTrend[h.call].add(nowS > age ? nowS - age : 0, h.wx.p);
    }
}
bool wxTrend(const std::string& call, float& d3h) {
    auto it = wTrend.find(call);
    return it != wTrend.end() && it->second.delta3h(nowMs() / 1000, d3h);
}
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
        if (w.t > -900) { snprintf(b, sizeof b, "%.1f\xC2\xB0", wxc::tempShown(w.t, cfg::get().units)); setText(t, b); setColor(t, tempColor(w.t)); } else { setText(t, "-"); setColor(t, P->off); }
        lv_obj_t* d = lv_obj_get_child(r, 2);
        int k = 0;
        b[0] = 0;
        const wxc::Unit su = wxc::speedUnit(cfg::get().units);
        if (w.ws > -900) k += snprintf(b + k, sizeof b - k, "Wind %.0f %s%s%s", w.ws * su.k, su.label, w.wd > -900 ? " " : "", w.wd > -900 ? proto::cardinal16(w.wd) : "");
        if (w.wg > -900) k += snprintf(b + k, sizeof b - k, ", gusts %.0f", w.wg * su.k);
        if (w.ws > -900) { const int f = wxc::beaufort(wxc::kmhToKn(w.ws)); k += snprintf(b + k, sizeof b - k, " \xC2\xB7 F%d %s", f, wxc::beaufortName(f)); }
        setText(lv_obj_get_child(d, 0), b);
        k = 0; b[0] = 0;
        if (w.h > -900) k += snprintf(b + k, sizeof b - k, "Hum %.0f %%   ", w.h);
        float d3;
        if (w.p > -900) k += wxTrend(h.call, d3) ? snprintf(b + k, sizeof b - k, "%.1f hPa %s   ", w.p, wxc::PressureTrend::name(d3)) : snprintf(b + k, sizeof b - k, "%.1f hPa   ", w.p);
        const float dew = wxc::dewPointC(w.t, w.h), feels = wxc::feelsLikeC(w.t, w.h, w.ws);
        if (wxc::has(dew)) k += snprintf(b + k, sizeof b - k, "Dew %.0f\xC2\xB0   ", wxc::tempShown(dew, cfg::get().units));
        if (wxc::has(feels)) k += snprintf(b + k, sizeof b - k, "Feels %.0f\xC2\xB0   ", wxc::tempShown(feels, cfg::get().units));
        if (w.r > -900 && k < (int)sizeof b) snprintf(b + k, sizeof b - k, "Rain %.1f mm", w.r / 10.0f);
        setText(lv_obj_get_child(d, 1), b);
    }
    wList.end();
}

void initStations() {
    registerScreen(SLIST, {"Stations", false, buildSlist, refreshSlist, enterSlist, nullptr});
    registerScreen(STATION, {"Station", false, buildStation, refreshStation, enterStation, nullptr});
    registerScreen(MAP, {"Map", true, buildMap, refreshMap, enterMap, nullptr});
    registerScreen(WX, {"Weather", false, buildWx, refreshWx, enterSlist, nullptr});
}

} // namespace r7
#endif
