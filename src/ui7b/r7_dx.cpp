/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * DX Cluster (DxClusterScreen.kt de la app) por telnet con la WiFi de la pantalla: spots en directo con filtros de origen
 * (persona, RBN CW, RBN digital), modo, banda y actividad, consola para ordenes del nodo y TUNE a la emisora de la caja del
 * rotor (CAT). Tambien reparte lo que llega de la caja del rotor.
 */
#ifdef UI_RADIO
#include "r7.h"
#include "net.h"
#include "settings.h"
#include <cstdio>
#include <cstring>

namespace r7 {

// lo que llega de la caja del rotor y del DX Cluster (se llama desde update())
void pumpLinks() {
    std::string l;
    int n = 0;
    while (n++ < 20 && net::rotorPoll(l)) rotp::apply(RM, l.c_str());
    n = 0;
    while (n++ < 30 && net::dxPoll(l)) {
        dx::Spot s;
        if (dx::parse(l, s)) {
            s.atMs = nowMs();
            DX.spots.insert(DX.spots.begin(), s);
            if (DX.spots.size() > 200) DX.spots.pop_back();
        } else if (!l.empty()) {
            DX.console.push_back(l);
            if (DX.console.size() > 40) DX.console.erase(DX.console.begin());
        }
        DX.seq++;
    }
}

static lv_obj_t *xState, *xConn, *xF[6], *xBands, *xCons, *xCmd, *xDetail, *xDetTxt, *xEmpty;
static List xList;
static std::vector<int> xView;
static int xSel = -1;
static const char* BANDS[] = {"ALL", "160m", "80m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m", "2m"};
static std::string bandsShown;

static void onConn(lv_event_t*) {
    if (net::dxState() == net::Link::Linked || net::dxState() == net::Link::Connecting) { net::dxDisconnect(); return; }
    if (!net::wifi().connected) { error("The DX cluster needs WiFi (Settings > WiFi)"); return; }
    if (cfg::get().dxCall.empty() && myCall().empty()) { error("Set the login callsign (Settings > DX cluster)"); return; }
    if (cfg::get().dxCall.empty()) {
        std::string c = myCall();
        size_t d = c.find('-');
        cfg::get().dxCall = d == std::string::npos ? c : c.substr(0, d);             // el indicativo base, sin SSID
    }
    net::dxConnect();
}
static void onFilt(lv_event_t* e) {
    cfg::Settings& s = cfg::get();
    switch ((int)(intptr_t)lv_event_get_user_data(e)) {
        case 0: s.dxHuman = !s.dxHuman; break;
        case 1: s.dxRbnCw = !s.dxRbnCw; break;
        case 2: s.dxRbnDig = !s.dxRbnDig; break;
        case 3: s.dxCw = !s.dxCw; break;
        case 4: s.dxPho = !s.dxPho; break;
        case 5: s.dxDig = !s.dxDig; break;
    }
    cfg::save();
    requestRefresh();
}
static void onBand(lv_event_t* e) { int i = (int)(intptr_t)lv_event_get_user_data(e); cfg::get().dxBand = i ? BANDS[i] : ""; cfg::save(); requestRefresh(); }
static void onCmd(lv_event_t*) {
    std::string c = lv_textarea_get_text(xCmd);
    if (c.empty()) return;
    net::dxSend(c);
    DX.console.push_back("> " + c);
    lv_textarea_set_text(xCmd, "");
    DX.seq++;
}
static void onCmdReady(lv_event_t* e) { if (lv_event_get_code(e) == LV_EVENT_READY) onCmd(e); }
static void onShDx(lv_event_t*) { net::dxSend("sh/dx 30"); }
static lv_obj_t* makeSpot(lv_obj_t* p) {
    lv_obj_t* r = row(p, 12);
    lv_obj_set_height(r, 46);
    lv_obj_set_style_pad_hor(r, 10, 0);
    lv_obj_set_style_radius(r, 8, 0);
    lv_obj_set_style_bg_color(r, C(P->panel), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, C(P->selected), LV_STATE_PRESSED);
    lv_obj_t* t = text(r, "", &app_mono_20, P->muted); lv_obj_set_width(t, 64);
    lv_obj_t* f = text(r, "", &app_mono_20, P->cyan); lv_obj_set_width(f, 110); lv_obj_set_style_text_align(f, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t* c = text(r, "", &app_bold_22, P->text); lv_obj_set_width(c, 140);
    lv_obj_t* m = text(r, "", &app_bold_18, P->amber); lv_obj_set_width(m, 52);
    lv_obj_t* k = text(r, "", &app_text_18, P->subText); lv_obj_set_flex_grow(k, 1); lv_obj_set_width(k, 10); oneLine(k);
    lv_obj_t* s = text(r, "", &app_cond_16, P->muted); lv_obj_set_width(s, 110); lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_RIGHT, 0);
    return r;
}
static void onSpot(lv_event_t* e) { int i = clickIndex(e); if (i >= 0 && i < (int)xView.size()) { xSel = xView[i]; requestRefresh(); } }
static void onTune(lv_event_t*) {
    if (xSel < 0 || xSel >= (int)DX.spots.size()) return;
    if (net::rotorState() != net::Link::Linked || !RM.info.cat) { error("TUNE sends the frequency to the radio of the rotor box (CAT): link it first"); return; }
    const dx::Spot& s = DX.spots[xSel];
    net::rotorSend(rotp::catSet("f", s.khz * 1000.0));
    const char* m = s.mode == "CW" ? "CW" : s.mode == "DIG" ? "PKTUSB" : s.khz < 10000 ? "LSB" : "USB";
    net::rotorSend(rotp::catSetText("m", m));
    notice("Radio tuned");
}
static void onCloseDet(lv_event_t*) { xSel = -1; requestRefresh(); }

static void buildDx(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "DX cluster", &tr);
    xState = text(tr, "", &app_cond_18, P->muted);
    xConn = button(tr, "CONNECT", onConn, nullptr, 0, 48, P->green);
    button(tr, "SH/DX", onShDx, nullptr, 0, 48);
    lv_obj_t* fr = row(root, 6);
    static const char* fn[] = {"HUMAN", "RBN CW", "RBN DIG", "CW", "PHONE", "DIGITAL"};
    for (int i = 0; i < 6; i++) xF[i] = chip(fr, fn[i], true, onFilt, (void*)(intptr_t)i, 38, i < 3 ? P->cyan : P->amber);
    xBands = row(root, 4);
    for (int i = 0; i < 12; i++) { lv_obj_t* c = chip(xBands, BANDS[i], i == 0, onBand, (void*)(intptr_t)i, 36); lv_obj_set_style_pad_hor(c, 10, 0); }
    lv_obj_t* body = row(root, GAP);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_height(body, 10);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* left = col(body, 4);
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_width(left, 10);
    lv_obj_set_height(left, LV_PCT(100));
    lv_obj_set_scrollable(left, true);
    xEmpty = wrap(left, "", &app_text_20, P->muted);
    xList.create(left, makeSpot, onSpot);
    lv_obj_t* right = col(body, GAP);
    lv_obj_set_size(right, 300, LV_PCT(100));
    xDetail = panel(right, 12);
    xDetTxt = wrap(xDetail, "", &app_text_20, P->text);
    lv_obj_t* dr = row(xDetail, 8);
    button(dr, "TUNE", onTune, nullptr, 0, 48, P->cyan);
    button(dr, "CLOSE", onCloseDet, nullptr, 0, 48);
    lv_obj_t* cp = panel(right, 10);
    lv_obj_set_flex_grow(cp, 1);
    lv_obj_set_height(cp, 10);
    lv_obj_set_scrollable(cp, true);
    section(cp, "CONSOLE");
    xCons = text(cp, "", &app_mono_20, P->subText);
    lv_obj_set_width(xCons, LV_PCT(100));
    lv_label_set_long_mode(xCons, LV_LABEL_LONG_MODE_WRAP);
    xCmd = field(right, "Command (sh/dx, set/...)", 0, 60);
    lv_obj_set_width(xCmd, LV_PCT(100));
    lv_obj_add_event_cb(xCmd, onCmdReady, LV_EVENT_READY, nullptr);
}
static void refreshDx() {
    const cfg::Settings& s = cfg::get();
    net::Link st = net::dxState();
    setTextf(xState, "%s \xC2\xB7 %s:%d", net::dxStateText(), s.dxHost.c_str(), s.dxPort);
    setText(lv_obj_get_child(xConn, 0), st == net::Link::Off || st == net::Link::Failed ? "CONNECT" : "DISCONNECT");
    buttonColor(xConn, st == net::Link::Off || st == net::Link::Failed ? P->green : P->amber);
    bool fs[6] = {s.dxHuman, s.dxRbnCw, s.dxRbnDig, s.dxCw, s.dxPho, s.dxDig};
    for (int i = 0; i < 6; i++) chipSet(xF[i], fs[i], i < 3 ? P->cyan : P->amber);
    for (int i = 0; i < 12; i++) chipSet(lv_obj_get_child(xBands, i), i == 0 ? s.dxBand.empty() : s.dxBand == BANDS[i]);
    dx::Filter f;
    f.human = s.dxHuman; f.rbnCw = s.dxRbnCw; f.rbnDig = s.dxRbnDig; f.cw = s.dxCw; f.pho = s.dxPho; f.dig = s.dxDig; f.band = s.dxBand;
    xView.clear();
    for (size_t i = 0; i < DX.spots.size() && xView.size() < 80; i++) if (dx::pass(f, DX.spots[i])) xView.push_back((int)i);
    show(xEmpty, xView.empty());
    setText(xEmpty, st == net::Link::Linked ? "No spots pass the filters yet." : "Connect to a DX cluster node (WiFi needed). Default node: EA4URE.");
    xList.begin();
    char b[24];
    for (int i : xView) {
        const dx::Spot& sp = DX.spots[i];
        lv_obj_t* r = xList.next();
        setText(lv_obj_get_child(r, 0), sp.time.c_str());
        snprintf(b, sizeof b, "%.1f", sp.khz);
        setText(lv_obj_get_child(r, 1), b);
        setText(lv_obj_get_child(r, 2), sp.call.c_str());
        setText(lv_obj_get_child(r, 3), sp.mode.c_str());
        setColor(lv_obj_get_child(r, 3), sp.mode == "CW" ? P->amber : sp.mode == "DIG" ? P->green : P->cyan);
        setText(lv_obj_get_child(r, 4), sp.comment.c_str());
        setText(lv_obj_get_child(r, 5), sp.spotter.c_str());
        lv_obj_set_style_border_width(r, i == xSel ? 2 : 0, 0);
        lv_obj_set_style_border_color(r, C(P->amber), 0);
    }
    xList.end();
    show(xDetail, xSel >= 0 && xSel < (int)DX.spots.size());
    if (xSel >= 0 && xSel < (int)DX.spots.size()) {
        const dx::Spot& sp = DX.spots[xSel];
        char d[256];
        snprintf(d, sizeof d, "%s\n%.1f kHz \xC2\xB7 %s \xC2\xB7 %s\n%s\nby %s%s \xC2\xB7 %s", sp.call.c_str(), sp.khz, sp.band.c_str(), sp.mode.c_str(), sp.comment.c_str(),
                 sp.spotter.c_str(), sp.skimmer ? " (RBN)" : "", sp.time.c_str());
        setText(xDetTxt, d);
    }
    std::string c;
    for (size_t i = DX.console.size() > 14 ? DX.console.size() - 14 : 0; i < DX.console.size(); i++) c += DX.console[i] + "\n";
    setText(xCons, c.c_str());
}

void initDx() { registerScreen(DXC, {"DX cluster", false, buildDx, refreshDx, nullptr, nullptr}); }

} // namespace r7
#endif
