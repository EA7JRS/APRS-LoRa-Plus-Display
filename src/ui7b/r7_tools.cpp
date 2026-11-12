/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Herramientas (pestanas de la app): AIS (barcos, peligro primero, GO TO, ajustes), REMOTE (espejo de la pantalla del tracker y
 * todas sus opciones de menu por 'act'), CONFIG (todo /tracker_conf.json: cfgget por trozos, edicion por claves, cfgset),
 * FIRMWARE (version instalada y publicada; actualizacion por internet + BLE), SETTINGS (enlace, WiFi, DX, tiempo, pantalla) y ABOUT.
 */
#ifdef UI_RADIO
#include "r7.h"
#include "board.h"
#include "net7.h"
#include "settings.h"
#include "tracker_link.h"
#include "ui_assets.h"
#include "maptiles.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace r7 {

static lv_obj_t* card(lv_obj_t* p, const char* title) { lv_obj_t* c = panel(p, 14); lv_obj_set_style_pad_row(c, 10, 0); section(c, title); return c; }

// ------------------------------------------------------------------ AIS
static List aList;
static Rows aRows;
static lv_obj_t *aEmpty, *aInfo, *aChC[3];
static uint32_t aSel = 0;
static std::vector<int> aView;
static lv_obj_t* makeShip(lv_obj_t* p) {
    lv_obj_t* r = row(p, 10);
    lv_obj_set_height(r, 60);
    lv_obj_set_style_pad_hor(r, 10, 0);
    lv_obj_set_style_radius(r, 10, 0);
    lv_obj_set_style_bg_color(r, C(P->panel), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_t* c = col(r, 0);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_width(c, 10);
    lv_obj_t* n = text(c, "", &app_bold_22, P->text);
    oneLine(n);
    lv_obj_set_width(n, LV_PCT(100));
    text(c, "", &app_cond_16, P->muted);
    lv_obj_t* d = text(r, "", &app_mono_20, P->text);
    lv_obj_set_width(d, 140);
    lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_RIGHT, 0);
    return r;
}
static void onShip(lv_event_t* e) { int i = clickIndex(e); if (i >= 0 && i < (int)aView.size()) { aSel = M->ais[aView[i]].mmsi; requestRefresh(); } }
static void onAisCh(lv_event_t* e) { send(proto::Cmd("aisset").num("ch", (int)(intptr_t)lv_event_get_user_data(e)).str()); }
static void onAisGoto(lv_event_t*) { if (aSel) send(proto::Cmd("aisset").num("goto", aSel).str()); }
static void doAisClear(void*) { send(proto::Cmd("aisset").flag("clear", true).str()); M->ais.clear(); aSel = 0; }
static void onAisClear(lv_event_t*) { confirm("Clear the ship list", "The tracker forgets the ships it has heard.", "CLEAR", doAisClear); }
static void requestAis() { if (linked()) { send(proto::Cmd("aislist").num("from", 0).num("cnt", 15).str()); send(proto::Cmd("aislist").num("from", 15).num("cnt", 15).str()); } }
static void buildAis(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "AIS ships", &tr);
    static const char* ch[] = {"CH A", "CH B", "A + B"};
    for (int i = 0; i < 3; i++) aChC[i] = chip(tr, ch[i], false, onAisCh, (void*)(intptr_t)i, 42);
    button(tr, "CLEAR", onAisClear, nullptr, 0, 48);
    lv_obj_t* body = row(root, GAP);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_height(body, 10);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* left = col(body, 6);
    lv_obj_set_size(left, 560, LV_PCT(100));
    lv_obj_set_scrollable(left, true);
    aInfo = text(left, "", &app_cond_18, P->muted);
    aEmpty = wrap(left, "", &app_text_20, P->muted);
    aList.create(left, makeShip, onShip);
    lv_obj_t* right = panel(body, 14);
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_width(right, 10);
    lv_obj_set_height(right, LV_PCT(100));
    lv_obj_set_scrollable(right, true);
    aRows.create(right, 130);
    button(right, "GO TO (tracker)", onAisGoto, nullptr, 0, 50, P->cyan);
}
static void refreshAis() {
    aView.clear();
    for (size_t i = 0; i < M->ais.size(); i++) aView.push_back((int)i);
    std::stable_sort(aView.begin(), aView.end(), [](int a, int b) {
        const auto &x = M->ais[a], &y = M->ais[b];
        if (x.danger != y.danger) return x.danger;
        return (x.nm < 0 ? 1e9f : x.nm) < (y.nm < 0 ? 1e9f : y.nm);
    });
    for (int i = 0; i < 3; i++) chipSet(aChC[i], (M->aisCh == "A" && i == 0) || (M->aisCh == "B" && i == 1) || (M->aisCh.size() > 1 && i == 2));
    setTextf(aInfo, "%d ships \xC2\xB7 %d messages \xC2\xB7 %s", M->aisTot, M->aisMsgs, M->aisSrc.empty() ? "-" : M->aisSrc.c_str());
    show(aEmpty, aView.empty());
    setText(aEmpty, M->info.capAis ? (M->status.mode == 12 ? "No ships heard yet." : "The tracker receives AIS in mode 12 (Tracker > Mode).") :
                                     "This tracker has no AIS receiver.");
    aList.begin();
    char b[120];
    const proto::Vessel* sel = nullptr;
    for (int i : aView) {
        const proto::Vessel& v = M->ais[i];
        if (v.mmsi == aSel) sel = &v;
        lv_obj_t* r = aList.next();
        lv_obj_set_style_border_color(r, C(v.danger ? P->red : v.mmsi == aSel ? P->amber : P->panelBorder), 0);
        lv_obj_t* c = lv_obj_get_child(r, 0);
        setText(lv_obj_get_child(c, 0), v.name.empty() ? std::to_string(v.mmsi).c_str() : v.name.c_str());
        setColor(lv_obj_get_child(c, 0), v.danger ? P->red : P->text);
        snprintf(b, sizeof b, "%s \xC2\xB7 %s%s", v.cl.c_str(), v.sog >= 0 ? (std::to_string((int)v.sog) + " kn").c_str() : "-",
                 v.cpa >= 0 ? (" \xC2\xB7 CPA " + std::to_string(v.cpa).substr(0, 4) + " nm in " + std::to_string((int)v.tcpa) + " min").c_str() : "");
        setText(lv_obj_get_child(c, 1), b);
        if (v.nm >= 0) snprintf(b, sizeof b, "%.1f nm %s", v.nm, v.brg >= 0 ? proto::cardinal16(v.brg) : ""); else snprintf(b, sizeof b, "-");
        setText(lv_obj_get_child(r, 1), b);
    }
    aList.end();
    aRows.begin();
    if (!sel) aRows.add("SHIP", "Touch a ship to see it", P->muted);
    else {
        aRows.add("NAME", sel->name.empty() ? "-" : sel->name.c_str(), sel->danger ? P->red : P->text);
        aRows.addf("MMSI", P->text, "%lu \xC2\xB7 class %s", (unsigned long)sel->mmsi, sel->cl.c_str());
        if (!sel->call.empty()) aRows.add("CALLSIGN", sel->call.c_str());
        if (sel->imo) aRows.addf("IMO", P->text, "%d", sel->imo);
        if (sel->hasPos) aRows.addf("POSITION", P->text, "%.5f  %.5f", sel->lat, sel->lon);
        if (sel->nm >= 0) aRows.addf("DISTANCE", P->text, "%.2f nm \xC2\xB7 %s", sel->nm, sel->brg >= 0 ? fmtDeg(sel->brg).c_str() : "-");
        if (sel->sog >= 0) aRows.addf("SPEED", P->text, "%.1f kn \xC2\xB7 COG %.0f\xC2\xB0%s", sel->sog, sel->cog, sel->hdg >= 0 ? (" \xC2\xB7 HDG " + std::to_string(sel->hdg)).c_str() : "");
        if (sel->cpa >= 0) aRows.addf("CPA", sel->danger ? P->red : P->text, "%.2f nm in %.1f min", sel->cpa, sel->tcpa);
        if (!sel->dest.empty()) aRows.add("DESTINATION", (sel->dest + (sel->eta.empty() ? "" : " \xC2\xB7 ETA " + sel->eta)).c_str());
        if (sel->len) aRows.addf("SIZE", P->text, "%d x %d m%s", sel->len, sel->beam, sel->draught > 0 ? (" \xC2\xB7 draught " + std::to_string(sel->draught).substr(0, 4)).c_str() : "");
        if (!sel->txt.empty()) aRows.add("TEXT", sel->txt.c_str());
        aRows.add("HEARD", fmtAge(sel->age).c_str());
    }
    aRows.end();
}

