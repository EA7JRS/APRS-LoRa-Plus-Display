/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * SAT (SatTab.kt de la app): paso en curso con brujula, AZ / EL y AOS / LOS; lista de satelites del tracker con categorias,
 * favoritos y eleccion; receptor con Doppler y ajuste fino; emision (con confirmacion); rotor del tracker: fuente y objetivos.
 */
#ifdef UI_RADIO
#include "r7.h"
#include "net.h"
#include <cstdio>
#include <cstring>
#include <ctime>

namespace r7 {

static Compass* sComp;
static lv_obj_t *sName, *sAz, *sEl, *sPass, *sFav, *sTabs[4], *sPages[4], *sCatRow, *sTxRow, *sRxBtn, *sRxInfo, *sRxLog, *sTxRow2, *sTxText;
static lv_obj_t *sSrcRow, *sTgtRow, *sRotInfo, *sNotSat;
static List sList;
static int sTab = 0, sRxTx = 0, sTxTx = 0;
static std::string txShown, tgtShown;

static void requestSat() {
    if (!linked()) return;
    send(proto::cmdSimple("sat"));
    send(proto::Cmd("satlist").num("from", 0).num("cnt", 40).str());
    send(proto::cmdSimple("sattxlist"));
    send(proto::cmdSimple("tgtlist"));
}
static void onTab(lv_event_t* e) { sTab = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static void onCat(lv_event_t* e) { send(proto::Cmd("satcat").num("k", (int)(intptr_t)lv_event_get_user_data(e)).str()); send(proto::Cmd("satlist").num("from", 0).num("cnt", 40).str()); }
static void onFav(lv_event_t*) { send(proto::Cmd("satfav").flag("on", !M->sat.fav).str()); }
static lv_obj_t* makeSatRow(lv_obj_t* p) {
    lv_obj_t* r = row(p, 10);
    lv_obj_set_height(r, 50);
    lv_obj_set_style_pad_hor(r, 10, 0);
    lv_obj_set_style_radius(r, 8, 0);
    lv_obj_set_style_bg_color(r, C(P->subPanel), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, C(P->selected), LV_STATE_PRESSED);
    text(r, "", &app_bold_18, P->amber);
    lv_obj_t* n = text(r, "", &app_bold_22, P->text);
    lv_obj_set_flex_grow(n, 1);
    lv_obj_set_width(n, 10);
    oneLine(n);
    text(r, "", &app_cond_18, P->muted);
    return r;
}
static void onSatClick(lv_event_t* e) {
    int i = clickIndex(e);
    if (i < 0 || i >= (int)M->satList.size()) return;
    send(proto::cmdSatSel(M->satList[i].i));
    send(proto::cmdSimple("sattxlist"));
}
static void onMore(lv_event_t*) { send(proto::Cmd("satlist").num("from", (int)M->satList.size()).num("cnt", 40).str()); }
static void onRxTx(lv_event_t* e) { sRxTx = (int)(intptr_t)lv_event_get_user_data(e); txShown.clear(); requestRefresh(); }
static void onRx(lv_event_t*) { send(proto::Cmd("satrx").flag("on", !M->satRx.on).num("tx", sRxTx).str()); }
static void onFine(lv_event_t* e) { send(proto::Cmd("satrx").flag("on", true).num("tx", sRxTx).num("fine", (int)(intptr_t)lv_event_get_user_data(e)).str()); }
static void onTxTx(lv_event_t* e) { sTxTx = (int)(intptr_t)lv_event_get_user_data(e); txShown.clear(); requestRefresh(); }
static void doTx(void* pos) {
    proto::Cmd c("sattx");
    c.num("tx", sTxTx);
    if (pos) c.flag("pos", true);
    else { std::string t = lv_textarea_get_text(sTxText); if (!t.empty()) c.text("txt", t.substr(0, 64)); }
    send(c.str());
}
static void onTx(lv_event_t* e) {
    confirm("Transmit to the satellite", "The tracker transmits now (only with Allow transmitting in its WebUI and while the satellite is in view).", "TRANSMIT", doTx,
            lv_event_get_user_data(e), true);
}
static void onSrc(lv_event_t* e) { send(proto::Cmd("tgt").num("src", (int)(intptr_t)lv_event_get_user_data(e)).str()); }
static void onTgt(lv_event_t* e) { send(proto::Cmd("tgt").num("sel", (int)(intptr_t)lv_event_get_user_data(e)).str()); }
static void onPredict(lv_event_t*) { send(proto::Cmd("tgt").flag("pr", !M->targets.pr).str()); }

static void buildSat(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Satellites", &tr);
    sFav = chip(tr, "FAVOURITE", false, onFav, nullptr, 42, P->amber);
    lv_obj_t* body = row(root, GAP);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_height(body, 10);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* left = panel(body, 12);
    lv_obj_set_size(left, 400, LV_PCT(100));
    lv_obj_set_scrollable(left, true);
    lv_obj_set_style_pad_row(left, 4, 0);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    sName = text(left, "-", &app_bold_26, P->text);
    oneLine(sName);
    lv_obj_set_width(sName, LV_PCT(100));
    sComp = compass(left, 210);
    lv_obj_t* ae = row(left, 20);
    lv_obj_set_flex_align(ae, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_t* a1 = col(ae, 0); lv_obj_set_width(a1, LV_SIZE_CONTENT); section(a1, "AZ"); sAz = text(a1, "-", &app_mono_40, P->amber);
    lv_obj_t* a2 = col(ae, 0); lv_obj_set_width(a2, LV_SIZE_CONTENT); section(a2, "EL"); sEl = text(a2, "-", &app_mono_40, P->text);
    sPass = wrap(left, "", &app_text_18, P->subText);
    lv_obj_t* right = col(body, GAP);
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_width(right, 10);
    lv_obj_set_height(right, LV_PCT(100));
    sNotSat = wrap(right, "", &app_text_18, P->amber);
    lv_obj_t* cr = row(right, 8);
    static const char* tn[] = {"LIST", "RECEIVER", "TRANSMIT", "ROTOR"};
    for (int i = 0; i < 4; i++) { sTabs[i] = chip(cr, tn[i], i == sTab, onTab, (void*)(intptr_t)i, 42); lv_obj_set_flex_grow(sTabs[i], 1); }
    for (int i = 0; i < 4; i++) {
        sPages[i] = panel(right, 12);
        lv_obj_set_flex_grow(sPages[i], 1);
        lv_obj_set_height(sPages[i], 10);
        lv_obj_set_scrollable(sPages[i], true);
        lv_obj_set_style_pad_row(sPages[i], 8, 0);
    }
    // lista
    sCatRow = row(sPages[0], 6);
    lv_obj_set_flex_flow(sCatRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(sCatRow, 6, 0);
    static const char* cats[] = {"ALL", "70 cm", "LoRa", "GEO", "FAVS", "2 m", "THIS RADIO"};
    for (int i = 0; i < 7; i++) chip(sCatRow, cats[i], false, onCat, (void*)(intptr_t)i, 38);
    sList.create(sPages[0], makeSatRow, onSatClick);
    button(sPages[0], "MORE", onMore, nullptr, 0, 46);
    // receptor
    text(sPages[1], "Transmitter to listen to", &app_cond_18, P->muted);
    sTxRow = row(sPages[1], 6);
    lv_obj_set_flex_flow(sTxRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(sTxRow, 6, 0);
    lv_obj_t* rr = row(sPages[1], 8);
    sRxBtn = button(rr, "LISTEN", onRx, nullptr, 0, 52, P->green);
    button(rr, "-500", onFine, (void*)(intptr_t)-500, 0, 52);
    button(rr, "-100", onFine, (void*)(intptr_t)-100, 0, 52);
    button(rr, "+100", onFine, (void*)(intptr_t)100, 0, 52);
    button(rr, "+500", onFine, (void*)(intptr_t)500, 0, 52);
    sRxInfo = text(sPages[1], "", &app_mono_20, P->subText);
    sRxLog = text(sPages[1], "", &app_mono_20, P->green);
    lv_obj_set_width(sRxLog, LV_PCT(100));
    lv_label_set_long_mode(sRxLog, LV_LABEL_LONG_MODE_WRAP);
    // emision
    text(sPages[2], "Transmitter", &app_cond_18, P->muted);
    sTxRow2 = row(sPages[2], 6);
    lv_obj_set_flex_flow(sTxRow2, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(sTxRow2, 6, 0);
    sTxText = field(sPages[2], "Message (64; empty = the one in the WebUI)", 0, 64);
    lv_obj_set_width(sTxText, LV_PCT(100));
    lv_obj_t* xr = row(sPages[2], 8);
    button(xr, "TRANSMIT", onTx, nullptr, 0, 52, P->red);
    button(xr, "APRS POSITION", onTx, (void*)(intptr_t)1, 0, 52, P->amber);
    wrap(sPages[2], "Transmission needs 'Allow transmitting' in the tracker's WebUI and the satellite in view. APRS POSITION: through an APRS digipeater (ISS), T-TWR.", &app_text_18, P->muted);
    // rotor del tracker
    text(sPages[3], "What the tracker's rotor follows", &app_cond_18, P->muted);
    sSrcRow = row(sPages[3], 6);
    lv_obj_set_flex_flow(sSrcRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(sSrcRow, 6, 0);
    static const char* srcs[] = {"SATELLITE", "APRS COMPASS", "FIXED TARGET", "LAST APRS HEARD", "CW BEACON"};
    static const int srcIds[] = {0, 1, 2, 4, 5};
    for (int i = 0; i < 5; i++) chip(sSrcRow, srcs[i], false, onSrc, (void*)(intptr_t)srcIds[i], 40, P->amber);
    lv_obj_t* pr = row(sPages[3], 8);
    chip(pr, "ESTIMATE MOVING STATIONS", false, onPredict, nullptr, 40);
    sRotInfo = wrap(sPages[3], "", &app_text_20, P->subText);
    text(sPages[3], "Fixed targets", &app_cond_18, P->muted);
    sTgtRow = row(sPages[3], 6);
    lv_obj_set_flex_flow(sTgtRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(sTgtRow, 6, 0);
}

static std::string clock(uint32_t ep) {
    if (!ep) return "-";
    char b[16];
    snprintf(b, sizeof b, "%02u:%02u:%02u", (unsigned)(ep / 3600 % 24), (unsigned)(ep / 60 % 60), (unsigned)(ep % 60));
    return b;
}
static void refreshSat() {
    const proto::Sat& s = M->sat;
    char b[200];
    show(sNotSat, M->status.mode != 9 && !(M->info.capSatFm && M->status.mode == 11));
    setText(sNotSat, "The tracker is not in SAT mode: the pass data, the receiver and transmission need it (Tracker > Mode > SAT).");
    setText(sName, s.ok ? s.name.c_str() : "No satellite");
    chipSet(sFav, s.fav, P->amber);
    float az = s.az, rot = RM.pos.az;
    const bool rotOk = RM.pos.ok && net::rotorState() == net::Link::Linked;
    sComp->set(0, s.ok ? &az : nullptr, rotOk ? &rot : (M->rot.ok ? &M->rot.az : nullptr), nullptr);
    if (s.ok) { setTextf(sAz, "%.1f\xC2\xB0", s.az); setTextf(sEl, "%.1f\xC2\xB0", s.el); setColor(sEl, s.el > 0 ? P->green : P->text); }
    else { setText(sAz, "-"); setText(sEl, "-"); }
    if (s.ok) {
        snprintf(b, sizeof b, "%s\nAOS %s \xC2\xB7 LOS %s UTC \xC2\xB7 max %d\xC2\xB0\nRange %.0f km \xC2\xB7 %+.2f km/s \xC2\xB7 %d transmitters%s%s",
                 s.geo ? "Geostationary" : s.el > 0 ? "IN VIEW" : "Next pass", clock(s.aos).c_str(), clock(s.los).c_str(), s.maxEl, s.rng, s.rr, s.tx,
                 s.stamp.empty() ? "" : "\nData ", s.stamp.c_str());
        setText(sPass, b);
    } else setText(sPass, linked() ? "Choose a satellite in the list." : "Tracker not linked.");
    for (int i = 0; i < 4; i++) { chipSet(sTabs[i], i == sTab); show(sPages[i], i == sTab); }
    if (sTab == 0) {
        for (uint32_t i = 0; i < lv_obj_get_child_count(sCatRow); i++) chipSet(lv_obj_get_child(sCatRow, i), (int)i == s.cat);
        sList.begin();
        for (const proto::SatEntry& e : M->satList) {
            lv_obj_t* r = sList.next();
            setText(lv_obj_get_child(r, 0), e.fav ? "*" : "");
            setText(lv_obj_get_child(r, 1), e.name.c_str());
            setColor(lv_obj_get_child(r, 1), e.i == M->satListSel ? P->amber : e.in ? P->text : P->faint);
            snprintf(b, sizeof b, "%s \xC2\xB7 %d tx \xC2\xB7 %d", e.c.c_str(), e.t, e.id);
            setText(lv_obj_get_child(r, 2), b);
        }
        sList.end();
    }
    // transmisores (receptor y emision)
    std::string ts;
    for (const auto& t : M->satTxs) ts += std::to_string(t.i) + t.d + ";";
    ts += std::to_string(sRxTx) + "/" + std::to_string(sTxTx);
    if (ts != txShown) {
        txShown = ts;
        lv_obj_clean(sTxRow);
        lv_obj_clean(sTxRow2);
        for (const auto& t : M->satTxs) {
            snprintf(b, sizeof b, "%s %s %s", t.rx.c_str(), proto::mhz(t.dn).c_str(), t.d.substr(0, 18).c_str());
            if (t.rx != "n/a") chip(sTxRow, b, t.i == sRxTx, onRxTx, (void*)(intptr_t)t.i, 40);
            snprintf(b, sizeof b, "%s %s %s", t.tx.c_str(), t.up ? proto::mhz(t.up).c_str() : "-", t.aprs ? "APRS" : t.d.substr(0, 14).c_str());
            if (t.tx != "n/a") chip(sTxRow2, b, t.i == sTxTx, onTxTx, (void*)(intptr_t)t.i, 40, P->amber);
        }
        if (M->satTxs.empty()) { text(sTxRow, "No transmitters known for this satellite.", &app_text_18, P->muted); }
    }
    const proto::SatRx& rx = M->satRx;
    setText(lv_obj_get_child(sRxBtn, 0), rx.on ? "STOP" : "LISTEN");
    if (rx.on) snprintf(b, sizeof b, "%s %s MHz \xC2\xB7 Doppler %+d Hz \xC2\xB7 fine %+d \xC2\xB7 %d dBm \xC2\xB7 %d frames", rx.k.c_str(), proto::mhz(rx.f).c_str(), rx.dop, rx.fine, rx.rssi, rx.n);
    else snprintf(b, sizeof b, "Receiver off");
    setText(sRxInfo, b);
    std::string log;
    for (size_t i = rx.lines.size() > 8 ? rx.lines.size() - 8 : 0; i < rx.lines.size(); i++) log += rx.lines[i] + "\n";
    setText(sRxLog, log.c_str());
    // rotor del tracker
    static const int srcIds[] = {0, 1, 2, 4, 5};
    for (int i = 0; i < 5; i++) chipSet(lv_obj_get_child(sSrcRow, i), M->targets.ok && M->targets.src == srcIds[i], P->amber);
    lv_obj_t* prc = lv_obj_get_child(lv_obj_get_child(sPages[3], 2), 0);
    chipSet(prc, M->targets.pr);
    if (M->rot.ok) {
        static const char* sn[] = {"satellite", "APRS compass station", "fixed target", "external program", "last APRS heard", "last CW beacon"};
        snprintf(b, sizeof b, "Now: %s \xC2\xB7 %s \xC2\xB7 AZ %.0f\xC2\xB0 EL %.0f\xC2\xB0%s%s", M->rot.src >= 0 && M->rot.src < 6 ? sn[M->rot.src] : "?", M->rot.name.c_str(),
                 M->rot.az, M->rot.el, M->rot.trk ? " \xC2\xB7 tracking" : "", M->targets.ff.empty() ? "" : ("\nFilter " + M->targets.ff).c_str());
        setText(sRotInfo, b);
    } else setText(sRotInfo, "The tracker sends its rotor aim with 'Send the rot event' (WebUI), in SAT mode or with a terrestrial source.");
    std::string tg;
    for (const auto& t : M->targets.items) tg += std::to_string(t.i) + t.n + ";";
    tg += std::to_string(M->targets.sel);
    if (tg != tgtShown) {
        tgtShown = tg;
        lv_obj_clean(sTgtRow);
        for (const auto& t : M->targets.items) chip(sTgtRow, t.n.c_str(), t.i == M->targets.sel, onTgt, (void*)(intptr_t)t.i, 40);
        if (M->targets.items.empty()) text(sTgtRow, "None (they are added in the tracker's WebUI or the app).", &app_text_18, P->muted);
    }
}

void initSat() { registerScreen(SAT, {"Satellites", true, buildSat, refreshSat, requestSat, nullptr}); }

} // namespace r7
#endif
