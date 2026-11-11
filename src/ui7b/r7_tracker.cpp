/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * TRACKER (TrackerTabs.kt, FmSection.kt, BcastSection.kt de la app): modo con reinicio, perfil de baliza y LoRa, baliza ahora,
 * digipeater, CW (mensajes, texto, escucha por RF), SMS M17, radio FM de voz y radio comercial (T-TWR y cualquier placa con
 * receptor I2C), emergencia (EAI, alarma) y rango de emision de la T-TWR. SENSORS: todos los sensores y el tiempo de internet.
 */
#ifdef UI_RADIO
#include "r7.h"
#include "net.h"
#include "settings.h"
#include "tracker_link.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace r7 {

static bool haveMode(int m) { for (int id : M->info.modeIds) if (id == m) return true; return false; }

// ------------------------------------------------------------------ TRACKER
static lv_obj_t* tSa;
static Rows tInfo;
static lv_obj_t *tModeRow, *tProfRow, *tLoraRow, *tCwPanel, *tCwRow, *tCwText, *tCwRxBtn, *tCwRx, *tCwRxInfo;
static lv_obj_t *tM17Panel, *tM17To, *tM17Text, *tFmPanel, *tFmFreq, *tFmInfo, *tFmF, *tFmMems, *tFmSq;
static lv_obj_t *tFmChips[10];
static lv_obj_t *tBcPanel, *tBcFreq, *tBcRds, *tBcOn, *tSaPanel, *tSaInfo, *tSaCode, *tDigi;
static std::string modesShown, profShown, loraShown, cwShown, memShown;
static int pendingMode = -1;

