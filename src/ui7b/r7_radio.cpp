/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * RADIO (RadioScreen.kt de la app): banda A = el tracker, con su frecuencia, modulacion, luces TX / RX / MSG / EMG y el
 * S-meter de la ultima estacion oida; banda B = la emisora de la caja del rotor por CAT; BEACON / DIGI / ROTOR y las ultimas
 * oidas. Menu [F] (tres paginas de teclas, mantener pulsada una para ponerla abajo) y DISP (brujula, GPS, altitud, reloj).
 */
#ifdef UI_RADIO
#include "r7.h"
#include "wxc7.h"
#include "astro.h"
#include "settings.h"
#include "net7.h"
#include "settings.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace r7 {

// ------------------------------------------------------------------ teclas del menu [F]
static void kGoSlist() { go(SLIST); }
static void kGoMsgs() { go(MSGS); }
static void kGoTracker() { go(TRACKER); }
static void kBcn() { send(proto::cmdSimple("beacon")); notice("Beacon sent to the tracker"); }
static void kGps() { cfg::get().dispPage = 1; go(DISP); requestRefresh(); }
static void kWx() { go(WX); }
static void kMap() { go(MAP); }
static void kDisp() { go(DISP); }
static void kWarn() { go(WXWARN); }
static void kPager() { go(PAGER); }
static void kDigi() { send(proto::Cmd("digi").flag("on", !M->status.digi).str()); send(proto::cmdSimple("status")); }
static void kDx() { go(DXC); }
static void kSat() { go(SAT); }
static void kRotor() { go(ROTOR); }
static void kAis() { go(AIS); }
static void kRemote() { go(REMOTE); }
static void kConfig() { go(CONFIG); }
static void kFirmware() { go(FIRMWARE); }
static void kSensors() { go(SENSORS); }
static void kSettings() { go(SETTINGS); }
static void kAbout() { go(ABOUT); }
static void doReboot(void*) { send(proto::cmdSimple("reboot")); }
static void kReboot() { confirm("Restart the tracker", "The tracker restarts now; the display links again by itself.", "RESTART", doReboot, nullptr, true); }
static void kCwrx() {
    if (M->status.mode != 2) { notice("CW listening needs the tracker in MORSE mode (Tracker > Mode)"); go(TRACKER); return; }
    send(proto::Cmd("cwrx").text("s", M->cwrx.on ? "stop" : "start").str());
}

