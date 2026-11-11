/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ROTOR (RotorTab.kt de la app): la caja APRS LoRa+ Rotor por WiFi (TCP 4534) o BLE. Brujula con posicion y objetivo,
 * ir a, mover manteniendo pulsado, STOP, park, borrar fallo, seguir (punteria del tracker, una estacion o la ultima oida),
 * CAT de la emisora de la caja con el Doppler del satelite del tracker, enlace y PIN, calibracion y datos de la caja.
 */
#ifdef UI_RADIO
#include "r7.h"
#include "net.h"
#include "settings.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace r7 {

static bool rotOn() { return net::rotorState() == net::Link::Linked; }
static void rsend(const std::string& l) {
    if (!rotOn()) { error("The rotor box is not linked (Rotor > SETUP)"); return; }
    net::rotorSend(l);
}

// ------------------------------------------------------------------ seguir y Doppler (los llama update())
static float lastAz = -999, lastEl = -999;
static uint32_t lastSent = 0;
void rotorFollowStep() {
    const cfg::Settings& s = cfg::get();
    if (!s.rotorFollow || !rotOn() || !RM.info.ok) return;
    const uint32_t t = nowMs();
    if (t - lastSent < 1000) return;
    float az = 0, el = 0;
    bool have = false;
    double la, lo;
    if (s.rotorFollow == 1 && M->rot.ok && M->rot.trk) { az = M->rot.az; el = M->rot.el; have = true; }
    else if ((s.rotorFollow == 2 || s.rotorFollow == 3) && ownPos(la, lo)) {
        for (const auto& h : M->heard) {
            if (!h.hasPos) continue;
            if (s.rotorFollow == 2 && h.call != s.rotorFollowCall) continue;
            az = (float)proto::bearingDeg(la, lo, h.lat, h.lon);
            el = 0;
            have = true;
            break;                                                  // la lista va de la mas reciente a la mas antigua
        }
    }
    if (!have) return;
    if (fabsf(az - lastAz) < s.rotorStep && fabsf(el - lastEl) < s.rotorStep) return;
    lastAz = az; lastEl = el; lastSent = t;
    net::rotorSend(rotp::goTo(az, el < 0 ? 0 : el, RM.info.hasEl));
}

static std::string dopName;
static uint32_t dopSent = 0;
static int dopTx = -1;
void catDopplerStep() {
    if (!cfg::get().catDoppler || !rotOn() || !RM.info.cat) return;
    if (M->status.mode != 9 || !M->sat.ok || M->satTxs.empty()) {
        if (!dopName.empty()) { net::rotorSend(rotp::catTrack(false, "", 0, 0, 0)); dopName.clear(); }
        return;
    }
    const uint32_t t = nowMs();
    if (t - dopSent < 1500) return;
    dopSent = t;
    int txi = M->satRx.on ? M->satRx.tx : 0;
    const proto::SatTx* tx = nullptr;
    for (const auto& x : M->satTxs) if (x.i == txi) tx = &x;
    if (!tx) tx = &M->satTxs[0];
    if (M->sat.name != dopName || tx->i != dopTx) {                    // otro satelite o transmisor: modo, split y tono
        dopName = M->sat.name; dopTx = tx->i;
        net::rotorSend(rotp::catSetText("m", rotp::satMode(tx->m.empty() ? tx->rx : tx->m)));
        net::rotorSend(rotp::catSetFlag("split", tx->up != 0));
        net::rotorSend(rotp::catSet("tone", tx->tone > 0 ? tx->tone : 0));
    }
    net::rotorSend(rotp::catTrack(true, M->sat.name, tx->dn, tx->up, M->sat.rr));
}

// ------------------------------------------------------------------ pantalla
static Compass* rComp;
static lv_obj_t *rState, *rAz, *rEl, *rFault, *rWind, *rTabs[4], *rPages[4], *rAzF, *rElF, *rFollowC[4], *rFollowCall, *rFollowInfo, *rStepL;
static lv_obj_t *rCatF, *rCatInfo, *rModeC[6], *rVfoA, *rVfoB, *rSplit, *rPtt, *rDop, *rLinkC[3], *rHost, *rBle, *rPin;
static SMeter rSm;
static Rows rInfo, rRaw;
static int rTab = 0;
static uint32_t catStep = 1000;