// ------------------------------------------------------------------ REMOTE
static lv_obj_t *rmTitle, *rmScreen, *rmActs;
static size_t rmShown = (size_t)-1;
static std::string rmPending;
static void onRmPage(lv_event_t* e) { send(proto::cmdScreen(2, M->screen.p + (int)(intptr_t)lv_event_get_user_data(e))); }
static void doAct(void*) { send(proto::cmdAct(rmPending)); }
static void onAct(lv_event_t* e) {
    const char* n = (const char*)lv_event_get_user_data(e);
    rmPending = n;
    std::string t = "The tracker runs \"" + std::string(lv_label_get_text(lv_obj_get_child(lv_event_get_target_obj(e), 0))) + "\" now.";
    confirm("Run on the tracker", t.c_str(), "RUN", doAct);
}
static void onActRefresh(lv_event_t*) { M->actions.clear(); M->actionsDone = false; rmShown = (size_t)-1; send(proto::cmdSimple("actlist")); }
static void buildRemote(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Remote screen", &tr);
    button(tr, "\xE2\x80\xB9 PAGE", onRmPage, (void*)(intptr_t)-1, 0, 48);
    button(tr, "PAGE \xE2\x80\xBA", onRmPage, (void*)(intptr_t)1, 0, 48);
    button(tr, "RELOAD MENU", onActRefresh, nullptr, 0, 48);
    lv_obj_t* body = row(root, GAP);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_height(body, 10);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* scrP = panel(body, 16, 0x000000, P->panelBorder);
    lv_obj_set_size(scrP, 470, LV_PCT(100));
    rmTitle = text(scrP, "", &app_mono_20, 0x3CE68A);
    rmScreen = text(scrP, "Open with the tracker linked", &app_mono_28, 0xE8EEF3);
    lv_obj_t* ap = panel(body, 12);
    lv_obj_set_flex_grow(ap, 1);
    lv_obj_set_width(ap, 10);
    lv_obj_set_height(ap, LV_PCT(100));
    lv_obj_set_scrollable(ap, true);
    section(ap, "EVERY MENU OPTION OF THE TRACKER");
    rmActs = col(ap, 8);
}
static void refreshRemote() {
    const proto::Screen& s = M->screen;
    if (s.ok) {
        setTextf(rmTitle, "%s   %s%s", s.title.c_str(), s.right.c_str(), s.on ? "" : "   (screen off)");
        std::string t;
        for (const auto& r : s.rows) { t += r; t += '\n'; }
        setText(rmScreen, t.c_str());
    }
    size_t n = 0;
    for (const auto& p : M->actions) n += p.items.size() + 1;
    if (n != rmShown) {
        rmShown = n;
        lv_obj_clean(rmActs);
        for (const auto& p : M->actions) {
            text(rmActs, p.title.c_str(), &app_bold_22, P->cyan);
            lv_obj_t* r = row(rmActs, 6);
            lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
            lv_obj_set_style_pad_row(r, 6, 0);
            for (const auto& a : p.items) {
                lv_obj_t* c = chip(r, a.l.empty() ? a.n.c_str() : a.l.c_str(), false, a.ok ? onAct : nullptr, (void*)a.n.c_str(), 42);
                if (!a.ok) lv_obj_set_style_opa(c, LV_OPA_50, 0);
            }
        }
        if (M->actions.empty()) text(rmActs, linked() ? "Reading the tracker's menus..." : "Tracker not linked.", &app_text_20, P->muted);
    }
}
static void enterRemote() { if (linked()) { send(proto::cmdScreen(2)); if (M->actions.empty()) send(proto::cmdSimple("actlist")); } }
static void leaveRemote() { if (linked()) send(proto::cmdScreen(0)); }