static const std::vector<FKey> FK = {
    {"SLIST", "S.LIST", kGoSlist}, {"MSG", "MSG", kGoMsgs}, {"BEACON", "BEACON", kGoTracker}, {"BCN", "BCN", kBcn},
    {"GPS", "GPS", kGps}, {"WX", "WX", kWx}, {"MAP", "MAP", kMap}, {"DISP", "DISP", kDisp},
    {"PAGER", "DAPNET", kPager}, {"DIGI", "DIGI", kDigi}, {"DX", "DX", kDx}, {"SAT", "SAT", kSat},
    {"ROTOR", "ROTOR", kRotor}, {"AIS", "AIS", kAis}, {"CWRX", "CW RX", kCwrx}, {"MODE", "MODE", kGoTracker},
    {"REMOTE", "REMOTE", kRemote}, {"CONFIG", "CONFIG", kConfig}, {"FIRMWARE", "FIRMWARE", kFirmware}, {"SENSORS", "SENSORS", kSensors},
    {"SETTINGS", "SETTINGS", kSettings}, {"ABOUT", "ABOUT", kAbout}, {"REBOOT", "REBOOT", kReboot}, {"TRACKER", "TRACKER", kGoTracker},
    {"WARN", "WARNINGS", kWarn},
};
static const char* PAGES[3][12] = {
    {"SLIST", "MSG", "BEACON", "BCN", "GPS", "WX", "MAP", "DISP", "PAGER", "DIGI", "WARN", nullptr},
    {"DX", "SAT", "ROTOR", "AIS", "CWRX", "MODE", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
    {"REMOTE", "CONFIG", "FIRMWARE", "SENSORS", "SETTINGS", "TRACKER", "REBOOT", "ABOUT", nullptr, nullptr, nullptr, nullptr},
};
static const char* PAGE_NAMES[3] = {"APRS", "RADIO", "SYSTEM"};

const std::vector<FKey>& fkeys() { return FK; }
const FKey* fkey(const std::string& id) { for (const FKey& k : FK) if (id == k.id) return &k; return nullptr; }

std::string fkeySub(const std::string& id, int& lit) {
    char b[64];
    lit = 0;
    if (id == "SLIST") { snprintf(b, sizeof b, "%d heard", (int)M->heard.size()); return b; }
    if (id == "MSG") {
        if (M->msgUnread > 0) { lit = 2; snprintf(b, sizeof b, "%d new", M->msgUnread); return b; }
        snprintf(b, sizeof b, "%d msgs", M->msgTot); return b;
    }
    if (id == "BEACON") {
        if (M->status.smart) { lit = 2; return "SMART"; }
        if (M->status.profile >= 0 && M->status.profile < (int)M->info.profiles.size()) return M->info.profiles[M->status.profile].label;
        return "profile";
    }
    if (id == "BCN") return "TX now";
    if (id == "GPS") { if (!M->gnss.ok) return "-"; if (M->gnss.fix) { lit = 1; snprintf(b, sizeof b, "%d sats", M->gnss.sats); return b; } return "no fix"; }
    if (id == "WX") {
        const proto::Heard* best = nullptr;
        double la, lo, bd = 1e9;
        bool me = ownPos(la, lo);
        for (const auto& h : M->heard) if (h.wx.ok && h.wx.t > -900) {
            double d = me && h.hasPos ? proto::distanceKm(la, lo, h.lat, h.lon) : 1e8;
            if (!best || d < bd) { best = &h; bd = d; }
        }
        if (best) { lit = 1; snprintf(b, sizeof b, "%.0f\xC2\xB0 %s", best->wx.t, best->call.c_str()); return b; }
        return "stations";
    }
    if (id == "MAP") return "radar";
    if (id == "DISP") return "compass";
    if (id == "PAGER") { snprintf(b, sizeof b, "%d pages", (int)M->pages.size()); return b; }
    if (id == "DIGI") { if (M->status.digi) { lit = 1; return "ON"; } return "OFF"; }
    if (id == "DX") { if (net7::dxState() == net7::Link::Linked) { lit = 1; snprintf(b, sizeof b, "%d spots", (int)DX.spots.size()); return b; } return "cluster"; }
    if (id == "SAT") { if (M->sat.ok) { lit = 1; return M->sat.name; } return "passes"; }
    if (id == "ROTOR") {
        if (RM.pos.ok && net7::rotorState() == net7::Link::Linked) { snprintf(b, sizeof b, "%03.0f\xC2\xB0 / %.0f\xC2\xB0", RM.pos.az, RM.pos.el); return b; }
        if (M->rot.ok) { snprintf(b, sizeof b, "%03.0f\xC2\xB0 / %.0f\xC2\xB0", M->rot.az, M->rot.el); return b; }
        return "box";
    }
    if (id == "AIS") { if (!M->ais.empty()) { lit = 1; snprintf(b, sizeof b, "%d ships", (int)M->ais.size()); return b; } return "ships"; }
    if (id == "CWRX") { if (M->cwrx.on) { lit = 1; return "listening"; } return "MORSE"; }
    if (id == "MODE") return M->status.ok ? proto::modeName(*M, M->status.mode) : "-";
    if (id == "REMOTE") return "screen";
    if (id == "CONFIG") return "tracker";
    if (id == "FIRMWARE") return M->info.ok ? M->info.fw : "update";
    if (id == "SENSORS") return "tracker";
    if (id == "SETTINGS") return "link \xC2\xB7 WiFi";
    if (id == "ABOUT") return "APRS LoRa+";
    if (id == "REBOOT") return "tracker";
    if (id == "TRACKER") return M->info.ok ? M->info.board : "-";
    return "";
}

// ------------------------------------------------------------------ bandas
static uint32_t trackerHz() {
    if (M->fm.ok && M->status.mode == 11 && M->fm.f) return M->fm.f;
    if (M->status.f) return M->status.f;
    const int lo = M->status.lora;
    bool aprs = M->status.mode == 0 || M->status.mode == 1;
    if (aprs && lo >= 0 && lo < (int)M->info.loras.size()) return M->info.loras[lo].f;
    return 0;
}
static std::string trackerMod() {
    if (M->status.mode == 11 && M->fm.ok) return M->fm.w ? "FM" : "NFM";
    if (!M->status.mod.empty()) return M->status.mod;
    const int lo = M->status.lora;
    if (M->status.mode == 0) {
        if (M->info.radio == "sa868") return "AFSK";
        return lo >= 0 && lo < (int)M->info.loras.size() && M->info.loras[lo].fsk ? "FSK" : "LoRa";
    }
    static const char* mods[] = {"LoRa", "FSK", "CW", "WSPR", "LR-FHSS", "POCSAG", "FT8", "FT4", "FST4W", "SAT", "M17", "FM", "AIS"};
    return M->status.mode >= 0 && M->status.mode < 13 ? mods[M->status.mode] : "-";
}
static bool isAprsMode(int m) { return m == 0 || m == 1 || m == 10; }
static const proto::Heard* lastWithSignal() {
    const proto::Heard* best = nullptr;
    for (const auto& h : M->heard) if (h.rssi != 0 && (!best || h.age < best->age)) best = &h;
    return best;
}

struct BandA {
    lv_obj_t *obj, *bar, *mod, *alertL, *detail, *detail2, *lastRow, *lastCall, *lastInfo, *lastSym, *note;
    Led tx, rx, msg, emg;
    Freq f;
    SMeter sm;
    bool big;
};
struct BandB { lv_obj_t *obj, *title, *mode, *hint; Freq f; SMeter sm; bool big; };

static void onBandA(lv_event_t*) { go(linked() ? TRACKER : SETTINGS); }
static void onBandB(lv_event_t*) { go(ROTOR); }
static void onLastRx(lv_event_t*) { const proto::Heard* l = lastWithSignal(); if (l) go(STATION, l->call); }

static BandA buildBandA(lv_obj_t* p, bool big, int w) {
    BandA a{};
    a.big = big;
    a.obj = panel(p, 0);
    lv_obj_set_width(a.obj, w);
    lv_obj_set_style_pad_all(a.obj, 0, 0);
    lv_obj_set_clickable(a.obj, true);
    lv_obj_add_event_cb(a.obj, onBandA, LV_EVENT_CLICKED, nullptr);
    a.bar = box(a.obj);                                         // raya verde de la izquierda
    lv_obj_set_ignore_layout(a.bar, true);
    lv_obj_set_size(a.bar, 6, big ? 170 : 50);
    lv_obj_set_pos(a.bar, 0, 16);
    lv_obj_set_style_bg_color(a.bar, C(P->green), 0);
    lv_obj_set_style_bg_opa(a.bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(a.bar, 4, 0);
    lv_obj_t* in = col(a.obj, big ? 8 : 4);
    lv_obj_set_style_pad_left(in, 24, 0);
    lv_obj_set_style_pad_right(in, 16, 0);
    lv_obj_set_style_pad_ver(in, big ? 14 : 10, 0);
    lv_obj_t* hr = row(in, 10);
    lv_obj_t* badge = text(hr, "A", &app_bold_22, P->bg);
    lv_obj_set_style_bg_color(badge, C(P->green), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_set_style_pad_hor(badge, 9, 0);
    lv_obj_t* tl = text(hr, "TRACKER", &app_bold_22, P->muted);
    lv_obj_set_style_text_letter_space(tl, 1, 0);
    lv_obj_t* sp = box(hr);
    lv_obj_set_flex_grow(sp, 1);
    if (big) {
        lv_obj_t* leds = row(hr, 12);
        lv_obj_set_width(leds, LV_SIZE_CONTENT);
        a.tx = led(leds, "TX", P->red); a.rx = led(leds, "RX", P->green); a.msg = led(leds, "MSG", P->amber); a.emg = led(leds, "EMG", P->red);
        lv_obj_t* sp2 = box(hr);
        lv_obj_set_flex_grow(sp2, 1);
    }
    a.mod = text(hr, "-", &app_bold_22, P->cyan);
    lv_obj_set_style_bg_color(a.mod, C(P->bg), 0);
    lv_obj_set_style_bg_opa(a.mod, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(a.mod, 6, 0);
    lv_obj_set_style_pad_hor(a.mod, 8, 0);
    a.alertL = text(in, "", &app_bold_22, P->red);
    oneLine(a.alertL);
    lv_obj_set_width(a.alertL, LV_PCT(100));
    show(a.alertL, false);
    lv_obj_t* fr = row(in, 8);
    lv_obj_set_flex_align(fr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    a.f = freq(fr, big ? &app_mono_64 : &app_mono_40, big ? &app_mono_28 : &app_mono_20);
    lv_obj_t* sp3 = box(fr);
    lv_obj_set_flex_grow(sp3, 1);
    lv_obj_t* dc = col(fr, 0);
    lv_obj_set_width(dc, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(dc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    a.detail = text(dc, "", &app_cond_18, P->subText);
    a.detail2 = text(dc, "", &app_cond_18, P->muted);
    a.note = text(in, "", &app_text_18, P->muted);
    show(a.note, false);
    if (big) {
        a.sm = smeter(in, w - 40);
        a.lastRow = row(in, 10);
        lv_obj_set_clickable(a.lastRow, true);
        lv_obj_add_event_cb(a.lastRow, onLastRx, LV_EVENT_CLICKED, nullptr);
        text(a.lastRow, "Last RX", &app_text_18, P->muted);
        a.lastSym = symbol(a.lastRow);
        a.lastCall = text(a.lastRow, "", &app_bold_22, P->text);
        lv_obj_t* sp4 = box(a.lastRow);
        lv_obj_set_flex_grow(sp4, 1);
        a.lastInfo = text(a.lastRow, "", &app_text_18, P->subText);
    }
    return a;
}

static void refreshBandA(BandA& a, bool lightsOnly = false) {
    const bool lk = linked() && M->status.ok;
    const uint32_t now = nowMs();
    if (a.big) {
        const bool fmMode = M->status.mode == 11 && M->fm.ok;
        bool tx = lk && (fmMode ? M->fm.ptt : (M->rf.txSinceMs && now - M->rf.txSinceMs < 15000));
        bool rx = lk && !tx && (fmMode ? M->fm.busy : (M->rf.rxAtMs && now - M->rf.rxAtMs < 600));
        a.tx.set(tx); a.rx.set(rx);
        a.msg.set(lk && M->leds.msg && now % 5000 < 1000);
        a.emg.set(lk && M->leds.emg && now % 500 < 250, M->leds.flash);
    }
    if (lightsOnly) return;
    const int mode = M->status.mode;
    std::string mod = trackerMod(), mname = proto::modeName(*M, mode);
    uint32_t hz = lk ? trackerHz() : 0;
    char b[160];
    if (!lk) setText(a.mod, "-");
    else if (upper(mname) == upper(mod)) setText(a.mod, mod.c_str());
    else { snprintf(b, sizeof b, "%s \xC2\xB7 %s", mod.c_str(), upper(mname).c_str()); setText(a.mod, b); }
    setColor(a.mod, mod == "CW" ? P->amber : (mod == "POCSAG" || mod == "SAT" || mod == "AIS" || mod == "FM" || mod == "NFM") ? P->green : P->cyan);
    // alarma o mensajes del tracker
    if (lk && M->leds.emg) { snprintf(b, sizeof b, "EMERGENCY \xC2\xB7 %s \xC2\xB7 %s", M->leds.ec.c_str(), M->leds.et.c_str()); setText(a.alertL, b); setColor(a.alertL, P->red); show(a.alertL, true); }
    else if (lk && M->leds.msg) { snprintf(b, sizeof b, "%d message%s on the tracker", M->leds.unread, M->leds.unread == 1 ? "" : "s"); setText(a.alertL, b); setColor(a.alertL, P->amber); show(a.alertL, true); }
    else show(a.alertL, false);
    if (!lk) {
        a.f.set(0);
        setText(a.detail, ""); setText(a.detail2, "");
        setText(a.note, linked() ? "Waiting for the tracker status..." : "Tracker not linked \xC2\xB7 touch to set up the link");
        show(a.note, true);
    } else {
        a.f.set(hz);
        show(a.note, a.big && !hz);
        if (!hz) setText(a.note, "This tracker firmware does not report the frequency of this mode.");
        std::string det = mname;
        const int lo = M->status.lora;
        if (mod == "LoRa" && lo >= 0 && lo < (int)M->info.loras.size()) { snprintf(b, sizeof b, "SF%d \xC2\xB7 CR4/%d", M->info.loras[lo].sf, M->info.loras[lo].cr); det = b; }
        else if (mod == "FSK") det = "G3RUH 9600";
        else if (mod == "AFSK") det = "AFSK 1200";
        else if ((mod == "FM" || mod == "NFM") && M->fm.ok) {
            static const char* tms[] = {"", "T", "TSQL", "DCS", "1750"};
            det = M->fm.w ? "25 kHz" : "12.5 kHz";
            if (M->fm.tm == 1 || M->fm.tm == 2) { snprintf(b, sizeof b, " \xC2\xB7 %s %d.%d", tms[M->fm.tm], M->fm.t / 10, M->fm.t % 10); det += b; }
            else if (M->fm.tm == 3) { snprintf(b, sizeof b, " \xC2\xB7 DCS %03d", M->fm.d); det += b; }
            if (M->fm.m && M->fm.n >= 0) for (const auto& mm : M->fmMems) if (mm.i == M->fm.n && !mm.n.empty()) det += " \xC2\xB7 " + mm.n;
        }
        setText(a.detail, det.c_str());
        std::string d2;
        if (isAprsMode(mode) && M->status.profile >= 0 && M->status.profile < (int)M->info.profiles.size()) d2 = M->info.profiles[M->status.profile].label;
        if (isAprsMode(mode) && M->status.digi) d2 += d2.empty() ? "DIGI" : " \xC2\xB7 DIGI";
        setText(a.detail2, d2.c_str());
    }
    if (!a.big) return;
    // S-meter: modos APRS = ultima estacion oida; MORSE = escucha CW; FM = la del canal
    proto::SReading r;
    bool haveR = false;
    std::string ex;
    const proto::Heard* last = lastWithSignal();
    if (lk && mode == 11 && M->fm.ok) {
        if (M->fm.busy && M->fm.rssi) { r = proto::sMeter(M->fm.rssi, hz); haveR = true; }
        snprintf(b, sizeof b, "%s%s", M->fm.ptt ? "TX" : M->fm.busy ? "BUSY" : "squelch closed", M->fm.busy && M->fm.rssi ? (" \xC2\xB7 " + std::to_string(M->fm.rssi) + " dBm").c_str() : "");
        ex = b;
    } else if (lk && mode == 2) {
        if (M->cwrx.on && M->cwrx.dbm > -900) { r = proto::sMeter(M->cwrx.dbm, hz ? hz : 433775000); haveR = true; snprintf(b, sizeof b, "%.0f dBm \xC2\xB7 CW listening", M->cwrx.dbm); ex = b; }
        else ex = "CW listening off";
    } else if (lk && isAprsMode(mode) && last) {
        r = proto::sMeter(last->rssi, hz ? hz : 433775000); haveR = true;
        snprintf(b, sizeof b, "%d dBm \xC2\xB7 SNR %.1f", last->rssi, last->snr); ex = b;
    }
    a.sm.set(haveR ? &r : nullptr, ex.c_str());
    show(a.lastRow, lk && isAprsMode(mode) && last);
    if (last) {
        symbolSet(a.lastSym, last->symTable(), last->symCode());
        setText(a.lastCall, last->call.c_str());
        double la, lo2;
        std::string info;
        if (last->hasPos && ownPos(la, lo2)) info = fmtKm(proto::distanceKm(la, lo2, last->lat, last->lon)) + " " + fmtDeg(proto::bearingDeg(la, lo2, last->lat, last->lon)) + " \xC2\xB7 ";
        info += fmtAge(last->age + (int)((nowMs() - M->heardAtMs) / 1000));
        setText(a.lastInfo, info.c_str());
    }
}

static BandB buildBandB(lv_obj_t* p, bool big, int w) {
    BandB bb{};
    bb.big = big;
    bb.obj = panel(p, 0, P->subPanel, P->subBorder);
    lv_obj_set_width(bb.obj, w);
    lv_obj_set_clickable(bb.obj, true);
    lv_obj_add_event_cb(bb.obj, onBandB, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bar = box(bb.obj);
    lv_obj_set_ignore_layout(bar, true);
    lv_obj_set_size(bar, 6, big ? 110 : 44);
    lv_obj_set_pos(bar, 0, 14);
    lv_obj_set_style_bg_color(bar, C(P->bandB), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 4, 0);
    lv_obj_t* in = col(bb.obj, big ? 6 : 3);
    lv_obj_set_style_pad_left(in, 24, 0);
    lv_obj_set_style_pad_right(in, 16, 0);
    lv_obj_set_style_pad_ver(in, big ? 12 : 8, 0);
    lv_obj_t* hr = row(in, 10);
    lv_obj_t* badge = text(hr, "B", &app_bold_22, P->bg);
    lv_obj_set_style_bg_color(badge, C(P->subMuted), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_set_style_pad_hor(badge, 9, 0);
    bb.title = text(hr, "NO RADIO", &app_bold_22, P->subMuted);
    oneLine(bb.title);
    lv_obj_set_flex_grow(bb.title, 1);
    lv_obj_set_width(bb.title, 10);
    bb.mode = text(hr, "", &app_bold_22, P->subText);
    lv_obj_set_style_bg_color(bb.mode, C(P->bg), 0);
    lv_obj_set_style_bg_opa(bb.mode, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bb.mode, 6, 0);
    lv_obj_set_style_pad_hor(bb.mode, 8, 0);
    bb.hint = text(in, "CAT through the rotor box \xC2\xB7 touch to set up", &app_text_18, P->subMuted);
    bb.f = freq(in, big ? &app_mono_40 : &app_mono_28, &app_mono_20);
    if (big) bb.sm = smeter(in, w - 40);
    return bb;
}
static void refreshBandB(BandB& bb) {
    const rotp::Cat& c = RM.cat;
    const bool on = net7::rotorState() == net7::Link::Linked && c.ok && c.link && c.f;
    show(bb.hint, !on);
    show(bb.f.obj, on);
    if (bb.big) show(bb.sm.obj, on);
    if (!on) {
        setText(bb.title, net7::rotorState() == net7::Link::Linked && c.ok ? "CAT \xC2\xB7 radio not answering" : "NO RADIO");
        setText(bb.mode, "");
        show(bb.mode, false);
        return;
    }
    std::string t = "CAT \xC2\xB7 " + (c.rig.empty() ? std::string("radio") : c.rig);
    setText(bb.title, t.c_str());
    setText(bb.mode, c.mode.c_str());
    show(bb.mode, !c.mode.empty());
    bb.f.set(c.f, P->subText);
    if (bb.big) {
        proto::SReading r;
        if (c.sm >= 0) { r.s = c.sm <= 9 ? c.sm : 9; r.overDb = c.sm > 9 ? (c.sm - 9) * 10 : 0; r.segments = c.sm > 15 ? 15 : c.sm; r.label = rotp::sLabel(c.sm); }
        char b[96];
        snprintf(b, sizeof b, "VFO %s%s%s%s", c.vfo.c_str(), c.split ? " \xC2\xB7 SPLIT" : "", c.tone > 0 ? (" \xC2\xB7 T " + std::to_string((int)c.tone)).c_str() : "", c.ptt ? " \xC2\xB7 TX" : "");
        bb.sm.set(c.sm >= 0 ? &r : nullptr, b);
    }
}

// ------------------------------------------------------------------ RADIO
static BandA bA;
static BandB bB;
static lv_obj_t *tBeacon, *tDigi, *tRotor;
static List lastList;

static void onTile(lv_event_t* e) { int i = (int)(intptr_t)lv_event_get_user_data(e); if (i == 0) go(TRACKER); else if (i == 1) kDigi(); else go(ROTOR); }
static lv_obj_t* stateTile(lv_obj_t* p, const char* label, int i) {
    lv_obj_t* t = panel(p, 12);
    lv_obj_set_flex_grow(t, 1);
    lv_obj_set_width(t, 10);
    lv_obj_set_style_pad_row(t, 2, 0);
    lv_obj_set_clickable(t, true);
    lv_obj_add_event_cb(t, onTile, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    section(t, label);
    lv_obj_t* v = text(t, "-", &app_bold_26, P->text);
    oneLine(v);
    lv_obj_set_width(v, LV_PCT(100));
    return v;
}
static lv_obj_t* makeLastRow(lv_obj_t* p) {
    lv_obj_t* r = row(p, 10);
    lv_obj_set_height(r, 48);
    lv_obj_set_style_pad_hor(r, 6, 0);
    lv_obj_set_style_radius(r, 8, 0);
    lv_obj_set_style_bg_color(r, C(P->selected), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    symbol(r);
    lv_obj_t* c = text(r, "", &app_bold_22, P->text);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_width(c, 10);
    oneLine(c);
    text(r, "", &app_cond_18, P->muted);
    lv_obj_t* d = text(r, "", &app_cond_18, P->text);
    lv_obj_set_width(d, 76);
    lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t* br = text(r, "", &app_mono_20, P->cyan);
    lv_obj_set_width(br, 52);
    lv_obj_set_style_text_align(br, LV_TEXT_ALIGN_RIGHT, 0);
    return r;
}
static void onLastClick(lv_event_t* e) { int i = clickIndex(e); if (i >= 0 && i < (int)M->heard.size()) go(STATION, M->heard[i].call); }
static void onAllStations(lv_event_t*) { go(SLIST); }

static void buildRadioScr(lv_obj_t* root) {
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(root, GAP, 0);
    lv_obj_t* left = col(root, GAP);
    lv_obj_set_width(left, 600);
    lv_obj_set_height(left, LV_PCT(100));
    bA = buildBandA(left, true, 600);
    bB = buildBandB(left, true, 600);
    lv_obj_set_flex_grow(bB.obj, 1);
    lv_obj_t* right = col(root, GAP);
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_width(right, 10);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_t* tiles = row(right, 8);
    tBeacon = stateTile(tiles, "BEACON", 0);
    tDigi = stateTile(tiles, "DIGI", 1);
    tRotor = stateTile(tiles, "ROTOR", 2);
    lv_obj_t* lp = panel(right, 10);
    lv_obj_set_flex_grow(lp, 1);
    lv_obj_t* lh = row(lp, 6);
    lv_obj_t* sl = section(lh, "LAST HEARD");
    lv_obj_set_flex_grow(sl, 1);
    lv_obj_t* all = text(lh, "stations \xE2\x80\xBA", &app_cond_18, P->cyan);
    lv_obj_set_clickable(all, true);
    lv_obj_set_ext_click_area(all, 12);
    lv_obj_add_event_cb(all, onAllStations, LV_EVENT_CLICKED, nullptr);
    lastList.create(lp, makeLastRow, onLastClick);
    text(lp, "", &app_text_18, P->muted);                       // "Nothing heard yet."
}
static void refreshRadioScr() {
    refreshBandA(bA);
    refreshBandB(bB);
    const bool lk = linked() && M->status.ok;
    if (lk && M->status.smart) { setText(tBeacon, "SMART"); setColor(tBeacon, P->amber); }
    else {
        setColor(tBeacon, P->text);
        setText(tBeacon, lk && M->status.profile >= 0 && M->status.profile < (int)M->info.profiles.size() ? M->info.profiles[M->status.profile].label.c_str() : "-");
    }
    setText(tDigi, !lk ? "-" : M->status.digi ? "ON" : "OFF");
    setColor(tDigi, lk && M->status.digi ? P->green : P->text);
    char b[48];
    if (net7::rotorState() == net7::Link::Linked && RM.pos.ok) snprintf(b, sizeof b, "%03.0f\xC2\xB0/%.0f\xC2\xB0", RM.pos.az, RM.pos.el);
    else if (M->rot.ok) snprintf(b, sizeof b, "%03.0f\xC2\xB0/%.0f\xC2\xB0", M->rot.az, M->rot.el);
    else snprintf(b, sizeof b, "-");
    setText(tRotor, b);
    // ultimas oidas (la lista del tracker va de la mas reciente a la mas antigua)
    lastList.begin();
    double la, lo;
    const bool me = ownPos(la, lo);
    const int extra = (int)((nowMs() - M->heardAtMs) / 1000);
    for (size_t i = 0; i < M->heard.size() && i < 6; i++) {
        const proto::Heard& h = M->heard[i];
        lv_obj_t* r = lastList.next();
        symbolSet(lv_obj_get_child(r, 0), h.symTable(), h.symCode());
        setText(lv_obj_get_child(r, 1), h.call.c_str());
        setText(lv_obj_get_child(r, 2), fmtAge(h.age + extra).c_str());
        const bool pos = me && h.hasPos;
        setText(lv_obj_get_child(r, 3), pos ? fmtKm(proto::distanceKm(la, lo, h.lat, h.lon)).c_str() : "-");
        setText(lv_obj_get_child(r, 4), pos ? proto::cardinal16(proto::bearingDeg(la, lo, h.lat, h.lon)) : "");
    }
    lastList.end();
    lv_obj_t* none = lv_obj_get_child(lv_obj_get_parent(lastList.box), -1);
    setText(none, M->heard.empty() ? (linked() ? "Nothing heard yet." : "Link the tracker to see what it hears.") : "");
    show(none, M->heard.empty());
}

// ------------------------------------------------------------------ banda compacta (menu [F])
static BandA sA;
static BandB sB;
static lv_obj_t* summaryBox = nullptr;
lv_obj_t* bandSummary(lv_obj_t* p) {
    summaryBox = col(p, GAP);
    lv_obj_set_width(summaryBox, 380);
    sA = buildBandA(summaryBox, false, 380);
    sB = buildBandB(summaryBox, false, 380);
    return summaryBox;
}
void bandSummaryRefresh() { if (summaryBox) { refreshBandA(sA); refreshBandB(sB); } }

// ------------------------------------------------------------------ menu [F]
static int fPage = 0;
static lv_obj_t* fChips[3];
static lv_obj_t* fPageLbl;
static Key fK[12];
static lv_obj_t* assignBox = nullptr;
static std::string assignId;

static void onFKey(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    const char* id = PAGES[fPage][i];
    const FKey* k = id ? fkey(id) : nullptr;
    if (k && k->run) k->run();
}
static void assignClose() { if (assignBox) { lv_obj_delete(assignBox); assignBox = nullptr; } }
static void onAssignSlot(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    cfg::get().keys[i] = assignId;
    cfg::save();
    assignClose();
    refreshTouchKeys();
}
static void onAssignCancel(lv_event_t*) { assignClose(); }
static void onFLong(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    const char* id = PAGES[fPage][i];
    if (!id) return;
    lv_indev_wait_release(lv_indev_active());
    assignId = id;
    assignClose();
    assignBox = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(assignBox);
    lv_obj_set_size(assignBox, W, H);
    lv_obj_set_style_bg_color(assignBox, C(P->scrim), 0);
    lv_obj_set_style_bg_opa(assignBox, LV_OPA_70, 0);
    lv_obj_set_clickable(assignBox, true);
    lv_obj_t* p = panel(assignBox, 22);
    lv_obj_set_width(p, 720);
    lv_obj_center(p);
    lv_obj_set_style_pad_row(p, 14, 0);
    char b[64];
    snprintf(b, sizeof b, "Put %s on a touch key", fkey(id)->label);
    text(p, b, &app_bold_26, P->text);
    wrap(p, "Choose which of the five keys next to [F] it replaces.", &app_text_20, P->muted);
    lv_obj_t* r = row(p, 8);
    for (int s = 0; s < 5; s++) {
        const FKey* k = fkey(cfg::get().keys[s]);
        char sub[16];
        snprintf(sub, sizeof sub, "key %d", s + 1);
        Key kk = key(r, k ? k->label : cfg::get().keys[s].c_str(), sub, 10, 64, onAssignSlot, (void*)(intptr_t)s);
        lv_obj_set_flex_grow(kk.obj, 1);
    }
    lv_obj_t* r2 = row(p, 8);
    lv_obj_set_flex_align(r2, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    button(r2, "CANCEL", onAssignCancel, nullptr, 170, 52);
}
static void onFPage(lv_event_t* e) { fPage = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static void onFStep(lv_event_t* e) { fPage = (fPage + 3 + (int)(intptr_t)lv_event_get_user_data(e)) % 3; requestRefresh(); }

static void buildFMenu(lv_obj_t* root) {
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(root, GAP, 0);
    bandSummary(root);
    lv_obj_t* p = panel(root, 14);
    lv_obj_set_flex_grow(p, 1);
    lv_obj_set_width(p, 10);
    lv_obj_set_height(p, LV_PCT(100));
    lv_obj_set_style_pad_row(p, 10, 0);
    lv_obj_t* hr = row(p, 8);
    for (int i = 0; i < 3; i++) fChips[i] = chip(hr, PAGE_NAMES[i], i == fPage, onFPage, (void*)(intptr_t)i);
    lv_obj_t* sp = box(hr);
    lv_obj_set_flex_grow(sp, 1);
    fPageLbl = text(hr, "1 / 3", &app_text_20, P->muted);
    for (int r = 0; r < 3; r++) {
        lv_obj_t* rr = row(p, 8);
        for (int c = 0; c < 4; c++) {
            int i = r * 4 + c;
            fK[i] = key(rr, "", "", 10, 82, onFKey, (void*)(intptr_t)i, &app_bold_22);
            lv_obj_set_flex_grow(fK[i].obj, 1);
            lv_obj_add_event_cb(fK[i].obj, onFLong, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
        }
    }
    lv_obj_t* nr = row(p, 8);
    lv_obj_t* bk = button(nr, "\xE2\x80\xB9 BACK", onFStep, (void*)(intptr_t)-1, 0, 50);
    lv_obj_set_flex_grow(bk, 1);
    lv_obj_t* fw = button(nr, "FWD \xE2\x80\xBA", onFStep, (void*)(intptr_t)1, 0, 50);
    lv_obj_set_flex_grow(fw, 1);
    wrap(p, "Touch a key to open it. Hold it to put it on one of the five touch keys below.", &app_text_18, P->muted);
}
static void refreshFMenu() {
    bandSummaryRefresh();
    for (int i = 0; i < 3; i++) chipSet(fChips[i], i == fPage);
    setTextf(fPageLbl, "%d / 3", fPage + 1);
    for (int i = 0; i < 12; i++) {
        const char* id = PAGES[fPage][i];
        const FKey* k = id ? fkey(id) : nullptr;
        show(fK[i].obj, true);
        lv_obj_set_style_opa(fK[i].obj, k ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_clickable(fK[i].obj, k != nullptr);
        if (!k) continue;
        int lit = 0;
        std::string sub = fkeySub(id, lit);
        fK[i].set(k->label, sub.c_str());
        fK[i].style(P->text, lit == 1 ? P->greenDim : lit == 2 ? P->amberDim : P->keyBorder, lit == 1 ? P->green : lit == 2 ? P->amber : P->muted);
    }
}

// ------------------------------------------------------------------ DISP: brujula, GPS, altitud, reloj
static lv_obj_t* dChips[4];
static lv_obj_t* dPages[4];
static Compass* dComp;
static lv_obj_t *dMode, *dTarget, *dBearing, *dDistLbl, *dDist, *dHdg;
static Key dTk[4];
static Tile dSpd, dCrs, dTemp, dQnh;
static int dTgt = 1;           // 0 MY, 1 STN, 2 SAT, 3 ROTOR
static const char* TGT[] = {"MY", "STN", "SAT", "ROTOR"};
static lv_obj_t* skyObj;
static lv_obj_t* snrBox;
static Rows gpsRows;
static Tile aT[8];
static lv_obj_t *cUtc, *cDate, *cLoc;
static Rows clkRows;

static void onDispPage(lv_event_t* e) { cfg::get().dispPage = (int)(intptr_t)lv_event_get_user_data(e); cfg::save(); requestRefresh(); }
static void onDispTgt(lv_event_t* e) { dTgt = (int)(intptr_t)lv_event_get_user_data(e); requestRefresh(); }
static void onHdgUp(lv_event_t*) { cfg::get().dispHdgUp = !cfg::get().dispHdgUp; cfg::save(); requestRefresh(); }
static void onDispNext(lv_event_t*) { cfg::get().dispPage = (cfg::get().dispPage + 1) % 4; cfg::save(); requestRefresh(); }
static void onDispTarget(lv_event_t*) {
    if (dTgt != 1) return;
    for (const auto& h : M->heard) if (h.hasPos) { go(STATION, h.call); return; }
}

static uint32_t sysColor(char s) {
    switch (s) { case 'G': return 0x3CE68A; case 'E': return 0x4F8BFF; case 'C': return 0xFF5A5A; case 'R': return 0x9AA4AE; default: return 0xC9D4DE; }
}
static void skyDraw(lv_event_t* e) {
    lv_obj_t* o = lv_event_get_target_obj(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const int cx = (a.x1 + a.x2) / 2, cy = (a.y1 + a.y2) / 2, R = lv_area_get_width(&a) / 2 - 18;
    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_color = C(P->skyBg); bg.radius = LV_RADIUS_CIRCLE;
    lv_area_t ca = {cx - R - 12, cy - R - 12, cx + R + 12, cy + R + 12};
    lv_draw_rect(layer, &bg, &ca);
    lv_draw_arc_dsc_t ad;
    lv_draw_arc_dsc_init(&ad);
    ad.center.x = cx; ad.center.y = cy; ad.start_angle = 0; ad.end_angle = 360; ad.width = 1; ad.color = C(P->outline);
    for (int k = 1; k <= 3; k++) { ad.radius = R * k / 3; lv_draw_arc(layer, &ad); }
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.color = C(P->outline); ld.width = 1;
    ld.p1.x = cx - R; ld.p1.y = cy; ld.p2.x = cx + R; ld.p2.y = cy; lv_draw_line(layer, &ld);
    ld.p1.x = cx; ld.p1.y = cy - R; ld.p2.x = cx; ld.p2.y = cy + R; lv_draw_line(layer, &ld);
    lv_draw_label_dsc_t td;
    lv_draw_label_dsc_init(&td);
    td.align = LV_TEXT_ALIGN_CENTER; td.font = &app_bold_22;
    static const char* nm[] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; i++) {
        float t = i * (float)M_PI / 2;
        int x = cx + (int)((R + 2) * sinf(t)), y = cy - (int)((R + 2) * cosf(t));
        td.color = C(i == 0 ? P->red : P->muted); td.text = nm[i];
        lv_area_t la = {x - 14, y - 13, x + 14, y + 13};
        lv_draw_label(layer, &td, &la);
    }
    char b[8];
    for (const proto::SkySat& s : M->sky.sats) {
        if (s.el < 0 || s.az < 0) continue;
        float r = R * (90 - s.el) / 90.0f, t = s.az * (float)M_PI / 180.0f;
        int x = cx + (int)(r * sinf(t)), y = cy - (int)(r * cosf(t));
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        uint32_t col2 = sysColor(s.sys);
        d.bg_color = C(col2); d.bg_opa = s.used ? LV_OPA_COVER : LV_OPA_TRANSP;
        d.border_color = C(col2); d.border_width = 2; d.radius = s.sys == 'G' ? LV_RADIUS_CIRCLE : s.sys == 'E' ? 3 : 8;
        lv_area_t sa = {x - 11, y - 11, x + 11, y + 11};
        lv_draw_rect(layer, &d, &sa);
        snprintf(b, sizeof b, "%d", s.prn);
        td.font = &app_cond_16; td.color = C(s.used ? 0x101010 : col2); td.text = b;
        lv_area_t la = {x - 14, y - 9, x + 14, y + 9};
        lv_draw_label(layer, &td, &la);
    }
}

static void buildDisp(lv_obj_t* root) {
    lv_obj_t* cr = row(root, 8);
    static const char* names[] = {"COMPASS", "GPS", "ALTITUDE", "CLOCK"};
    for (int i = 0; i < 4; i++) { dChips[i] = chip(cr, names[i], false, onDispPage, (void*)(intptr_t)i, 42); lv_obj_set_flex_grow(dChips[i], 1); }
    lv_obj_t* nx = button(cr, "DISP \xE2\x80\xBA", onDispNext, nullptr, 150, 42, P->cyan);
    (void)nx;
    for (int i = 0; i < 4; i++) {
        dPages[i] = row(root, GAP);
        lv_obj_set_flex_grow(dPages[i], 1);
        lv_obj_set_height(dPages[i], 10);
        lv_obj_set_flex_align(dPages[i], LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    }
    // brujula
    lv_obj_t* cp = panel(dPages[0], 10);
    lv_obj_set_size(cp, 400, LV_PCT(100));
    lv_obj_set_flex_align(cp, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    dComp = compass(cp, 360);
    lv_obj_t* rc = col(dPages[0], GAP);
    lv_obj_set_flex_grow(rc, 1);
    lv_obj_set_width(rc, 10);
    lv_obj_t* tp = panel(rc, 14);
    lv_obj_t* th = row(tp, 8);
    lv_obj_t* tcol = col(th, 2);
    lv_obj_set_flex_grow(tcol, 1);
    lv_obj_set_width(tcol, 10);
    dMode = section(tcol, "NORTH UP");
    dTarget = text(tcol, "-", &app_bold_26, P->text);
    oneLine(dTarget);
    lv_obj_set_width(dTarget, LV_PCT(100));
    lv_obj_set_clickable(dTarget, true);
    lv_obj_add_event_cb(dTarget, onDispTarget, LV_EVENT_CLICKED, nullptr);
    dHdg = chip(th, "N-UP", false, onHdgUp, nullptr, 38);
    lv_obj_t* br = row(tp, 8);
    lv_obj_set_flex_align(br, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_t* bc = col(br, 0);
    lv_obj_set_width(bc, LV_SIZE_CONTENT);
    section(bc, "BEARING");
    dBearing = text(bc, "-", &app_mono_40, P->amber);
    lv_obj_t* dc = col(br, 0);
    lv_obj_set_width(dc, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(dc, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    dDistLbl = section(dc, "DISTANCE");
    dDist = text(dc, "-", &app_mono_28, P->text);
    lv_obj_t* kr = row(rc, 8);
    for (int i = 0; i < 4; i++) { dTk[i] = key(kr, TGT[i], nullptr, 10, 56, onDispTgt, (void*)(intptr_t)i); lv_obj_set_flex_grow(dTk[i].obj, 1); }
    lv_obj_t* t1 = row(rc, 8);
    dSpd = tile(t1, "SPEED", "km/h", 10); grow(dSpd.obj);
    dCrs = tile(t1, "COURSE", nullptr, 10); grow(dCrs.obj);
    dTemp = tile(t1, "TEMP", "\xC2\xB0" "C", 10); grow(dTemp.obj);
    dQnh = tile(t1, "QNH", "hPa", 10); grow(dQnh.obj);
    // GPS
    lv_obj_t* sp = panel(dPages[1], 8, P->skyBg, P->panelBorder);
    lv_obj_set_size(sp, 400, LV_PCT(100));
    lv_obj_set_flex_align(sp, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    skyObj = box(sp);
    lv_obj_set_size(skyObj, 370, 370);
    lv_obj_add_event_cb(skyObj, skyDraw, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_t* gr = col(dPages[1], GAP);
    lv_obj_set_flex_grow(gr, 1);
    lv_obj_set_width(gr, 10);
    lv_obj_set_height(gr, LV_PCT(100));
    lv_obj_t* snp = panel(gr, 10);
    section(snp, "SIGNAL (dB-Hz)");
    snrBox = row(snp, 3);
    lv_obj_set_height(snrBox, 96);
    lv_obj_set_flex_align(snrBox, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_t* gp = panel(gr, 12);
    lv_obj_set_flex_grow(gp, 1);
    lv_obj_set_height(gp, 10);
    lv_obj_set_scrollable(gp, true);
    gpsRows.create(gp, 150);
    // altitud
    lv_obj_t* ag = col(dPages[2], GAP);
    static const char* al[8] = {"GNSS ALT", "BARO ALT", "PRESSURE", "QNH", "TEMP", "HUMIDITY", "AIR", "SPEED"};
    static const char* au[8] = {"m", "m", "hPa", "hPa", "\xC2\xB0" "C", "%", "", "km/h"};
    for (int r = 0; r < 2; r++) {
        lv_obj_t* rr = row(ag, GAP);
        for (int c = 0; c < 4; c++) { aT[r * 4 + c] = tile(rr, al[r * 4 + c], au[r * 4 + c], 10); grow(aT[r * 4 + c].obj); }
    }
    // reloj
    lv_obj_t* cp2 = panel(dPages[3], 18);
    lv_obj_set_size(cp2, 600, LV_PCT(100));
    section(cp2, "UTC");
    cUtc = text(cp2, "--:--:--", &app_mono_64, P->green);
    cDate = text(cp2, "", &app_mono_28, P->subText);
    lv_obj_set_style_margin_top(section(cp2, "LOCATOR"), 14, 0);
    cLoc = text(cp2, "-", &app_mono_40, P->text);
    lv_obj_t* cp3 = panel(dPages[3], 18);
    lv_obj_set_flex_grow(cp3, 1);
    lv_obj_set_width(cp3, 10);
    clkRows.create(cp3, 160);
}

static void refreshDisp() {
    const int page = cfg::get().dispPage;
    for (int i = 0; i < 4; i++) { chipSet(dChips[i], i == page); show(dPages[i], i == page); }
    char b[96];
    const proto::Gnss& g = M->gnss;
    const proto::Sensors& s = M->sensors;
    if (page == 0) {
        const bool moving = g.ok && g.spd > 3;
        float heading = moving ? g.crs : (s.heading >= 0 ? s.heading : -1);
        const bool hdgUp = cfg::get().dispHdgUp && heading >= 0;
        setText(dMode, hdgUp ? "HEADING UP" : "NORTH UP");
        setText(lv_obj_get_child(dHdg, 0), cfg::get().dispHdgUp ? "HDG-UP" : "N-UP");
        chipSet(dHdg, cfg::get().dispHdgUp);
        for (int i = 0; i < 4; i++) dTk[i].style(i == dTgt ? P->amber : P->text, i == dTgt ? P->amber : P->keyBorder);
        double la, lo;
        const bool me = ownPos(la, lo);
        float tdeg = 0;
        bool have = false;
        std::string name = "-", dist = "-";
        setText(dDistLbl, dTgt == 1 ? "DISTANCE" : dTgt == 0 ? "SPEED" : "ELEVATION");
        if (dTgt == 1) {
            const proto::Heard* st = nullptr;
            for (const auto& h : M->heard) if (h.hasPos) { st = &h; break; }
            if (!st) name = "No station with position";
            else { name = st->call; if (me) { tdeg = (float)proto::bearingDeg(la, lo, st->lat, st->lon); have = true; dist = fmtKm(proto::distanceKm(la, lo, st->lat, st->lon)); } }
        } else if (dTgt == 2) {
            if (M->sat.ok) { name = M->sat.name; tdeg = M->sat.az; have = true; snprintf(b, sizeof b, "%.0f\xC2\xB0", M->sat.el); dist = b; }
            else name = "No satellite (SAT mode)";
        } else if (dTgt == 3) {
            name = "Rotor box";
            if (RM.pos.ok && net7::rotorState() == net7::Link::Linked) { tdeg = RM.pos.az; have = true; snprintf(b, sizeof b, "%.0f\xC2\xB0", RM.pos.el); dist = b; }
            else if (M->rot.ok) { name = "Tracker rotor: " + M->rot.name; tdeg = M->rot.az; have = true; snprintf(b, sizeof b, "%.0f\xC2\xB0", M->rot.el); dist = b; }
        } else {
            name = "My course";
            if (g.ok) { tdeg = g.crs; have = true; dist = fmtSpeed(g.spd); }
        }
        setText(dTarget, name.c_str());
        if (have) { snprintf(b, sizeof b, "%s %s", fmtDeg(tdeg).c_str(), proto::cardinal16(tdeg)); setText(dBearing, b); } else setText(dBearing, "-");
        setText(dDist, dist.c_str());
        float rot = RM.pos.az;
        const bool rotOk = dTgt != 3 && RM.pos.ok && net7::rotorState() == net7::Link::Linked;
        dComp->set(hdgUp ? heading : 0, have ? &tdeg : nullptr, rotOk ? &rot : nullptr, heading >= 0 ? &heading : nullptr);
        setText(dSpd.unit, speedLabel());
        if (g.ok) { snprintf(b, sizeof b, "%.0f", g.spd * wxc::speedUnit(cfg::get().units).k); dSpd.set(b); dCrs.set(fmtDeg(g.crs).c_str()); } else { dSpd.set("-"); dCrs.set("-"); }
        if (s.temp > -900) { snprintf(b, sizeof b, "%.1f", s.temp); dTemp.set(b, tempColor(s.temp)); } else dTemp.set("-");
        if (s.qnh > 0) { snprintf(b, sizeof b, "%.0f", s.qnh); dQnh.set(b); } else dQnh.set("-");
    } else if (page == 1) {
        lv_obj_invalidate(skyObj);
        // barras de SNR por satelite (como las apps de estado GNSS)
        size_t n = M->sky.sats.size() > 32 ? 32 : M->sky.sats.size();
        while (lv_obj_get_child_count(snrBox) < n) {
            lv_obj_t* c = col(snrBox, 2);
            lv_obj_set_width(c, 10);
            lv_obj_set_flex_grow(c, 1);
            lv_obj_set_height(c, LV_PCT(100));
            lv_obj_set_flex_align(c, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_t* bar = box(c);
            lv_obj_set_width(bar, LV_PCT(80));
            lv_obj_set_style_radius(bar, 3, 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            text(c, "", &app_cond_16, P->muted);
        }
        for (uint32_t i = 0; i < lv_obj_get_child_count(snrBox); i++) {
            lv_obj_t* c = lv_obj_get_child(snrBox, i);
            show(c, i < n);
            if (i >= n) continue;
            const proto::SkySat& ss = M->sky.sats[i];
            lv_obj_t* bar = lv_obj_get_child(c, 0);
            int h = ss.snr > 0 ? ss.snr * 80 / 50 : 2;
            lv_obj_set_height(bar, h > 80 ? 80 : h);
            lv_obj_set_style_bg_color(bar, C(sysColor(ss.sys)), 0);
            lv_obj_set_style_bg_opa(bar, ss.used ? LV_OPA_COVER : LV_OPA_40, 0);
            setTextf(lv_obj_get_child(c, 1), "%d", ss.prn);
        }
        gpsRows.begin();
        if (!g.ok) gpsRows.add("GNSS", "No data from the tracker", P->muted);
        else {
            gpsRows.add("FIX", g.fix ? "3D / yes" : "no fix", g.fix ? P->green : P->amber);
            int seen = 0, used = 0;
            for (const auto& ss : M->sky.sats) { seen++; if (ss.used) used++; }
            if (M->sky.ok) gpsRows.addf("SATELLITES", P->text, "%d used / %d seen", M->sky.used >= 0 ? M->sky.used : used, seen);
            else gpsRows.addf("SATELLITES", P->text, "%d", g.sats);
            gpsRows.addf("POSITION", P->text, "%.5f  %.5f", g.lat, g.lon);
            gpsRows.add("LOCATOR", proto::locator(g.lat, g.lon).c_str());
            gpsRows.addf("ALTITUDE", P->text, "%.0f m", g.alt);
            if (M->sky.ok && M->sky.pd > 0) gpsRows.addf("DOP", P->text, "P %.1f  H %.1f  V %.1f", M->sky.pd, M->sky.hd, M->sky.vd);
            else gpsRows.addf("HDOP", P->text, "%.1f", g.hdop);
            if (M->sky.ok && M->sky.err > 0) gpsRows.addf("ERROR", P->text, "%.1f m", M->sky.err);
            gpsRows.addf("SPEED", P->text, "%s  %s", fmtSpeed(g.spd).c_str(), fmtDeg(g.crs).c_str());
            if (!g.utc.empty()) gpsRows.add("UTC", g.utc.c_str());
            if (!M->sky.ok) gpsRows.add("SKY", "Needs tracker firmware with the 'sky' event", P->muted);
        }
        gpsRows.end();
    } else if (page == 2) {
        auto f = [&](int i, float v, const char* fmt, bool ok, uint32_t col2 = 0xFFFFFFFF) { if (ok) { snprintf(b, sizeof b, fmt, v); aT[i].set(b, col2); } else aT[i].set("-"); };
        f(0, g.alt, "%.0f", g.ok);
        f(1, s.baroAlt, "%.0f", s.baroAlt > -900);
        f(2, s.press, "%.1f", s.press > 0);
        f(3, s.qnh, "%.1f", s.qnh > 0);
        f(4, s.temp, "%.1f", s.temp > -900, s.temp > -900 ? tempColor(s.temp) : 0xFFFFFFFF);
        f(5, s.hum, "%.0f", s.hum >= 0);
        if (!s.iaqLabel.empty()) aT[6].set(s.iaqLabel.c_str()); else f(6, s.iaq, "IAQ %.0f", s.iaq >= 0);
        setText(aT[7].unit, speedLabel());
        f(7, g.spd * (float)wxc::speedUnit(cfg::get().units).k, "%.0f", g.ok);
    } else {
        int y, mo, d, h, mi, sc;
        if (utcNow(y, mo, d, h, mi, sc)) {
            setTextf(cUtc, "%02d:%02d:%02d", h, mi, sc);
            setTextf(cDate, "%04d-%02d-%02d", y, mo, d);
        } else { setText(cUtc, "--:--:--"); setText(cDate, "no time yet (GNSS or WiFi)"); }
        setText(cLoc, g.ok && g.fix ? proto::locator(g.lat, g.lon).c_str() : "-");
        clkRows.begin();
        clkRows.add("TIME FROM", net7::timeValid() ? "internet (NTP)" : g.ok ? "tracker GNSS" : "-");
        if (M->status.ok) clkRows.addf("TRACKER UP", P->text, "%lu h %02lu min", (unsigned long)(M->status.up / 3600), (unsigned long)(M->status.up / 60 % 60));
        // sol y luna en la posicion propia (astro del nucleo del plotter: formulas de NOAA, alrededor de un minuto)
        double la, lo;
        if (ownPos(la, lo) && utcNow(y, mo, d, h, mi, sc)) {
            auto hm = [](double m, char* o, size_t n) { int t = ((int)lround(m) % 1440 + 1440) % 1440; snprintf(o, n, "%02d:%02d", t / 60, t % 60); };
            double r, s2, r6, s6;
            char a[8], c[8];
            if (plotter::sunTimes(y, mo, d, la, lo, -0.833, r, s2)) {
                hm(r, a, sizeof a); hm(s2, c, sizeof c);
                clkRows.addf("SUN UP / DOWN", P->amber, "%s / %s UTC", a, c);
                clkRows.addf("DAYLIGHT", P->text, "%d h %02d min", (int)(s2 - r) / 60, (int)(s2 - r) % 60);
            } else clkRows.add("SUN UP / DOWN", "none today");
            if (plotter::sunTimes(y, mo, d, la, lo, -6, r6, s6)) { hm(r6, a, sizeof a); hm(s6, c, sizeof c); clkRows.addf("TWILIGHT", P->text, "%s / %s UTC", a, c); }
            double alt, az;
            const double uh = h + mi / 60.0 + sc / 3600.0;
            plotter::sunPosition(y, mo, d, uh, la, lo, alt, az);
            clkRows.addf("SUN NOW", alt > 0 ? P->text : P->muted, "alt %.0f\xC2\xB0 az %s", alt, fmtDeg(az).c_str());
            const double age = plotter::moonAgeDays(y, mo, d, uh), ill = plotter::moonIllumination(age);
            const char* ph = age < 1.85 ? "new" : age < 5.54 ? "crescent +" : age < 9.23 ? "first quarter" : age < 12.92 ? "gibbous +" : age < 16.61 ? "full"
                           : age < 20.30 ? "gibbous -" : age < 23.99 ? "last quarter" : age < 27.68 ? "crescent -" : "new";
            clkRows.addf("MOON", P->text, "%s \xC2\xB7 %.0f %%", ph, ill * 100);
        }
        clkRows.addf("DISPLAY UP", P->text, "%lu h %02lu min", (unsigned long)(nowMs() / 3600000), (unsigned long)(nowMs() / 60000 % 60));
        clkRows.end();
    }
}

static void enterDisp() { send(proto::cmdSub(5, 2, 4)); }
static void leaveDisp() { if (linked()) send(proto::cmdSub(5, 2, 0)); }

// ------------------------------------------------------------------ cada segundo y mas a menudo
void tick1s() {}
void fastStep() {
    if (top() == RADIO) refreshBandA(bA, true);
}

void initRadio() {
    registerScreen(RADIO, {"Radio", true, buildRadioScr, refreshRadioScr, nullptr, nullptr});
    registerScreen(FMENU, {"Functions", true, buildFMenu, refreshFMenu, nullptr, nullptr});
    registerScreen(DISP, {"Display", true, buildDisp, refreshDisp, enterDisp, leaveDisp});
}

} // namespace r7
#endif
