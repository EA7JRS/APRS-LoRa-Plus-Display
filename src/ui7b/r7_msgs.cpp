/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Mensajes APRS del registro del tracker (MessageScreens.kt de la app): conversaciones por estacion, conversacion con burbujas
 * y estado (enviado, esperando ack, ack, fallido), escribir y responder; avisos DAPNET que recibe el tracker.
 */
#ifdef UI_RADIO
#include "r7.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>

namespace r7 {

static std::string hhmm(uint32_t ep) {
    if (!ep) return "";
    char b[16];
    snprintf(b, sizeof b, "%02u:%02u", (unsigned)(ep / 3600 % 24), (unsigned)(ep / 60 % 60));
    return b;
}
static uint32_t stColor(int st) { return st == 4 ? P->green : st == 5 ? P->red : st == 3 ? P->amber : P->muted; }
static void requestAllMsgs() { if (linked()) { send(proto::cmdMsgs(0, 20)); send(proto::cmdMsgs(20, 20)); send(proto::cmdMsgs(40, 20)); } }

// ------------------------------------------------------------------ lista de conversaciones
struct Conv { std::string call; const proto::Msg* last; int n, unread; };
static std::vector<Conv> convs;
static List cList;
static lv_obj_t *cEmpty, *cTo, *cCount;
static void buildConvs() {
    convs.clear();
    std::map<std::string, size_t> idx;
    for (const proto::Msg& m : M->msgs) {                          // de la mas reciente a la mas antigua
        if (m.call.empty()) continue;
        auto it = idx.find(m.call);
        if (it == idx.end()) { idx[m.call] = convs.size(); convs.push_back({m.call, &m, 0, 0}); it = idx.find(m.call); }
        Conv& c = convs[it->second];
        c.n++;
        if (m.dir == 'R' && m.st == 0) c.unread++;
    }
}
static lv_obj_t* makeConvRow(lv_obj_t* p) {
    lv_obj_t* r = row(p, 12);
    lv_obj_set_height(r, 70);
    lv_obj_set_style_pad_hor(r, 14, 0);
    lv_obj_set_style_radius(r, 10, 0);
    lv_obj_set_style_bg_color(r, C(P->panel), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(r, C(P->selected), LV_STATE_PRESSED);
    lv_obj_t* c = col(r, 2);
    lv_obj_set_flex_grow(c, 1);
    lv_obj_set_width(c, 10);
    lv_obj_t* top = row(c, 10);
    text(top, "", &app_bold_22, P->text);
    lv_obj_t* badge = text(top, "", &app_bold_18, P->bg);
    lv_obj_set_style_bg_color(badge, C(P->amber), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_pad_hor(badge, 8, 0);
    lv_obj_t* last = text(c, "", &app_text_18, P->muted);
    oneLine(last);
    lv_obj_set_width(last, LV_PCT(100));
    lv_obj_t* rc = col(r, 2);
    lv_obj_set_width(rc, 150);
    lv_obj_set_flex_align(rc, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    text(rc, "", &app_cond_18, P->muted);
    text(rc, "", &app_cond_16, P->muted);
    return r;
}
static void onConv(lv_event_t* e) { int i = clickIndex(e); if (i >= 0 && i < (int)convs.size()) go(THREAD, convs[i].call); }
static void onNew(lv_event_t*) {
    std::string to = upper(lv_textarea_get_text(cTo));
    if (to.empty()) { error("Write the callsign first (EA7XXX-9)"); return; }
    lv_textarea_set_text(cTo, "");
    go(THREAD, to);
}
static void onToReady(lv_event_t* e) { if (lv_event_get_code(e) == LV_EVENT_READY) onNew(e); }
static void onPagerKey(lv_event_t*) { go(PAGER); }

static void buildMsgs(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Messages", &tr);
    cCount = text(tr, "", &app_cond_18, P->muted);
    button(tr, "DAPNET", onPagerKey, nullptr, 0, 48);
    lv_obj_t* nr = row(root, 10);
    cTo = field(nr, "New message to... (EA7XXX-9)", 420, 9, nullptr, true);
    lv_obj_add_event_cb(cTo, onToReady, LV_EVENT_READY, nullptr);
    button(nr, "WRITE", onNew, nullptr, 0, 52, P->cyan);
    lv_obj_t* sa = scrollArea(root);
    cEmpty = wrap(sa, "", &app_text_20, P->muted);
    cList.create(sa, makeConvRow, onConv);
}
static void refreshMsgs() {
    buildConvs();
    setTextf(cCount, "%d in the tracker \xC2\xB7 %d new", M->msgTot, M->msgUnread);
    show(cEmpty, convs.empty());
    setText(cEmpty, linked() ? "No messages in the tracker's log yet." : "Link the tracker (Settings) to see and send messages.");
    cList.begin();
    char b[24];
    for (const Conv& c : convs) {
        lv_obj_t* r = cList.next();
        lv_obj_t* cc = lv_obj_get_child(r, 0);
        lv_obj_t* top = lv_obj_get_child(cc, 0);
        setText(lv_obj_get_child(top, 0), c.call.c_str());
        lv_obj_t* badge = lv_obj_get_child(top, 1);
        snprintf(b, sizeof b, "%d", c.unread);
        setText(badge, b);
        show(badge, c.unread > 0);
        std::string last = (c.last->dir == 'T' ? "You: " : "") + c.last->text;
        setText(lv_obj_get_child(cc, 1), last.c_str());
        lv_obj_t* rc = lv_obj_get_child(r, 1);
        setText(lv_obj_get_child(rc, 0), hhmm(c.last->ep).c_str());
        lv_obj_t* st = lv_obj_get_child(rc, 1);
        setText(st, c.last->dir == 'T' ? proto::msgStateName(c.last->st) : (std::to_string(c.n) + " msgs").c_str());
        setColor(st, c.last->dir == 'T' ? stColor(c.last->st) : P->muted);
    }
    cList.end();
}

// ------------------------------------------------------------------ conversacion
static List tList;
static lv_obj_t *tArea, *tText, *tEmpty, *tInfo, *tCount;
static std::vector<const proto::Msg*> tView;
static size_t tShown = 0;
static lv_obj_t* makeBubble(lv_obj_t* p) {
    lv_obj_t* r = row(p, 0);
    lv_obj_t* b = col(r, 2);
    lv_obj_set_width(b, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(b, 640, 0);
    lv_obj_set_style_pad_all(b, 12, 0);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_t* t = text(b, "", &app_text_24, P->text);
    lv_label_set_long_mode(t, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_max_width(t, 616, 0);
    lv_obj_set_width(t, LV_SIZE_CONTENT);
    text(b, "", &app_cond_16, P->muted);
    return r;
}
static void doSend() {
    std::string t = lv_textarea_get_text(tText);
    if (t.empty()) return;
    send(proto::cmdMsgSend(arg(), t, true));
    lv_textarea_set_text(tText, "");
    requestAllMsgs();
    notice("Message handed to the tracker: it goes out with the next window");
}
static void onSend(lv_event_t*) { doSend(); }
static void onTextReady(lv_event_t* e) { if (lv_event_get_code(e) == LV_EVENT_READY) doSend(); }
static void onQuick(lv_event_t* e) { lv_textarea_set_text(tText, (const char*)lv_event_get_user_data(e)); }
static void onStationOf(lv_event_t*) { go(STATION, arg()); }

static void buildThread(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "Conversation", &tr);
    tCount = text(tr, "", &app_cond_18, P->muted);
    button(tr, "STATION", onStationOf, nullptr, 0, 48);
    tInfo = text(root, "", &app_bold_26, P->cyan);
    tArea = scrollArea(root);
    tEmpty = wrap(tArea, "No messages with this station yet. Write the first one below.", &app_text_20, P->muted);
    tList.create(tArea, makeBubble, nullptr);
    lv_obj_t* qr = row(root, 8);
    static const char* quick[] = {"QSL", "73", "OK", "Copy, thanks", "QRV?", "Where are you?"};
    for (const char* q : quick) chip(qr, q, false, onQuick, (void*)q, 40);
    lv_obj_t* sr = row(root, 10);
    tText = field(sr, "Message (max 67 characters)", 0, 67);
    lv_obj_set_flex_grow(tText, 1);
    lv_obj_add_event_cb(tText, onTextReady, LV_EVENT_READY, nullptr);
    button(sr, "SEND", onSend, nullptr, 150, 54, P->green);
}
static void refreshThread() {
    setText(tInfo, arg().c_str());
    tView.clear();
    for (const proto::Msg& m : M->msgs) if (m.call == arg()) tView.push_back(&m);
    std::reverse(tView.begin(), tView.end());                       // la mas antigua arriba
    setTextf(tCount, "%d messages", (int)tView.size());
    show(tEmpty, tView.empty());
    tList.begin();
    for (const proto::Msg* m : tView) {
        lv_obj_t* r = tList.next();
        const bool out = m->dir == 'T';
        lv_obj_set_flex_align(r, out ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* b = lv_obj_get_child(r, 0);
        lv_obj_set_style_bg_color(b, C(out ? P->greenBg : P->panel), 0);
        lv_obj_set_style_border_color(b, C(out ? P->greenDim : P->panelBorder), 0);
        lv_obj_set_style_border_width(b, 1, 0);
        setText(lv_obj_get_child(b, 0), m->text.c_str());
        lv_obj_t* meta = lv_obj_get_child(b, 1);
        std::string s = hhmm(m->ep) + (m->ep ? " UTC" : "");
        if (out) s += std::string(s.empty() ? "" : " \xC2\xB7 ") + proto::msgStateName(m->st);
        setText(meta, s.c_str());
        setColor(meta, out ? stColor(m->st) : P->muted);
    }
    tList.end();
    if (tView.size() != tShown) { tShown = tView.size(); lv_obj_update_layout(tArea); lv_obj_scroll_to_y(tArea, LV_COORD_MAX, LV_ANIM_OFF); }
}
static void enterThread() { tShown = (size_t)-1; requestAllMsgs(); }

// ------------------------------------------------------------------ DAPNET
static List pList;
static lv_obj_t* pEmpty;
static lv_obj_t* makePage(lv_obj_t* p) {
    lv_obj_t* r = panel(p, 12);
    lv_obj_t* h = row(r, 10);
    text(h, "", &app_bold_22, P->text);
    lv_obj_t* sp = box(h);
    lv_obj_set_flex_grow(sp, 1);
    text(h, "", &app_cond_18, P->muted);
    wrap(r, "", &app_text_24, P->text);
    return r;
}
static void onPagerRefresh(lv_event_t*) { send(proto::cmdSimple("dapnet")); }
static void buildPager(lv_obj_t* root) {
    lv_obj_t* tr;
    header(root, "DAPNET pager", &tr);
    button(tr, "REFRESH", onPagerRefresh, nullptr, 0, 48);
    lv_obj_t* sa = scrollArea(root);
    pEmpty = wrap(sa, "", &app_text_20, P->muted);
    pList.create(sa, makePage, nullptr);
}
static void refreshPager() {
    show(pEmpty, M->pages.empty());
    setText(pEmpty, M->status.mode == 5 ? "No pages yet. The tracker is listening to DAPNET." :
                    "The tracker receives DAPNET pages in DAPNET mode (Tracker > Mode). The ones it has kept are shown here.");
    pList.begin();
    for (const proto::Page& p : M->pages) {
        lv_obj_t* r = pList.next();
        lv_obj_t* h = lv_obj_get_child(r, 0);
        char b[64];
        snprintf(b, sizeof b, "RIC %lu%s", (unsigned long)p.ric, p.own ? "  \xC2\xB7  MINE" : "");
        setText(lv_obj_get_child(h, 0), b);
        setColor(lv_obj_get_child(h, 0), p.own ? P->amber : P->text);
        snprintf(b, sizeof b, "%d dBm \xC2\xB7 %s ago%s", p.rssi, fmtAge((int)p.age).c_str(), p.bad ? " \xC2\xB7 errors" : "");
        setText(lv_obj_get_child(h, 2), b);
        setText(lv_obj_get_child(r, 1), p.text.c_str());
    }
    pList.end();
}

void initMessages() {
    registerScreen(MSGS, {"Messages", false, buildMsgs, refreshMsgs, requestAllMsgs, nullptr});
    registerScreen(THREAD, {"Conversation", false, buildThread, refreshThread, enterThread, nullptr});
    registerScreen(PAGER, {"DAPNET", false, buildPager, refreshPager, []() { if (linked()) send(proto::cmdSimple("dapnet")); }, nullptr});
}

} // namespace r7
#endif