static void doMode(void*) { if (pendingMode >= 0) send(proto::cmdMode(pendingMode)); pendingMode = -1; }
static void onMode(lv_event_t* e) {
    int m = (int)(intptr_t)lv_event_get_user_data(e);
    if (m == M->status.mode) return;
    pendingMode = m;
    std::string t = "The tracker restarts in " + proto::modeName(*M, m) + ". APRS modes are kept; the others only for the next start.";
    confirm("Change tracker mode", t.c_str(), "SWITCH", doMode);
}
static void onProfile(lv_event_t* e) { send(proto::cmdProfile((int)(intptr_t)lv_event_get_user_data(e))); send(proto::cmdSimple("status")); }
static void onLora(lv_event_t* e) { send(proto::cmdLora((int)(intptr_t)lv_event_get_user_data(e))); send(proto::cmdSimple("status")); }
static void onBeacon(lv_event_t*) { send(proto::cmdSimple("beacon")); notice("Beacon sent"); }
static void onDigi(lv_event_t*) { send(proto::Cmd("digi").flag("on", !M->status.digi).str()); send(proto::cmdSimple("status")); }
static void onCwMsg(lv_event_t* e) { send(proto::Cmd("cw").num("i", (int)(intptr_t)lv_event_get_user_data(e)).str()); }
static void onCwText(lv_event_t*) {
    std::string t = lv_textarea_get_text(tCwText);
    if (t.empty()) return;
    send(proto::Cmd("cw").text("t", t.substr(0, 60)).str());
    lv_textarea_set_text(tCwText, "");
}
static void onCwRx(lv_event_t*) { send(proto::Cmd("cwrx").text("s", M->cwrx.on ? "stop" : "start").str()); }
static void onM17(lv_event_t*) {
    std::string t = lv_textarea_get_text(tM17Text);
    if (t.empty()) return;
    send(proto::Cmd("m17sms").text("to", upper(lv_textarea_get_text(tM17To))).text("t", t).str());
    lv_textarea_set_text(tM17Text, "");
}
// FM
static void fmSet(const char* k, int v) { send(proto::Cmd("fmset").num(k, v).str()); }
static void onFmDial(lv_event_t* e) { send(proto::Cmd("fmdial").num("d", (int)(intptr_t)lv_event_get_user_data(e)).str()); }
static void onFmFreq(lv_event_t*) {
    double mhz = atof(lv_textarea_get_text(tFmF));
    if (mhz < 100 || mhz > 500) { error("Write the frequency in MHz (145.500)"); return; }
    send(proto::Cmd("fmset").num("f", (uint32_t)(mhz * 1e6 + 0.5)).str());
    lv_textarea_set_text(tFmF, "");
}
static void onFmChip(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    const proto::Fm& f = M->fm;
    switch (i) {
        case 0: send(proto::Cmd("fmset").flag("w", !f.w).str()); break;
        case 1: send(proto::Cmd("fmset").flag("hp", !f.hp).str()); break;
        case 2: fmSet("tm", (f.tm + 1) % 5); break;
        case 3: fmSet("sh", f.sh == 0 ? -1 : f.sh == -1 ? 1 : 0); break;
        case 4: fmSet("sq", f.sq > 0 ? f.sq - 1 : 0); break;
        case 5: fmSet("sq", f.sq < 9 ? f.sq + 1 : 9); break;
        case 6: fmSet("nr", (f.nr + 1) % 4); break;
        case 7: send(proto::Cmd("fmscan").text("s", f.scan ? "stop" : "start").flag("mem", true).str()); break;
        case 8: send(proto::Cmd("fmarts").text("s", f.arts ? "stop" : "start").str()); break;
        case 9: send(proto::Cmd("fmset").flag("aprs", !f.aprs).str()); break;
    }
}
static void onFmMem(lv_event_t* e) { send(proto::Cmd("fmmem").text("op", "recall").num("i", (int)(intptr_t)lv_event_get_user_data(e)).str()); }
static void onFmMemList(lv_event_t*) { send(proto::Cmd("fmmem").text("op", "list").str()); }
// radio comercial
static void bc(const char* k, int v) { send(proto::Cmd("bcastset").num(k, v).str()); }
static void onBc(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    const proto::Bcast& b = M->bcast;
    switch (i) {
        case 0: send(proto::Cmd("bcastset").flag("on", !b.on).str()); break;
        case 1: bc("seek", -1); break;
        case 2: bc("step", -1); break;
        case 3: bc("step", 1); break;
        case 4: bc("seek", 1); break;
        case 5: bc("vol", b.vol > 0 ? b.vol - 1 : 0); break;
        case 6: bc("vol", b.vol < 15 ? b.vol + 1 : 15); break;
        case 7: send(proto::Cmd("bcastset").flag("mute", !b.mute).str()); break;
    }
}
// emergencia
static void doEai(void*) { send(proto::Cmd("eai").text("s", "start").str()); }
static void onEai(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i == 0) confirm("Start emergency", "The tracker starts its SOS beacon (5 s countdown) or, on the T-TWR in FM, the voice EAI. It transmits.", "START", doEai, nullptr, true);
    else if (i == 1) send(proto::Cmd("eai").text("s", "stop").str());
    else if (i == 2) send(proto::Cmd("eai").text("s", "ok").str());
    else send(proto::Cmd("alarm").text("s", "off").str());
}
static void onUnlock(lv_event_t* e) {
    if ((intptr_t)lv_event_get_user_data(e)) { send(proto::Cmd("txunlock").flag("lock", true).str()); return; }
    std::string c = lv_textarea_get_text(tSaCode);
    if (c.size() != 128) { error("The unlock code has 128 hexadecimal digits"); return; }
    send(proto::Cmd("txunlock").text("code", c).str());
}

static lv_obj_t* card(lv_obj_t* p, const char* title) { lv_obj_t* c = panel(p, 14); lv_obj_set_style_pad_row(c, 10, 0); section(c, title); return c; }
static lv_obj_t* wrapRow(lv_obj_t* p) { lv_obj_t* r = row(p, 8); lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP); lv_obj_set_style_pad_row(r, 8, 0); return r; }