static void onRTab(lv_event_t* e) { rTab = (int)(intptr_t)lv_event_get_user_data(e); if (rTab == 3 && rotOn()) net::rotorSend(rotp::cmd("raw")); requestRefresh(); }
static void onGo(lv_event_t*) {
    const char* a = lv_textarea_get_text(rAzF);
    if (!*a) { error("Write the azimuth"); return; }
    rsend(rotp::goTo((float)atof(a), (float)atof(lv_textarea_get_text(rElF)), RM.info.hasEl && *lv_textarea_get_text(rElF)));
}
static void onPreset(lv_event_t* e) { rsend(rotp::goTo((float)(intptr_t)lv_event_get_user_data(e), RM.pos.el, false)); }
static void onMove(lv_event_t* e) {
    const char* d = (const char*)lv_event_get_user_data(e);
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED) rsend(rotp::move(d));
    else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) rsend(rotp::cmd("stop"));
}
static void onCmd(lv_event_t* e) { rsend(rotp::cmd((const char*)lv_event_get_user_data(e))); }
static void onFollowMode(lv_event_t* e) {
    cfg::get().rotorFollow = (int)(intptr_t)lv_event_get_user_data(e);
    if (cfg::get().rotorFollow == 2) {
        std::string c = upper(lv_textarea_get_text(rFollowCall));
        if (!c.empty()) cfg::get().rotorFollowCall = c;
    }
    cfg::save();
    lastAz = lastEl = -999;
    requestRefresh();
}
static void onStep(lv_event_t* e) {
    float& s = cfg::get().rotorStep;
    s += (int)(intptr_t)lv_event_get_user_data(e) > 0 ? 1.0f : -1.0f;
    if (s < 1) s = 1;
    if (s > 15) s = 15;
    cfg::save();
    requestRefresh();
}
static void onCatStep(lv_event_t* e) {
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    if (d == 0) { catStep = catStep >= 100000 ? 10 : catStep * 10; requestRefresh(); return; }
    if (RM.cat.f) rsend(rotp::catSet("f", (double)RM.cat.f + d * (double)catStep));
}
static void onCatFreq(lv_event_t*) {
    double mhz = atof(lv_textarea_get_text(rCatF));
    if (mhz <= 0) return;
    rsend(rotp::catSet("f", mhz * 1e6));
    lv_textarea_set_text(rCatF, "");
}
static const char* MODES[6] = {"USB", "LSB", "CW", "FM", "AM", "PKTUSB"};
static void onCatMode(lv_event_t* e) { rsend(rotp::catSetText("m", MODES[(int)(intptr_t)lv_event_get_user_data(e)])); }
static void onVfo(lv_event_t* e) { rsend(rotp::catSetText("vfo", (const char*)lv_event_get_user_data(e))); }
static void onSplit(lv_event_t*) { rsend(rotp::catSetFlag("split", !RM.cat.split)); }
static void onPtt(lv_event_t* e) {
    lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_PRESSED) rsend(rotp::catSetFlag("ptt", true));
    else if (c == LV_EVENT_RELEASED || c == LV_EVENT_PRESS_LOST) rsend(rotp::catSetFlag("ptt", false));
}
static void onDop(lv_event_t*) { cfg::get().catDoppler = !cfg::get().catDoppler; cfg::save(); if (!cfg::get().catDoppler && rotOn()) net::rotorSend(rotp::catTrack(false, "", 0, 0, 0)); requestRefresh(); }
static void onLinkType(lv_event_t* e) { cfg::get().rotorLink = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static void onSaveLink(lv_event_t*) {
    cfg::Settings& s = cfg::get();
    s.rotorHost = lv_textarea_get_text(rHost);
    s.rotorBle = lv_textarea_get_text(rBle);
    s.rotorPin = lv_textarea_get_text(rPin);
    cfg::save();
    net::rotorApply();
    notice(s.rotorLink == 0 ? "Rotor box link off" : "Connecting to the rotor box...");
}
static void onCal(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    float deg = (float)atof(lv_textarea_get_text(rAzF));
    if (i == 0) rsend(rotp::cal("az", "a", deg));
    else if (i == 1) rsend(rotp::cal("az", "b", deg));
    else rsend(rotp::here("az", deg));
    net::rotorSend(rotp::cmd("raw"));
}
static void doBoxReboot(void*) { rsend(rotp::cmd("reboot")); }
static void onBoxReboot(lv_event_t*) { confirm("Restart the rotor box", "The box restarts and links again.", "RESTART", doBoxReboot, nullptr, true); }

static lv_obj_t* holdKey(lv_obj_t* p, const char* label, const char* dir) {
    Key k = key(p, label, nullptr, 10, 70, nullptr, nullptr, &app_bold_26);
    lv_obj_set_flex_grow(k.obj, 1);
    lv_obj_add_event_cb(k.obj, onMove, LV_EVENT_PRESSED, (void*)dir);
    lv_obj_add_event_cb(k.obj, onMove, LV_EVENT_RELEASED, (void*)dir);
    lv_obj_add_event_cb(k.obj, onMove, LV_EVENT_PRESS_LOST, (void*)dir);
    return k.obj;
}

static void buildRotor(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Rotor box", &tr);
    rState = text(tr, "", &app_cond_18, P->muted);
    lv_obj_t* body = row(root, GAP);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_height(body, 10);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* left = panel(body, 12);
    lv_obj_set_size(left, 360, LV_PCT(100));
    lv_obj_set_scrollable(left, true);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    rComp = compass(left, 230);
    lv_obj_t* ae = row(left, 20);
    lv_obj_set_flex_align(ae, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_t* a1 = col(ae, 0); lv_obj_set_width(a1, LV_SIZE_CONTENT); section(a1, "AZ"); rAz = text(a1, "-", &app_mono_40, P->cyan);
    lv_obj_t* a2 = col(ae, 0); lv_obj_set_width(a2, LV_SIZE_CONTENT); section(a2, "EL"); rEl = text(a2, "-", &app_mono_40, P->text);
    rFault = text(left, "", &app_bold_22, P->green);
    rWind = wrap(left, "", &app_text_18, P->subText);
    lv_obj_t* right = col(body, GAP);
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_width(right, 10);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_t* cr = row(right, 8);
    static const char* tn[] = {"CONTROL", "FOLLOW", "RADIO (CAT)", "SETUP"};
    for (int i = 0; i < 4; i++) { rTabs[i] = chip(cr, tn[i], i == rTab, onRTab, (void*)(intptr_t)i, 42); lv_obj_set_flex_grow(rTabs[i], 1); }
    for (int i = 0; i < 4; i++) {
        rPages[i] = panel(right, 12);
        lv_obj_set_flex_grow(rPages[i], 1);
        lv_obj_set_height(rPages[i], 10);
        lv_obj_set_scrollable(rPages[i], true);
        lv_obj_set_style_pad_row(rPages[i], 10, 0);
    }
    // control
    lv_obj_t* g = row(rPages[0], 8);
    rAzF = field(g, "AZ", 130, 6, "0123456789.");
    rElF = field(g, "EL", 110, 5, "0123456789.");
    button(g, "GO TO", onGo, nullptr, 0, 52, P->amber);
    button(g, "STOP", onCmd, (void*)"stop", 0, 52, P->red);
    lv_obj_t* pr = row(rPages[0], 6);
    static const int presets[] = {0, 45, 90, 135, 180, 225, 270, 315};
    for (int a : presets) { char b[8]; snprintf(b, sizeof b, "%d", a); lv_obj_t* k = button(pr, b, onPreset, (void*)(intptr_t)a, 0, 46); lv_obj_set_flex_grow(k, 1); lv_obj_set_style_min_width(k, 60, 0); }
    lv_obj_t* mv = row(rPages[0], 8);
    holdKey(mv, "\xE2\x80\xB9 CCW", "ccw");
    holdKey(mv, "CW \xE2\x80\xBA", "cw");
    holdKey(mv, "UP", "up");
    holdKey(mv, "DOWN", "down");
    lv_obj_t* ot = row(rPages[0], 8);
    button(ot, "PARK", onCmd, (void*)"park", 0, 52);
    button(ot, "CLEAR FAULT", onCmd, (void*)"clear", 0, 52);
    wrap(rPages[0], "Hold the arrows to move; it stops when you let go.", &app_text_18, P->muted);
    // seguir
    lv_obj_t* fr = row(rPages[1], 8);
    lv_obj_set_flex_flow(fr, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(fr, 8, 0);
    static const char* fn[] = {"OFF", "TRACKER AIM", "ONE STATION", "LAST HEARD"};
    for (int i = 0; i < 4; i++) rFollowC[i] = chip(fr, fn[i], false, onFollowMode, (void*)(intptr_t)i, 44, P->amber);
    lv_obj_t* fc = row(rPages[1], 8);
    rFollowCall = field(fc, "Station to follow (EA7XXX-9)", 320, 9, nullptr, true);
    lv_obj_t* st = row(rPages[1], 8);
    text(st, "Minimum step", &app_cond_18, P->muted);
    button(st, "-", onStep, (void*)(intptr_t)-1, 60, 46);
    rStepL = text(st, "", &app_mono_20, P->text);
    button(st, "+", onStep, (void*)(intptr_t)1, 60, 46);
    rFollowInfo = wrap(rPages[1], "", &app_text_20, P->subText);
    // CAT
    lv_obj_t* c1 = row(rPages[2], 12);
    rCatInfo = text(c1, "", &app_mono_40, P->subText);
    lv_obj_t* sp = box(c1);
    lv_obj_set_flex_grow(sp, 1);
    rCatF = field(c1, "MHz", 170, 12, "0123456789.");
    button(c1, "TUNE", onCatFreq, nullptr, 0, 52, P->cyan);
    rSm = smeter(rPages[2], 560);
    lv_obj_t* c2 = row(rPages[2], 6);
    button(c2, "\xE2\x80\xB9\xE2\x80\xB9", onCatStep, (void*)(intptr_t)-10, 70, 50);
    button(c2, "\xE2\x80\xB9", onCatStep, (void*)(intptr_t)-1, 70, 50);
    lv_obj_t* stp = button(c2, "STEP", onCatStep, (void*)(intptr_t)0, 150, 50, P->cyan);
    (void)stp;
    button(c2, "\xE2\x80\xBA", onCatStep, (void*)(intptr_t)1, 70, 50);
    button(c2, "\xE2\x80\xBA\xE2\x80\xBA", onCatStep, (void*)(intptr_t)10, 70, 50);
    lv_obj_t* c3 = row(rPages[2], 6);
    for (int i = 0; i < 6; i++) rModeC[i] = chip(c3, MODES[i], false, onCatMode, (void*)(intptr_t)i, 42);
    lv_obj_t* c4 = row(rPages[2], 8);
    rVfoA = chip(c4, "VFO A", false, onVfo, (void*)"A", 42);
    rVfoB = chip(c4, "VFO B", false, onVfo, (void*)"B", 42);
    rSplit = chip(c4, "SPLIT", false, onSplit, nullptr, 42, P->amber);
    rDop = chip(c4, "SAT DOPPLER", false, onDop, nullptr, 42, P->green);
    Key pk = key(c4, "PTT", nullptr, 120, 52, nullptr, nullptr, &app_bold_22);
    rPtt = pk.obj;
    lv_obj_add_event_cb(rPtt, onPtt, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(rPtt, onPtt, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(rPtt, onPtt, LV_EVENT_PRESS_LOST, nullptr);
    // ajustes
    lv_obj_t* l1 = row(rPages[3], 8);
    static const char* ln[] = {"OFF", "WIFI (TCP)", "BLE"};
    for (int i = 0; i < 3; i++) rLinkC[i] = chip(l1, ln[i], false, onLinkType, (void*)(intptr_t)i, 44);
    lv_obj_t* l2 = row(rPages[3], 8);
    rHost = field(l2, "Host (APRS-Rotor.local or IP)", 290, 63);
    rBle = field(l2, "BLE address (empty = first box)", 260, 17);
    lv_obj_t* l3 = row(rPages[3], 8);
    rPin = field(l3, "PIN", 150, 8, "0123456789");
    button(l3, "SAVE AND CONNECT", onSaveLink, nullptr, 0, 52, P->green);
    button(l3, "RESTART BOX", onBoxReboot, nullptr, 0, 52);
    rInfo.create(rPages[3], 150);
    section(rPages[3], "CALIBRATION (direct control)");
    rRaw.create(rPages[3], 150);
    lv_obj_t* l4 = row(rPages[3], 8);
    button(l4, "POINT A HERE", onCal, (void*)(intptr_t)0, 0, 48);
    button(l4, "POINT B HERE", onCal, (void*)(intptr_t)1, 0, 48);
    button(l4, "IT IS AT AZ", onCal, (void*)(intptr_t)2, 0, 48);
    wrap(rPages[3], "Calibration uses the degrees written in AZ (CONTROL).", &app_text_18, P->muted);
}

static void refreshRotor() {
    const bool on = rotOn();
    const rotp::Pos& p = RM.pos;
    char b[200];
    setText(rState, cfg::get().rotorLink ? net::rotorStateText() : "link off");
    float az = p.az, taz = p.taz;
    rComp->set(0, on && p.hasTarget ? &taz : nullptr, on && p.ok ? &az : nullptr, nullptr);
    if (on && p.ok) {
        setTextf(rAz, "%.1f\xC2\xB0", p.az);
        if (p.hasEl) setTextf(rEl, "%.1f\xC2\xB0", p.el); else setText(rEl, "-");
        setText(rFault, p.moving ? "MOVING" : p.fault.c_str());
        setColor(rFault, p.fault == "OK" ? (p.moving ? P->amber : P->green) : P->red);
    } else { setText(rAz, "-"); setText(rEl, "-"); setText(rFault, cfg::get().rotorLink ? "" : "Set the link in SETUP"); setColor(rFault, P->muted); }
    if (on && p.wind.ok) {
        snprintf(b, sizeof b, "%s %.0f m/s, gusts %.0f (%s, %d min ago)%s", p.wind.locked ? "PARKED BY WIND:" : "Wind", p.wind.ws, p.wind.wg, p.wind.src.c_str(),
                 p.wind.age / 60, p.wind.locked && p.wind.hold > 0 ? (" \xC2\xB7 free in " + std::to_string(p.wind.hold / 60) + " min").c_str() : "");
        setText(rWind, b);
        setColor(rWind, p.wind.locked ? P->red : P->subText);
    } else setText(rWind, on && !p.aprs.empty() ? ("APRS: " + p.aprs).c_str() : "");
    for (int i = 0; i < 4; i++) { chipSet(rTabs[i], i == rTab); show(rPages[i], i == rTab); }
    if (rTab == 1) {
        const cfg::Settings& s = cfg::get();
        for (int i = 0; i < 4; i++) chipSet(rFollowC[i], i == s.rotorFollow, P->amber);
        if (!*lv_textarea_get_text(rFollowCall) && !s.rotorFollowCall.empty()) lv_textarea_set_text(rFollowCall, s.rotorFollowCall.c_str());
        setTextf(rStepL, "%.0f\xC2\xB0", s.rotorStep);
        static const char* why[] = {"The box does not follow anything.",
            "The box points where the tracker's rotor aims (event rot: satellite, station, target...).",
            "The box points at the chosen station (its last position heard by the tracker), from the tracker's GNSS.",
            "The box points at the last station heard with position, from the tracker's GNSS."};
        std::string t = why[s.rotorFollow & 3];
        if (s.rotorFollow == 2) t += " Station: " + (s.rotorFollowCall.empty() ? std::string("(write it above and touch ONE STATION)") : s.rotorFollowCall);
        if (!on && s.rotorFollow) t += " The box is not linked.";
        setText(rFollowInfo, t.c_str());
    }
    if (rTab == 2) {
        const rotp::Cat& c = RM.cat;
        if (on && c.ok && c.link) {
            setText(rCatInfo, c.f ? (proto::mhz(c.f) + "." + proto::subKhz(c.f)).c_str() : "-");
            proto::SReading r;
            if (c.sm >= 0) { r.s = c.sm > 9 ? 9 : c.sm; r.overDb = c.sm > 9 ? (c.sm - 9) * 10 : 0; r.segments = c.sm > 15 ? 15 : c.sm; r.label = rotp::sLabel(c.sm); }
            snprintf(b, sizeof b, "%s%s%s", c.rig.c_str(), c.split && c.tf ? (" \xC2\xB7 TX " + proto::mhz(c.tf)).c_str() : "",
                     c.trackOn ? (" \xC2\xB7 Doppler " + c.trackName + " " + std::to_string(c.dop) + " Hz").c_str() : "");
            rSm.set(c.sm >= 0 ? &r : nullptr, b);
        } else { setText(rCatInfo, on && RM.info.ok && !RM.info.cat ? "No CAT in the box" : on ? "Radio not answering" : "-"); rSm.set(nullptr, ""); }
        for (int i = 0; i < 6; i++) chipSet(rModeC[i], c.mode == MODES[i]);
        chipSet(rVfoA, c.vfo == "A"); chipSet(rVfoB, c.vfo == "B"); chipSet(rSplit, c.split, P->amber); chipSet(rDop, cfg::get().catDoppler, P->green);
        show(rPtt, c.pttAllowed);
        lv_obj_set_style_bg_color(rPtt, C(c.ptt ? P->red : P->key), 0);
        lv_obj_t* stp = lv_obj_get_child(lv_obj_get_parent(rModeC[0]), 0);
        (void)stp;
        lv_obj_t* stepBtn = lv_obj_get_child(lv_obj_get_child(rPages[2], 2), 2);
        snprintf(b, sizeof b, catStep >= 1000 ? "STEP %u kHz" : "STEP %u Hz", (unsigned)(catStep >= 1000 ? catStep / 1000 : catStep));
        setText(lv_obj_get_child(stepBtn, 0), b);
    }
    if (rTab == 3) {
        const cfg::Settings& s = cfg::get();
        for (int i = 0; i < 3; i++) chipSet(rLinkC[i], i == s.rotorLink);
        if (!lv_obj_has_state(rHost, LV_STATE_FOCUSED) && !*lv_textarea_get_text(rHost)) lv_textarea_set_text(rHost, s.rotorHost.c_str());
        if (!lv_obj_has_state(rBle, LV_STATE_FOCUSED) && !*lv_textarea_get_text(rBle)) lv_textarea_set_text(rBle, s.rotorBle.c_str());
        if (!lv_obj_has_state(rPin, LV_STATE_FOCUSED) && !*lv_textarea_get_text(rPin)) lv_textarea_set_text(rPin, s.rotorPin.c_str());
        rInfo.begin();
        const rotp::Info& i = RM.info;
        rInfo.add("LINK", net::rotorStateText(), on ? P->green : P->amber);
        if (on && !RM.authOk && i.pin) rInfo.add("PIN", RM.authFailed ? "wrong PIN" : "not accepted yet", P->red);
        if (i.ok) {
            rInfo.add("BOX", (i.dev + " " + i.ver).c_str());
            rInfo.add("BOARD", i.board.c_str());
            rInfo.add("DRIVER", i.drv.c_str());
            if (!i.call.empty()) rInfo.add("CALLSIGN", i.call.c_str());
            if (!i.ip.empty()) rInfo.addf("ADDRESS", P->text, "%s \xC2\xB7 rotctld %d%s", i.ip.c_str(), i.rotctld, i.rigctld ? (" \xC2\xB7 rigctld " + std::to_string(i.rigctld)).c_str() : "");
            rInfo.addf("LIMITS", P->text, "AZ %.0f-%.0f\xC2\xB0%s", i.minAz, i.maxAz, i.hasEl ? (" \xC2\xB7 EL " + std::to_string((int)i.minEl) + "-" + std::to_string((int)i.maxEl) + "\xC2\xB0").c_str() : "");
        }
        rInfo.end();
        rRaw.begin();
        if (RM.raw.ok) {
            if (RM.raw.hasAz) rRaw.addf("SENSOR AZ", P->text, "%d %s", RM.raw.az, RM.raw.unit.c_str());
            if (RM.raw.hasEl) rRaw.addf("SENSOR EL", P->text, "%d %s", RM.raw.el, RM.raw.unit.c_str());
            if (!RM.raw.hasAz) rRaw.add("SENSOR", RM.raw.link ? "serial gateway linked (no calibration here)" : "-");
        } else rRaw.add("SENSOR", "-", P->muted);
        rRaw.end();
    }
}
static void enterRotor() { if (rotOn()) { net::rotorSend(rotp::cmd("get")); if (RM.info.cat) net::rotorSend(rotp::cmd("cat")); } }

void initRotor() { registerScreen(ROTOR, {"Rotor box", true, buildRotor, refreshRotor, enterRotor, nullptr}); }

} // namespace r7
#endif