// ------------------------------------------------------------------ CONFIG
static JsonDocument* cfgDoc = nullptr;
struct Leaf { std::string path; std::string value; bool secret, isStr, isBool; };
static std::vector<Leaf> leaves;
static std::vector<std::string> edited;
static uint32_t cfgAsked = 0;
static bool cfgParsed = false;
static List cList;
static lv_obj_t *cState, *cEditKey, *cEditVal, *cFilter;
static int cSel = -1;
static std::vector<std::string> upload;
static size_t upIdx = 0;
static uint32_t upOk = 0, upErr = 0, upAt = 0;

static bool secretKey(const std::string& k) {
    std::string l = k;
    for (char& c : l) c = (char)tolower((unsigned char)c);
    return l.find("pass") != std::string::npos || l.find("pin") != std::string::npos || l.find("token") != std::string::npos ||
           l.find("apikey") != std::string::npos || l.find("secret") != std::string::npos || (l.find("key") != std::string::npos && l.find("keyer") == std::string::npos);
}
static void flatten(JsonVariant v, const std::string& path) {
    if (v.is<JsonObject>()) { for (JsonPair kv : v.as<JsonObject>()) flatten(kv.value(), path.empty() ? kv.key().c_str() : path + "." + kv.key().c_str()); return; }
    if (v.is<JsonArray>()) { int i = 0; for (JsonVariant x : v.as<JsonArray>()) flatten(x, path + "[" + std::to_string(i++) + "]"); return; }
    Leaf l;
    l.path = path;
    l.isStr = v.is<const char*>();
    l.isBool = v.is<bool>();
    if (l.isBool) l.value = v.as<bool>() ? "true" : "false";
    else if (l.isStr) l.value = v.as<const char*>();
    else { std::string s; serializeJson(v, s); l.value = s; }
    size_t dot = path.find_last_of('.');
    l.secret = l.isStr && secretKey(dot == std::string::npos ? path : path.substr(dot + 1));
    leaves.push_back(l);
}
static JsonVariant at(const std::string& path) {
    JsonVariant v = cfgDoc->as<JsonVariant>();
    size_t i = 0;
    while (i < path.size()) {
        if (path[i] == '[') { size_t e = path.find(']', i); v = v[atoi(path.substr(i + 1, e - i - 1).c_str())]; i = e + 1; if (i < path.size() && path[i] == '.') i++; continue; }
        size_t e = path.find_first_of(".[", i);
        std::string k = path.substr(i, e == std::string::npos ? std::string::npos : e - i);
        v = v[k];
        i = e == std::string::npos ? path.size() : (path[e] == '.' ? e + 1 : e);
    }
    return v;
}
static void requestCfg() {
    if (!linked()) return;
    if (!cfgDoc) cfgDoc = new JsonDocument();
    cfgParsed = false; leaves.clear(); edited.clear(); cSel = -1;
    M->cfgText.clear(); M->cfgDone = false; M->cfgTotal = 0;
    cfgAsked = 0;
    send(proto::Cmd("cfgget").num("o", 0).str());
}
static lv_obj_t* makeLeaf(lv_obj_t* p) {
    lv_obj_t* r = row(p, 10);
    lv_obj_set_height(r, 44);
    lv_obj_set_style_pad_hor(r, 8, 0);
    lv_obj_set_style_radius(r, 8, 0);
    lv_obj_set_style_bg_color(r, C(P->selected), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_t* k = text(r, "", &app_cond_18, P->muted);
    lv_obj_set_width(k, 420);
    oneLine(k);
    lv_obj_t* v = text(r, "", &app_mono_20, P->text);
    lv_obj_set_flex_grow(v, 1);
    lv_obj_set_width(v, 10);
    oneLine(v);
    return r;
}
static std::vector<int> cView;
static void onLeaf(lv_event_t* e) {
    int i = clickIndex(e);
    if (i < 0 || i >= (int)cView.size()) return;
    cSel = cView[i];
    setText(cEditKey, leaves[cSel].path.c_str());
    lv_textarea_set_text(cEditVal, leaves[cSel].secret ? "" : leaves[cSel].value.c_str());
    lv_textarea_set_placeholder_text(cEditVal, leaves[cSel].secret ? "(hidden: write a new value to change it)" : "value");
    keyboardFor(cEditVal);
}
static void onLeafSet(lv_event_t*) {
    if (cSel < 0) return;
    Leaf& l = leaves[cSel];
    std::string nv = lv_textarea_get_text(cEditVal);
    if (l.secret && nv.empty()) return;
    JsonVariant v = at(l.path);
    if (l.isBool) { if (nv != "true" && nv != "false") { error("Write true or false"); return; } v.set(nv == "true"); }
    else if (l.isStr) v.set(nv);
    else {
        char* end = nullptr;
        double d = strtod(nv.c_str(), &end);
        if (!end || *end) { error("Write a number"); return; }
        if (nv.find('.') == std::string::npos) v.set((long)d); else v.set(d);
    }
    l.value = nv;
    if (std::find(edited.begin(), edited.end(), l.path) == edited.end()) edited.push_back(l.path);
    requestRefresh();
}
static void onLeafReady(lv_event_t* e) { if (lv_event_get_code(e) == LV_EVENT_READY) onLeafSet(e); }
static void doUpload(void*) {
    std::string s;
    serializeJson(*cfgDoc, s);
    upload = proto::chunked("cfgset", s, true);
    upIdx = 0; upOk = M->okSeq; upErr = M->errSeq; upAt = nowMs();
    send(upload[upIdx++]);
}
static void onCfgSave(lv_event_t*) {
    if (!cfgParsed) { error("Read the configuration first"); return; }
    if (edited.empty()) { notice("Nothing changed"); return; }
    std::string t = std::to_string(edited.size()) + " value(s) changed. The whole file is uploaded and the tracker restarts.";
    confirm("Save the tracker configuration", t.c_str(), "SAVE", doUpload, nullptr, true);
}
static void onCfgRead(lv_event_t*) { requestCfg(); }
static void onFilter(lv_event_t*) { requestRefresh(); }
static void buildConfig(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Tracker configuration", &tr);
    cState = text(tr, "", &app_cond_18, P->muted);
    button(tr, "READ", onCfgRead, nullptr, 0, 48);
    button(tr, "SAVE AND RESTART", onCfgSave, nullptr, 0, 48, P->amber);
    lv_obj_t* fr = row(root, 10);
    cFilter = field(fr, "Filter keys (wifi, beacon, lora...)", 360, 40);
    lv_obj_add_event_cb(cFilter, onFilter, LV_EVENT_VALUE_CHANGED, nullptr);
    cEditKey = text(fr, "Touch a key to change it", &app_cond_18, P->cyan);
    lv_obj_set_flex_grow(cEditKey, 1);
    lv_obj_set_width(cEditKey, 10);
    oneLine(cEditKey);
    lv_obj_t* er = row(root, 10);
    cEditVal = field(er, "value", 0, 200);
    lv_obj_set_flex_grow(cEditVal, 1);
    lv_obj_add_event_cb(cEditVal, onLeafReady, LV_EVENT_READY, nullptr);
    button(er, "SET", onLeafSet, nullptr, 120, 52, P->green);
    lv_obj_t* sa = scrollArea(root);
    cList.create(sa, makeLeaf, onLeaf);
}
static void refreshConfig() {
    // lectura: se pide el siguiente trozo cuando llega el anterior
    if (!cfgParsed && cfgDoc && M->cfgTotal && !M->cfgDone && M->cfgText.size() > cfgAsked) {
        cfgAsked = (uint32_t)M->cfgText.size();
        send(proto::Cmd("cfgget").num("o", cfgAsked).str());
    }
    if (!cfgParsed && cfgDoc && M->cfgDone) {
        cfgDoc->clear();
        if (deserializeJson(*cfgDoc, M->cfgText) == DeserializationError::Ok) { leaves.clear(); flatten(cfgDoc->as<JsonVariant>(), ""); cfgParsed = true; }
        else error("The configuration file could not be read");
    }
    // subida: un trozo por 'ok'
    if (!upload.empty()) {
        if (M->errSeq != upErr) { upload.clear(); error("The tracker refused the configuration"); }
        else if (M->okSeq != upOk) {
            upOk = M->okSeq; upAt = nowMs();
            if (upIdx < upload.size()) send(upload[upIdx++]);
            else { upload.clear(); edited.clear(); notice("Configuration saved: the tracker restarts"); }
        } else if (nowMs() - upAt > 8000) { upload.clear(); error("No answer from the tracker while uploading"); }
    }
    char b[96];
    if (!upload.empty()) snprintf(b, sizeof b, "uploading %u / %u", (unsigned)upIdx, (unsigned)upload.size());
    else if (cfgParsed) snprintf(b, sizeof b, "%u bytes \xC2\xB7 %d keys \xC2\xB7 %d changed", (unsigned)M->cfgText.size(), (int)leaves.size(), (int)edited.size());
    else if (M->cfgTotal) snprintf(b, sizeof b, "reading %u / %u", (unsigned)M->cfgText.size(), (unsigned)M->cfgTotal);
    else snprintf(b, sizeof b, linked() ? "touch READ" : "tracker not linked");
    setText(cState, b);
    std::string f = lv_textarea_get_text(cFilter);
    for (char& c : f) c = (char)tolower((unsigned char)c);
    cView.clear();
    for (size_t i = 0; i < leaves.size(); i++) {
        if (!f.empty()) { std::string p = leaves[i].path; for (char& c : p) c = (char)tolower((unsigned char)c); if (p.find(f) == std::string::npos) continue; }
        cView.push_back((int)i);
        if (cView.size() >= 150) break;
    }
    cList.begin();
    for (int i : cView) {
        const Leaf& l = leaves[i];
        lv_obj_t* r = cList.next();
        setText(lv_obj_get_child(r, 0), l.path.c_str());
        const bool ed = std::find(edited.begin(), edited.end(), l.path) != edited.end();
        setText(lv_obj_get_child(r, 1), l.secret && !ed ? "********" : l.value.c_str());
        setColor(lv_obj_get_child(r, 1), ed ? P->amber : l.isBool ? (l.value == "true" ? P->green : P->muted) : P->text);
        lv_obj_set_style_bg_opa(r, i == cSel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(r, C(P->selected), 0);
    }
    cList.end();
}
static void enterConfig() { if (!cfgParsed) requestCfg(); }

// ------------------------------------------------------------------ FIRMWARE
static Rows fRows;
static lv_obj_t *fBar, *fInstall, *fNote;
static void onFwCheck(lv_event_t*) {
    if (!M->info.ok) { error("Link the tracker first"); return; }
    if (!net7::wifi().connected) { error("Needs WiFi (Settings > WiFi)"); return; }
    net7::fwCheck(M->info.board);
}
static void doInstall(void*) { net7::fwInstall(M->info.board); }
static void onFwInstall(lv_event_t*) {
    if (!M->info.capOta) { error("This board has no second firmware slot: update it with the web installer over USB"); return; }
    std::string t = "The display downloads " + net7::fw().latest + " with its WiFi, checks it and sends it to the tracker over Bluetooth. Keep both on until it restarts.";
    confirm("Update the tracker firmware", t.c_str(), "UPDATE", doInstall, nullptr, true);
}
static void onFwCancel(lv_event_t*) { net7::fwCancel(); }
static void buildFirmware(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Tracker firmware", &tr);
    button(tr, "CHECK", onFwCheck, nullptr, 0, 48, P->cyan);
    fInstall = button(tr, "UPDATE", onFwInstall, nullptr, 0, 48, P->amber);
    button(tr, "CANCEL", onFwCancel, nullptr, 0, 48);
    lv_obj_t* p = panel(root, 18);
    fRows.create(p, 200);
    fBar = lv_bar_create(p);
    lv_obj_set_size(fBar, LV_PCT(100), 24);
    lv_obj_set_style_bg_color(fBar, C(P->subPanel), 0);
    lv_obj_set_style_bg_color(fBar, C(P->green), LV_PART_INDICATOR);
    lv_bar_set_range(fBar, 0, 1000);
    fNote = wrap(p, "", &app_text_18, P->muted);
}
static void refreshFirmware() {
    net7::Fw f = net7::fw();
    fRows.begin();
    fRows.add("INSTALLED", M->info.ok ? M->info.fw.c_str() : "-");
    fRows.add("BOARD", M->info.ok ? M->info.board.c_str() : "-");
    fRows.add("UPDATE OVER BLE", M->info.ok ? (M->info.capOta ? "yes (second slot)" : "no: use the web installer (USB)") : "-", M->info.capOta ? P->green : P->amber);
    fRows.add("PUBLISHED", f.latest.empty() ? "touch CHECK" : f.latest.c_str(), !f.latest.empty() && f.latest != M->info.fw ? P->amber : P->text);
    fRows.add("STATE", f.text.empty() ? "-" : f.text.c_str(), f.phase == 6 ? P->red : f.phase == 5 ? P->green : P->text);
    fRows.end();
    show(fBar, f.phase == 3 || f.phase == 4);
    if (f.total) lv_bar_set_value(fBar, (int32_t)((uint64_t)f.done * 1000 / f.total), LV_ANIM_OFF);
    setText(fNote, "Published versions: ea7jrs.github.io/APRS-LoRa-Plus. The display downloads the image over WiFi, checks its size and SHA-256 and "
                   "sends it in blocks; the tracker checks it again, activates it and restarts.");
}

// ------------------------------------------------------------------ SETTINGS
static lv_obj_t *gPin, *gPeer, *gSsid, *gPass, *gDxHost, *gDxPort, *gDxCall, *gOwm, *gWxMin, *gBack, *gWxTr, *gWxRot;
static Rows gLink;
static void onSaveTracker(lv_event_t*) {
    const char* p = lv_textarea_get_text(gPin);
    if (strlen(p) != 6) { error("The PIN has 6 digits"); return; }
    trk::setPin((uint32_t)atol(p));
    trk::setPeerAddress(lv_textarea_get_text(gPeer));
    trk::reconnect();
    notice("Linking again with the new settings");
}
static void onForget(lv_event_t*) { trk::forgetPairing(); notice("Pairings forgotten: it pairs again with the PIN"); }
static void onReconnect(lv_event_t*) { trk::reconnect(); }
static void onSaveWifi(lv_event_t*) {
    cfg::get().wifiSsid = lv_textarea_get_text(gSsid);
    std::string pw = lv_textarea_get_text(gPass);
    if (!pw.empty()) cfg::get().wifiPass = pw;
    cfg::save();
    net7::wifiApply();
    lv_textarea_set_text(gPass, "");
    notice(cfg::get().wifiSsid.empty() ? "WiFi off" : "Connecting to the WiFi...");
}
static void onSaveDx(lv_event_t*) {
    cfg::get().dxHost = lv_textarea_get_text(gDxHost);
    cfg::get().dxPort = atoi(lv_textarea_get_text(gDxPort));
    cfg::get().dxCall = upper(lv_textarea_get_text(gDxCall));
    cfg::save();
    notice("DX cluster saved");
}
static lv_obj_t *gArc, *gSd, *gUnit[3];
static void onUnits(lv_event_t* e) { cfg::get().units = (int)(intptr_t)lv_event_get_user_data(e); cfg::save(); requestRefresh(); }
static void onSaveArc(lv_event_t*) {
    std::string k = lv_textarea_get_text(gArc);
    if (k.empty()) return;
    cfg::get().arcgisKey = k;
    cfg::save();
    mapt::setKey(k);
    lv_textarea_set_text(gArc, "");
    notice("ArcGIS key saved");
}
static void onSaveWx(lv_event_t*) {
    std::string k = lv_textarea_get_text(gOwm);
    if (!k.empty()) cfg::get().owmKey = k;
    int m = atoi(lv_textarea_get_text(gWxMin));
    cfg::get().wxMinutes = m < 10 ? 10 : m;
    cfg::save();
    lv_textarea_set_text(gOwm, "");
    notice("Internet weather saved");
}
static void onWxToggle(lv_event_t* e) {
    if ((intptr_t)lv_event_get_user_data(e)) cfg::get().wxToRotor = !cfg::get().wxToRotor; else cfg::get().wxToTracker = !cfg::get().wxToTracker;
    cfg::save();
    requestRefresh();
}
static void onBacklight(lv_event_t* e) {
    int& b = cfg::get().backlight;
    b += (int)(intptr_t)lv_event_get_user_data(e);
    b = b < 10 ? 10 : b > 100 ? 100 : b;
    board::setBacklight(b);
    cfg::save();
    requestRefresh();
}
static void onKeysDefault(lv_event_t*) {
    static const char* d[5] = {"SLIST", "MSG", "BCN", "DISP", "ROTOR"};
    for (int i = 0; i < 5; i++) cfg::get().keys[i] = d[i];
    cfg::save();
    refreshTouchKeys();
}
static void buildSettings(lv_obj_t* root) {
    header(root, "Settings", nullptr);
    lv_obj_t* sa = scrollArea(root);
    lv_obj_t* r1 = row(sa, GAP);
    lv_obj_set_flex_align(r1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* t = card(r1, "TRACKER LINK (BLUETOOTH)");
    lv_obj_set_width(t, 560);
    gLink.create(t, 150);
    wrap(t, "Control PIN: tracker WebUI > Bluetooth > Control PIN (123456 by default).", &app_text_18, P->muted);
    lv_obj_t* tr = row(t, 8);
    gPin = field(tr, "PIN", 150, 6, "0123456789");
    gPeer = field(tr, "Address (empty = first tracker)", 0, 17);
    lv_obj_set_flex_grow(gPeer, 1);
    lv_obj_t* tb = row(t, 8);
    button(tb, "SAVE AND LINK", onSaveTracker, nullptr, 0, 52, P->green);
    button(tb, "FORGET PAIRING", onForget, nullptr, 0, 52);
    button(tb, "RELINK", onReconnect, nullptr, 0, 52);
    lv_obj_t* w = card(r1, "WIFI");
    lv_obj_set_flex_grow(w, 1);
    lv_obj_set_width(w, 10);
    wrap(w, "For the DX cluster, internet weather, tracker firmware and the rotor box over TCP.", &app_text_18, P->muted);
    gSsid = field(w, "Network name (SSID)", 0, 32);
    lv_obj_set_width(gSsid, LV_PCT(100));
    gPass = field(w, "Password (empty = keep)", 0, 63);
    lv_obj_set_width(gPass, LV_PCT(100));
    lv_textarea_set_password_mode(gPass, true);
    button(w, "SAVE AND CONNECT", onSaveWifi, nullptr, 0, 52, P->green);
    lv_obj_t* r2 = row(sa, GAP);
    lv_obj_set_flex_align(r2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* d = card(r2, "DX CLUSTER");
    lv_obj_set_width(d, 490);
    lv_obj_t* dr = row(d, 8);
    gDxHost = field(dr, "Node", 0, 63);
    lv_obj_set_flex_grow(gDxHost, 1);
    gDxPort = field(dr, "Port", 110, 5, "0123456789");
    lv_obj_t* dr2 = row(d, 8);
    gDxCall = field(dr2, "Login callsign (empty = the tracker's)", 0, 12, nullptr, true);
    lv_obj_set_flex_grow(gDxCall, 1);
    button(dr2, "SAVE", onSaveDx, nullptr, 0, 52, P->green);
    lv_obj_t* x = card(r2, "INTERNET WEATHER (OPENWEATHERMAP)");
    lv_obj_set_flex_grow(x, 1);
    lv_obj_set_width(x, 10);
    gOwm = field(x, "API key (empty = keep)", 0, 64);
    lv_obj_set_width(gOwm, LV_PCT(100));
    lv_obj_t* xr = row(x, 8);
    text(xr, "Every", &app_cond_18, P->muted);
    gWxMin = field(xr, "15", 90, 3, "0123456789");
    text(xr, "min", &app_cond_18, P->muted);
    button(xr, "SAVE", onSaveWx, nullptr, 0, 52, P->green);
    lv_obj_t* xr2 = row(x, 8);
    gWxTr = chip(xr2, "TO THE TRACKER", false, onWxToggle, (void*)(intptr_t)0, 42, P->green);
    gWxRot = chip(xr2, "TO THE ROTOR BOX (wind)", false, onWxToggle, (void*)(intptr_t)1, 42, P->green);
    lv_obj_t* r3 = row(sa, GAP);
    lv_obj_set_flex_align(r3, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_t* s = card(r3, "DISPLAY");
    lv_obj_set_width(s, 490);
    lv_obj_t* br = row(s, 12);
    button(br, "-", onBacklight, (void*)(intptr_t)-10, 70, 52);
    gBack = text(br, "", &app_semi_20, P->text);
    button(br, "+", onBacklight, (void*)(intptr_t)10, 70, 52);
    button(s, "DEFAULT TOUCH KEYS", onKeysDefault, nullptr, 0, 52);
    lv_obj_t* mc = card(r3, "MAP");
    lv_obj_set_flex_grow(mc, 1);
    lv_obj_set_width(mc, 10);
    wrap(mc, "OpenStreetMap, IGN and OpenSeaMap need only WiFi. Esri imagery needs your own ArcGIS API key (free tier, developers.arcgis.com).", &app_text_18, P->muted);
    lv_obj_t* mr = row(mc, 8);
    gArc = field(mr, "ArcGIS API key (empty = keep)", 0, 200);
    lv_obj_set_flex_grow(gArc, 1);
    button(mr, "SAVE", onSaveArc, nullptr, 0, 52, P->green);
    gSd = wrap(mc, "", &app_text_18, P->muted);
    lv_obj_t* ur = row(mc, 8);
    text(ur, "UNITS", &app_bold_18, P->muted);
    static const char* un[] = {"METRIC", "NAUTICAL", "IMPERIAL"};
    for (int i = 0; i < 3; i++) gUnit[i] = chip(ur, un[i], false, onUnits, (void*)(intptr_t)i);
    wrap(s, "Theme (dark / light): side menu. Touch keys: hold a key in the [F] menu.", &app_text_18, P->muted);
}
static void refreshSettings() {
    gLink.begin();
    gLink.add("STATE", trk::stateText(), linked() ? P->green : P->amber);
    gLink.add("TRACKER", linked() ? trk::peerName().c_str() : "-");
    gLink.add("FIRMWARE", M->info.ok ? M->info.fw.c_str() : "-");
    gLink.end();
    auto fill = [](lv_obj_t* ta, const std::string& v) { if (!lv_obj_has_state(ta, LV_STATE_FOCUSED) && !*lv_textarea_get_text(ta) && !v.empty()) lv_textarea_set_text(ta, v.c_str()); };
    char b[16];
    snprintf(b, sizeof b, "%06lu", (unsigned long)trk::pin());
    fill(gPin, b);
    fill(gPeer, trk::peerAddress());
    fill(gSsid, cfg::get().wifiSsid);
    fill(gDxHost, cfg::get().dxHost);
    fill(gDxPort, std::to_string(cfg::get().dxPort));
    fill(gDxCall, cfg::get().dxCall);
    fill(gWxMin, std::to_string(cfg::get().wxMinutes));
    lv_textarea_set_placeholder_text(gOwm, cfg::get().owmKey.empty() ? "API key (openweathermap.org, free)" : "API key saved (empty = keep)");
    lv_textarea_set_placeholder_text(gPass, cfg::get().wifiPass.empty() ? "Password" : "Password saved (empty = keep)");
    lv_textarea_set_placeholder_text(gArc, cfg::get().arcgisKey.empty() ? "ArcGIS API key" : "ArcGIS key saved (empty = keep)");
    setText(gSd, net7::sdOk() ? "microSD: tiles are saved in /maps and work without WiFi." : "No microSD (or the map not opened yet): tiles are kept only while the display is on.");
    for (int i = 0; i < 3; i++) chipSet(gUnit[i], cfg::get().units == i);
    chipSet(gWxTr, cfg::get().wxToTracker, P->green);
    chipSet(gWxRot, cfg::get().wxToRotor, P->green);
    setTextf(gBack, "Backlight %d %%", cfg::get().backlight);
}

// ------------------------------------------------------------------ ABOUT
static void buildAbout(lv_obj_t* root) {
    header(root, "About", nullptr);
    lv_obj_t* sa = scrollArea(root);
    lv_obj_t* p = panel(sa, 20);
    lv_obj_t* h = row(p, 16);
    lv_obj_t* ic = lv_image_create(h);
    lv_image_set_src(ic, &img_app_icon);
    lv_image_set_scale(ic, 512);
    lv_obj_set_size(ic, 80, 80);
    lv_image_set_inner_align(ic, LV_IMAGE_ALIGN_CENTER);
    lv_obj_t* c = col(h, 2);
    lv_obj_set_width(c, LV_SIZE_CONTENT);
    text(c, "APRS LoRa+ Display", &app_bold_34, P->text);
    text(c, "Version " DISPLAY_VERSION " \xC2\xB7 " BOARD_ENV, &app_text_20, P->cyan);
    text(c, "APRS LoRa+ \xC2\xB7 \xC2\xA9 2026 Manuel - EA7JRS", &app_text_20, P->subText);
    wrap(p, "Free software under the GNU General Public License, version 3 or later. Project: github.com/EA7JRS/APRS-LoRa-Plus-Display", &app_text_20, P->subText);
    lv_obj_t* k = panel(sa, 20);
    section(k, "CREDITS");
    static const char* cr[] = {
        "LVGL 9.6 (MIT)", "NimBLE-Arduino (Apache 2.0)", "ArduinoJson (MIT)", "Arduino-ESP32 / ESP-IDF (Apache 2.0)",
        "Barlow and Barlow Condensed by Jeremy Tribby, Share Tech Mono by Carrois Apostrophe (SIL Open Font License 1.1)",
        "DejaVu fonts (Bitstream Vera / DejaVu licence)", "APRS symbols: aprs.fi / hessu (aprs-symbols; licences per symbol, CC BY-SA 2.0 and others)",
        "Interface of the APRS LoRa+ Android app, inspired by how mobile radios are operated (own design)",
        "DX cluster: DX Spider / AR-Cluster nodes such as EA4URE"};
    for (const char* s : cr) wrap(k, s, &app_text_18, P->subText);
}

void initTools() {
    registerScreen(AIS, {"AIS", true, buildAis, refreshAis, requestAis, nullptr});
    registerScreen(REMOTE, {"Remote", true, buildRemote, refreshRemote, enterRemote, leaveRemote});
    registerScreen(CONFIG, {"Config", false, buildConfig, refreshConfig, enterConfig, nullptr});
    registerScreen(FIRMWARE, {"Firmware", true, buildFirmware, refreshFirmware, nullptr, nullptr});
    registerScreen(SETTINGS, {"Settings", false, buildSettings, refreshSettings, nullptr, nullptr});
    registerScreen(ABOUT, {"About", true, buildAbout, nullptr, nullptr, nullptr});
}

} // namespace r7
#endif