static void buildTracker(lv_obj_t* root) {
    header(root, "Tracker", nullptr);
    tSa = scrollArea(root);
    lv_obj_t* top = row(tSa, GAP);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* ic = card(top, "TRACKER");
    lv_obj_set_width(ic, 470);
    tInfo.create(ic, 150);
    lv_obj_t* bc2 = card(top, "BEACON");
    lv_obj_set_flex_grow(bc2, 1);
    lv_obj_set_width(bc2, 10);
    lv_obj_t* br = row(bc2, 8);
    button(br, "BEACON NOW", onBeacon, nullptr, 0, 52, P->green);
    tDigi = button(br, "DIGI", onDigi, nullptr, 0, 52);
    text(bc2, "Profile", &app_cond_18, P->muted);
    tProfRow = wrapRow(bc2);
    text(bc2, "LoRa", &app_cond_18, P->muted);
    tLoraRow = wrapRow(bc2);
    lv_obj_t* mc = card(tSa, "MODE (the tracker restarts)");
    tModeRow = wrapRow(mc);
    // CW
    tCwPanel = card(tSa, "TELEGRAPHY (MORSE mode)");
    tCwRow = wrapRow(tCwPanel);
    lv_obj_t* cr = row(tCwPanel, 8);
    tCwText = field(cr, "Free text to send in CW (60)", 0, 60);
    lv_obj_set_flex_grow(tCwText, 1);
    button(cr, "SEND CW", onCwText, nullptr, 0, 52, P->amber);
    lv_obj_t* lr = row(tCwPanel, 10);
    tCwRxBtn = button(lr, "LISTEN CW", onCwRx, nullptr, 0, 52, P->cyan);
    tCwRxInfo = text(lr, "", &app_cond_18, P->muted);
    tCwRx = text(tCwPanel, "", &app_mono_28, P->green);
    lv_obj_set_width(tCwRx, LV_PCT(100));
    lv_label_set_long_mode(tCwRx, LV_LABEL_LONG_MODE_WRAP);
    // M17
    tM17Panel = card(tSa, "M17 SMS");
    lv_obj_t* m1 = row(tM17Panel, 8);
    tM17To = field(m1, "To (empty = @ALL)", 240, 9, nullptr, true);
    tM17Text = field(m1, "Text", 0, 200);
    lv_obj_set_flex_grow(tM17Text, 1);
    button(m1, "SEND", onM17, nullptr, 0, 52, P->green);
    // FM
    tFmPanel = card(tSa, "FM VOICE (T-TWR)");
    lv_obj_t* f1 = row(tFmPanel, 12);
    tFmFreq = text(f1, "---.---", &app_mono_64, P->text);
    tFmInfo = text(f1, "", &app_text_20, P->subText);
    lv_obj_set_flex_grow(tFmInfo, 1);
    lv_obj_set_width(tFmInfo, 10);
    lv_label_set_long_mode(tFmInfo, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_t* f2 = row(tFmPanel, 8);
    static const int dials[] = {-10, -1, 1, 10};
    static const char* dl[] = {"\xE2\x80\xB9\xE2\x80\xB9", "\xE2\x80\xB9", "\xE2\x80\xBA", "\xE2\x80\xBA\xE2\x80\xBA"};
    for (int i = 0; i < 4; i++) button(f2, dl[i], onFmDial, (void*)(intptr_t)dials[i], 80, 52);
    tFmF = field(f2, "MHz", 180, 10, "0123456789.");
    button(f2, "TUNE", onFmFreq, nullptr, 0, 52, P->cyan);
    lv_obj_t* f3 = wrapRow(tFmPanel);
    static const char* fc[] = {"WIDE", "HIGH", "TONE", "SHIFT", "SQ -", "SQ +", "NR", "SCAN", "ARTS", "APRS"};
    for (int i = 0; i < 10; i++) tFmChips[i] = chip(f3, fc[i], false, onFmChip, (void*)(intptr_t)i, 44);
    tFmSq = text(tFmPanel, "", &app_cond_18, P->muted);
    lv_obj_t* f4 = row(tFmPanel, 8);
    text(f4, "Memories", &app_cond_18, P->muted);
    button(f4, "READ", onFmMemList, nullptr, 0, 44);
    tFmMems = wrapRow(tFmPanel);
    // radio comercial
    tBcPanel = card(tSa, "BROADCAST RADIO");
    lv_obj_t* b1 = row(tBcPanel, 12);
    tBcFreq = text(b1, "-", &app_mono_40, P->text);
    tBcRds = text(b1, "", &app_text_20, P->subText);
    lv_obj_set_flex_grow(tBcRds, 1);
    lv_obj_set_width(tBcRds, 10);
    lv_label_set_long_mode(tBcRds, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_t* b2 = wrapRow(tBcPanel);
    static const char* bl[] = {"ON", "SEEK \xE2\x80\xB9", "\xE2\x80\xB9", "\xE2\x80\xBA", "SEEK \xE2\x80\xBA", "VOL -", "VOL +", "MUTE"};
    for (int i = 0; i < 8; i++) { lv_obj_t* b = button(b2, bl[i], onBc, (void*)(intptr_t)i, 0, 50); if (i == 0) tBcOn = b; }
    // emergencia
    lv_obj_t* ec = card(tSa, "EMERGENCY");
    lv_obj_t* e1 = wrapRow(ec);
    button(e1, "START EAI / SOS", onEai, (void*)(intptr_t)0, 0, 52, P->red);
    button(e1, "STOP", onEai, (void*)(intptr_t)1, 0, 52, P->amber);
    button(e1, "I'M OK", onEai, (void*)(intptr_t)2, 0, 52, P->green);
    button(e1, "SILENCE ALARM", onEai, (void*)(intptr_t)3, 0, 52);
    // rango de emision de la T-TWR
    tSaPanel = card(tSa, "TRANSMIT RANGE (T-TWR)");
    tSaInfo = wrap(tSaPanel, "", &app_text_20, P->subText);
    lv_obj_t* s1 = row(tSaPanel, 8);
    tSaCode = field(s1, "Unlock code for this device (128 hex digits)", 0, 128, "0123456789abcdefABCDEF");
    lv_obj_set_flex_grow(tSaCode, 1);
    button(s1, "UNLOCK", onUnlock, (void*)(intptr_t)0, 0, 52, P->amber);
    button(s1, "LOCK", onUnlock, (void*)(intptr_t)1, 0, 52);
}

static void refreshTracker() {
    const proto::Info& in = M->info;
    const proto::Status& st = M->status;
    tInfo.begin();
    tInfo.add("LINK", trk::stateText(), linked() ? P->green : P->amber);
    if (in.ok) {
        tInfo.add("BOARD", in.board.c_str());
        tInfo.add("FIRMWARE", in.fw.c_str());
        tInfo.add("RADIO", in.radio == "sa868" ? (in.sa868.present ? ("SA868 " + in.sa868.band + " \xC2\xB7 " + in.sa868.fw).c_str() : "SA868") : "LoRa");
    }
    if (st.ok) {
        tInfo.add("MODE", proto::modeName(*M, st.mode).c_str(), P->cyan);
        if (st.f) tInfo.addf("FREQUENCY", P->text, "%s MHz %s", proto::mhz(st.f).c_str(), st.mod.c_str());
        tInfo.addf("UPTIME", P->text, "%lu h %02lu min", (unsigned long)(st.up / 3600), (unsigned long)(st.up / 60 % 60));
        if (st.batt > 0) tInfo.addf("BATTERY", P->text, "%.2f V", st.batt);
        if (st.sos) tInfo.add("SOS", "ON", P->red);
    }
    if (!in.rst.empty()) tInfo.add("LAST RESET", (in.rst + (in.rstAt.empty() ? "" : " \xC2\xB7 " + in.rstAt)).c_str(), P->amber);
    if (!in.pmuOff.empty()) tInfo.add("PMU OFF", in.pmuOff.c_str(), P->amber);
    tInfo.end();
    lv_obj_t* dl = lv_obj_get_child(tDigi, 0);
    setText(dl, st.digi ? "DIGI ON" : "DIGI OFF");
    buttonColor(tDigi, st.digi ? P->green : P->text);
    // listas que solo se rehacen si cambian
    std::string ms;
    for (size_t i = 0; i < in.modeIds.size(); i++) ms += std::to_string(in.modeIds[i]) + in.modeNames[i] + (in.modeIds[i] == st.mode ? "*" : "") + ";";
    if (ms != modesShown) {
        modesShown = ms;
        lv_obj_clean(tModeRow);
        for (size_t i = 0; i < in.modeIds.size(); i++) chip(tModeRow, in.modeNames[i].c_str(), in.modeIds[i] == st.mode, onMode, (void*)(intptr_t)in.modeIds[i], 46, P->amber);
        if (in.modeIds.empty()) text(tModeRow, linked() ? "Waiting for the tracker..." : "Tracker not linked", &app_text_20, P->muted);
    }
    std::string ps;
    for (size_t i = 0; i < in.profiles.size(); i++) ps += in.profiles[i].call + in.profiles[i].label + ((int)i == st.profile ? "*" : "") + ";";
    if (ps != profShown) {
        profShown = ps;
        lv_obj_clean(tProfRow);
        for (size_t i = 0; i < in.profiles.size(); i++)
            chip(tProfRow, (in.profiles[i].label.empty() ? in.profiles[i].call : in.profiles[i].label + " " + in.profiles[i].call).c_str(), (int)i == st.profile, onProfile, (void*)(intptr_t)i, 44);
    }
    std::string ls;
    for (size_t i = 0; i < in.loras.size(); i++) ls += std::to_string(in.loras[i].f) + ((int)i == st.lora ? "*" : "") + ";";
    if (ls != loraShown) {
        loraShown = ls;
        lv_obj_clean(tLoraRow);
        char b[48];
        for (size_t i = 0; i < in.loras.size(); i++) {
            const proto::LoraProfile& l = in.loras[i];
            if (l.fsk) snprintf(b, sizeof b, "%s FSK", proto::mhz(l.f).c_str()); else snprintf(b, sizeof b, "%s SF%d", proto::mhz(l.f).c_str(), l.sf);
            chip(tLoraRow, b, (int)i == st.lora, onLora, (void*)(intptr_t)i, 44);
        }
    }
    // CW
    show(tCwPanel, in.capCw || haveMode(2));
    std::string cs;
    for (const auto& c : in.cwMsgs) cs += c + ";";
    if (cs != cwShown) {
        cwShown = cs;
        lv_obj_clean(tCwRow);
        for (size_t i = 0; i < in.cwMsgs.size() && i < 5; i++) chip(tCwRow, in.cwMsgs[i].c_str(), false, onCwMsg, (void*)(intptr_t)i, 44, P->amber);
    }
    setText(lv_obj_get_child(tCwRxBtn, 0), M->cwrx.on ? "STOP LISTENING" : "LISTEN CW");
    char b[160];
    if (M->cwrx.on) snprintf(b, sizeof b, "%.0f dBm \xC2\xB7 SNR %d \xC2\xB7 %d wpm%s", M->cwrx.dbm, M->cwrx.snr, M->cwrx.wpm,
                             M->cwrx.f ? (" \xC2\xB7 " + proto::mhz(M->cwrx.f) + " MHz").c_str() : "");
    else snprintf(b, sizeof b, st.mode == 2 ? "The tracker is busy while listening (no beacons)." : "Needs the tracker in MORSE mode.");
    setText(tCwRxInfo, b);
    setText(tCwRx, M->cwrx.text.size() > 180 ? M->cwrx.text.substr(M->cwrx.text.size() - 180).c_str() : M->cwrx.text.c_str());
    // M17
    show(tM17Panel, in.capM17 && st.mode == 10);
    // FM
    const proto::Fm& f = M->fm;
    show(tFmPanel, in.capFm);
    if (in.capFm) {
        if (st.mode == 11 && f.ok) { setText(tFmFreq, proto::mhz(f.f).c_str()); setColor(tFmFreq, f.ptt ? P->red : f.busy ? P->green : P->text); }
        else { setText(tFmFreq, "---.---"); setColor(tFmFreq, P->off); }
        static const char* tms[] = {"no tone", "TONE", "TSQL", "DCS", "1750 Hz"};
        if (st.mode != 11) snprintf(b, sizeof b, "Switch the tracker to FM mode to use the voice radio.");
        else snprintf(b, sizeof b, "%s%s \xC2\xB7 %s %d.%d Hz \xC2\xB7 shift %s%s%s%s", f.ptt ? "TX \xC2\xB7 " : f.busy ? "BUSY \xC2\xB7 " : "",
                      f.m ? ("MEM " + std::to_string(f.n)).c_str() : "VFO", tms[f.tm < 5 ? f.tm : 0], f.t / 10, f.t % 10,
                      f.sh < 0 ? "-" : f.sh > 0 ? "+" : "0", f.scan ? " \xC2\xB7 SCAN" : "", f.arts ? " \xC2\xB7 ARTS" : "", f.note.empty() ? "" : (" \xC2\xB7 " + f.note).c_str());
        setText(tFmInfo, b);
        bool sel[10] = {f.w, f.hp, f.tm > 0, f.sh != 0, false, false, f.nr > 0, f.scan > 0, f.arts > 0, f.aprs};
        for (int i = 0; i < 10; i++) chipSet(tFmChips[i], sel[i] && st.mode == 11);
        snprintf(b, sizeof b, "Squelch %d%s \xC2\xB7 noise reduction %d \xC2\xB7 RSSI %d dBm", f.sq, f.sqa ? " (auto)" : "", f.nr, f.rssi);
        setText(tFmSq, b);
        std::string mm;
        for (const auto& m : M->fmMems) mm += std::to_string(m.i) + m.n + ";";
        if (mm != memShown) {
            memShown = mm;
            lv_obj_clean(tFmMems);
            for (const auto& m : M->fmMems) {
                snprintf(b, sizeof b, "%d %s %s", m.i, m.n.c_str(), proto::mhz(m.f).c_str());
                chip(tFmMems, b, false, onFmMem, (void*)(intptr_t)m.i, 42);
            }
        }
    }
    // radio comercial
    const proto::Bcast& bc2 = M->bcast;
    show(tBcPanel, in.bcast != "none" && !in.bcast.empty());
    if (bc2.ok) {
        static const char* bands[] = {"FM", "MW", "SW", "LW"};
        if (bc2.band == 0) snprintf(b, sizeof b, "%u.%u MHz", (unsigned)(bc2.f / 1000), (unsigned)(bc2.f % 1000 / 100));
        else snprintf(b, sizeof b, "%u kHz %s", (unsigned)bc2.f, bands[bc2.band & 3]);
        setText(tBcFreq, bc2.on ? b : "OFF");
        snprintf(b, sizeof b, "%s %s \xC2\xB7 vol %d%s \xC2\xB7 %d dBuV%s\n%s", bc2.chip.c_str(), bc2.ps.c_str(), bc2.vol, bc2.mute ? " (mute)" : "", bc2.rssi,
                 bc2.st ? " \xC2\xB7 stereo" : "", bc2.rt.c_str());
        setText(tBcRds, b);
        setText(lv_obj_get_child(tBcOn, 0), bc2.on ? "OFF" : "ON");
    }
    // rango de emision
    show(tSaPanel, in.sa868.present);
    if (in.sa868.present) {
        snprintf(b, sizeof b, "%s. Receive 136-174 / 400-480 MHz; transmit only in the amateur bands of the IARU region by GPS. MAC %s.",
                 in.sa868.txul ? "Transmit range UNLOCKED" : in.sa868.txulav ? "Amateur bands only (can be unlocked with this device's code)" : "Amateur bands only",
                 in.sa868.mac.c_str());
        setText(tSaInfo, b);
    }
}
static void enterTracker() {
    if (!linked()) return;
    send(proto::cmdSimple("info"));
    send(proto::cmdSimple("status"));
    if (M->info.capFm) { send(proto::cmdSimple("fm")); send(proto::Cmd("fmmem").text("op", "list").str()); }
    if (M->info.bcast != "none" && !M->info.bcast.empty()) send(proto::Cmd("bcast").num("sub", 2).str());
}
static void leaveTracker() { if (linked() && M->info.bcast != "none" && !M->info.bcast.empty()) send(proto::Cmd("bcast").num("sub", 0).str()); }

// ------------------------------------------------------------------ SENSORS
static Rows sTr, sNet, sImu, sDisp;
static lv_obj_t *sNetP, *sImuP;
static void onWxNow(lv_event_t*) { net::wxNow(); notice("Downloading the internet weather..."); }
static void buildSensors(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Sensors", &tr);
    button(tr, "INTERNET WEATHER NOW", onWxNow, nullptr, 0, 48, P->cyan);
    lv_obj_t* sa = scrollArea(root);
    lv_obj_t* r = row(sa, GAP);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* a = card(r, "TRACKER SENSORS");
    lv_obj_set_width(a, 490);
    sTr.create(a, 170);
    lv_obj_t* rc = col(r, GAP);
    lv_obj_set_flex_grow(rc, 1);
    lv_obj_set_width(rc, 10);
    sNetP = card(rc, "INTERNET WEATHER (in the tracker)");
    sNet.create(sNetP, 150);
    sImuP = card(rc, "MOTION");
    sImu.create(sImuP, 150);
    lv_obj_t* dp = card(rc, "THIS DISPLAY");
    sDisp.create(dp, 150);
}
static void refreshSensors() {
    const proto::Sensors& s = M->sensors;
    sTr.begin();
    if (!s.ok) sTr.add("-", linked() ? "No sensor data yet" : "Tracker not linked", P->muted);
    else {
        if (s.batt > 0) sTr.addf("BATTERY", P->text, "%.2f V%s", s.batt, s.battPct >= 0 ? (" \xC2\xB7 " + std::to_string(s.battPct) + " %").c_str() : "");
        if (s.charging >= 0) sTr.add("CHARGING", s.charging ? (s.chg.empty() ? "yes" : s.chg.c_str()) : "no", s.charging ? P->green : P->text);
        if (s.battMa > -900) sTr.addf("CURRENT", P->text, "%.0f mA", s.battMa);
        if (s.vbus > 0) sTr.addf("USB", P->text, "%.2f V", s.vbus);
        if (s.temp > -900) sTr.addf("TEMPERATURE", tempColor(s.temp), "%.1f \xC2\xB0" "C%s", s.temp, s.wxSensor.empty() ? "" : ("  " + s.wxSensor).c_str());
        if (s.hum >= 0) sTr.addf("HUMIDITY", P->text, "%.0f %%", s.hum);
        if (s.press > 0) sTr.addf("PRESSURE", P->text, "%.1f hPa", s.press);
        if (s.qnh > 0) sTr.addf("QNH", P->text, "%.1f hPa%s", s.qnh, s.qnhSrc == "net" ? " (internet)" : s.qnhSrc == "man" ? " (manual)" : s.qnhSrc == "gps" ? " (GPS)" : "");
        if (s.baroAlt > -900) sTr.addf("BARO ALTITUDE", P->text, "%.0f m", s.baroAlt);
        if (s.gas >= 0) sTr.addf("GAS", P->text, "%.1f kohm", s.gas);
        if (s.iaq >= 0) sTr.addf("AIR (IAQ)", P->text, "%.0f %s", s.iaq, s.iaqLabel.c_str());
        if (s.imuTemp > -900) sTr.addf("IMU TEMP", P->text, "%.1f \xC2\xB0" "C", s.imuTemp);
        if (s.boardTemp > -900) sTr.addf("BOARD TEMP", P->text, "%.1f \xC2\xB0" "C", s.boardTemp);
        if (s.pmuTemp > -900) sTr.addf("PMU TEMP", P->text, "%.1f \xC2\xB0" "C", s.pmuTemp);
        if (s.heap) sTr.addf("FREE MEMORY", P->text, "%lu KB", (unsigned long)(s.heap / 1024));
    }
    sTr.end();
    const proto::NetWx& n = s.net;
    show(sNetP, n.ok);
    if (n.ok) {
        sNet.begin();
        sNet.addf("SOURCE", P->text, "%s %s \xC2\xB7 %u min ago", n.src.c_str(), n.place.c_str(), (unsigned)(n.age / 60));
        if (!n.desc.empty()) sNet.add("SKY", n.desc.c_str());
        if (n.t > -900) sNet.addf("TEMPERATURE", tempColor(n.t), "%.1f \xC2\xB0" "C", n.t);
        if (n.h >= 0) sNet.addf("HUMIDITY", P->text, "%.0f %%", n.h);
        if (n.p > 0) sNet.addf("PRESSURE", P->text, n.d3 > -900 ? "%.1f hPa, %+.1f in 3 h" : "%.1f hPa", n.p, n.d3);
        if (!n.forecast.empty()) sNet.add("FORECAST", n.forecast.c_str(), P->cyan);
        if (n.ws >= 0) sNet.addf("WIND", P->text, "%.1f m/s%s%s", n.ws, n.wg >= 0 ? (", gusts " + std::to_string((int)n.wg)).c_str() : "", n.wd >= 0 ? (" from " + std::string(proto::cardinal16(n.wd))).c_str() : "");
        if (n.rain >= 0) sNet.addf("RAIN", P->text, "%.1f mm/h", n.rain);
        if (n.aqiUs >= 0) sNet.addf("AIR QUALITY", P->text, "US AQI %d %s", n.aqiUs, n.pollutant.c_str());
        else if (n.aqiOwm > 0) sNet.addf("AIR QUALITY", P->text, "%d / 5", n.aqiOwm);
        if (n.pm25 >= 0) sNet.addf("PM2.5", P->text, "%.1f \xC2\xB5g/m\xC2\xB3", n.pm25);
        sNet.end();
    }
    show(sImuP, s.hasAccel || s.hasGyro || s.heading >= 0 || s.moving >= 0);
    sImu.begin();
    if (s.heading >= 0) sImu.addf("HEADING", P->text, "%.0f\xC2\xB0 %s%s", s.heading, proto::cardinal16(s.heading), s.magCal == 0 ? " (not calibrated)" : "");
    if (s.moving >= 0) sImu.add("MOVING", s.moving ? "yes" : "no");
    if (s.hasAccel) sImu.addf("ACCEL (g)", P->text, "%.2f  %.2f  %.2f", s.accel[0], s.accel[1], s.accel[2]);
    if (s.hasGyro) sImu.addf("GYRO (\xC2\xB0/s)", P->text, "%.1f  %.1f  %.1f", s.gyro[0], s.gyro[1], s.gyro[2]);
    sImu.end();
    sDisp.begin();
    sDisp.add("INTERNET WEATHER", net::wxStatus().c_str());
    net::Wifi w = net::wifi();
    sDisp.add("WIFI", w.connected ? (w.ssid + " \xC2\xB7 " + w.ip).c_str() : w.configured ? "connecting..." : "not set (Settings)");
    sDisp.end();
}
static void enterSensors() { if (linked()) { send(proto::cmdSimple("sensors")); send(proto::cmdSub(3, 2, 0)); } }
static void leaveSensors() { if (linked()) send(proto::cmdSub(5, 2, 0)); }

void initTracker() {
    registerScreen(TRACKER, {"Tracker", true, buildTracker, refreshTracker, enterTracker, leaveTracker});
    registerScreen(SENSORS, {"Sensors", true, buildSensors, refreshSensors, enterSensors, leaveSensors});
}

} // namespace r7
#endif
