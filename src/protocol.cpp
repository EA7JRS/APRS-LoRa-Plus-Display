/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "protocol.h"
#include <ArduinoJson.h>
#include <cstring>

namespace proto {

static std::string S(JsonVariantConst v, const char* d = "") { const char* s = v | (const char*)nullptr; return s ? s : d; }

static std::string line(JsonDocument& d) { std::string s; serializeJson(d, s); s += '\n'; return s; }

bool applyEvent(Model& m, const char* text) {
    JsonDocument d;
    if (deserializeJson(d, text) != DeserializationError::Ok) return false;
    const char* e = d["e"] | (const char*)nullptr;
    if (!e) return false;
    bool known = true;

    if (!strcmp(e, "info")) {
        Info& i = m.info;
        i.ok = true; i.fw = S(d["fw"]); i.board = S(d["board"]); i.mode = d["mode"] | 0;
        i.capOta = d["caps"]["ota"] | false; i.capRotor = d["caps"]["rotor"] | false; i.capNmea = d["caps"]["nmea"] | false;
        i.modeNames.clear(); i.modeIds.clear();
        for (JsonObjectConst o : d["modes"].as<JsonArrayConst>()) { i.modeIds.push_back(o["m"] | 0); i.modeNames.push_back(S(o["n"])); }
    } else if (!strcmp(e, "status")) {
        Status& s = m.status;
        s.ok = true; s.mode = d["mode"] | 0; s.profile = d["profile"] | 0; s.lora = d["lora"] | 0;
        s.digi = d["digi"] | false; s.smart = d["smart"] | false; s.sos = d["sos"] | false;
        s.batt = d["batt"] | -1.0f; s.up = d["up"] | 0u;
    } else if (!strcmp(e, "sensors")) {
        Sensors& s = m.sensors;
        s.ok = true;
        s.batt = d["batt"] | s.batt; s.temp = d["temp"] | s.temp; s.hum = d["hum"] | s.hum; s.press = d["press"] | s.press;
        s.qnh = d["qnh"] | s.qnh; s.baroAlt = d["baroAlt"] | s.baroAlt; s.heading = d["heading"] | s.heading;
        s.battPct = d["battPct"] | s.battPct;
        s.qnhSrc = S(d["qs"], s.qnhSrc.c_str());
        NetWx n;                                        // sin "net": no hay tiempo de internet (o ha caducado en el tracker)
        JsonObjectConst o = d["net"];
        if (!o.isNull()) {
            n.ok = true;
            n.src = S(o["src"]); n.place = S(o["pl"]); n.desc = S(o["ds"]); n.pollutant = S(o["mp"]); n.forecast = S(o["fc"]); n.d3 = o["d3"] | n.d3;
            n.t = o["t"] | n.t; n.h = o["h"] | n.h; n.p = o["p"] | n.p; n.ws = o["ws"] | n.ws; n.wg = o["wg"] | n.wg;
            n.rain = o["r"] | n.rain; n.pm25 = o["pm25"] | n.pm25; n.pm10 = o["pm10"] | n.pm10;
            n.wd = o["wd"] | n.wd; n.clouds = o["cl"] | n.clouds; n.aqiOwm = o["aq"] | 0; n.aqiUs = o["aqus"] | -1;
            n.age = o["age"] | 0u;
        }
        s.net = n;
    } else if (!strcmp(e, "gnss")) {
        Gnss& g = m.gnss;
        g.ok = true; g.fix = d["fix"] | false; g.lat = d["lat"] | g.lat; g.lon = d["lon"] | g.lon; g.alt = d["alt"] | g.alt;
        g.spd = d["spd"] | 0.0f; g.crs = d["crs"] | 0.0f; g.sats = d["sats"] | 0; g.hdop = d["hdop"] | 0.0f; g.utc = S(d["utc"]);
    } else if (!strcmp(e, "heard")) {
        m.heardTot = d["tot"] | 0;
        int from = 0;                                   // el tracker no devuelve 'from' en heard: la pagina se pide siempre desde 0
        std::vector<Heard> v;
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) {
            Heard h;
            h.call = S(o["c"]); h.path = S(o["path"]); h.cmt = S(o["cmt"]);
            std::string sy = S(o["sym"]); for (size_t k = 0; k < 2 && k < sy.size(); k++) h.sym[k] = sy[k];
            h.hasPos = o["lat"].is<double>() && o["lon"].is<double>();
            h.lat = o["lat"] | 0.0; h.lon = o["lon"] | 0.0;
            h.rssi = o["rssi"] | 0; h.snr = o["snr"] | 0.0f; h.age = o["age"] | 0; h.n = o["n"] | 0;
            v.push_back(h);
        }
        (void)from;
        m.heard = v;
    } else if (!strcmp(e, "msgs")) {
        m.msgTot = d["tot"] | 0; m.msgUnread = d["unread"] | 0;
        std::vector<Msg> v;
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) {
            Msg g;
            g.i = o["i"] | 0; std::string dd = S(o["d"], "R"); g.dir = dd.empty() ? 'R' : dd[0];
            g.ep = o["ep"] | 0u; g.st = o["st"] | 0; g.call = S(o["c"]); g.text = S(o["t"]);
            v.push_back(g);
        }
        m.msgs = v;
    } else if (!strcmp(e, "msg")) {
        m.msgTot = d["tot"] | m.msgTot; m.msgUnread = d["unread"] | m.msgUnread;   // la pantalla vuelve a pedir la lista
    } else if (!strcmp(e, "sat")) {
        Sat& s = m.sat;
        s.ok = d["ok"] | false; s.name = S(d["n"]); s.az = d["az"] | 0.0f; s.el = d["el"] | 0.0f; s.rng = d["rng"] | 0.0f;
        s.aos = d["aos"] | 0u; s.los = d["los"] | 0u; s.maxEl = d["max"] | 0;
    } else if (!strcmp(e, "rot")) {
        Rot& r = m.rot;
        r.ok = true; r.trk = d["trk"] | false; r.az = d["az"] | 0.0f; r.el = d["el"] | 0.0f; r.km = d["km"] | 0.0f;
        r.src = d["src"] | 0; r.name = S(d["name"]);
    } else if (!strcmp(e, "screen")) {
        Screen& s = m.screen;
        s.ok = true; s.title = S(d["t"]); s.right = S(d["r"]); s.rows.clear();
        for (JsonVariantConst r : d["rows"].as<JsonArrayConst>()) s.rows.push_back(S(r));
    } else if (!strcmp(e, "page")) {
        Page p; p.ric = d["ric"] | 0u; p.own = d["own"] | false; p.rssi = d["rssi"] | 0; p.text = S(d["t"]);
        m.pages.insert(m.pages.begin(), p); if (m.pages.size() > 20) m.pages.pop_back();
    } else if (!strcmp(e, "dapnet")) {
        m.pages.clear();
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) {
            Page p; p.ric = o["ric"] | 0u; p.own = o["own"] | false; p.rssi = o["rssi"] | 0; p.text = S(o["t"]);
            m.pages.push_back(p);
        }
    } else if (!strcmp(e, "alertsv")) {
        m.alertsVerTracker = d["v"] | 0u;
        if ((d["n"] | 0) == 0) { m.alerts.clear(); m.alertsTot = 0; m.alertsVer = m.alertsVerTracker; }
    } else if (!strcmp(e, "alerts")) {
        const int from = d["from"] | 0;
        m.alertsTot = d["tot"] | 0;
        if (from == 0) m.alerts.clear();
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) {
            plotter::Alert a;
            a.id = S(o["id"]); a.src = plotter::alertSrcFromCode(S(o["s"])); a.level = (uint8_t)(o["lv"] | 0);
            a.event = S(o["ev"]); a.area = S(o["ar"]); a.text = S(o["tx"]);
            a.onset = a.sent = o["on"] | (int64_t)0; a.expires = o["ex"] | (int64_t)0;
            if (o["la"].is<double>() && o["lo"].is<double>()) { a.lat = o["la"] | 0.0; a.lon = o["lo"] | 0.0; }
            if (!a.id.empty()) m.alerts.push_back(a);
        }
        m.alertsPending = (int)m.alerts.size() < m.alertsTot && !d["items"].as<JsonArrayConst>().isNull() && d["items"].size() > 0;
        if (!m.alertsPending) m.alertsVer = d["v"] | m.alertsVerTracker;
    } else if (!strcmp(e, "spacewx")) {
        plotter::SpaceWx& w = m.space;
        w = plotter::SpaceWx();
        w.g = d["g"] | -1; w.r = d["r"] | -1; w.s = d["s"] | -1; w.gMax = d["gm"] | -1;
        for (JsonVariantConst k : d["kp"].as<JsonArrayConst>()) if (w.kpCount < 8) w.kp[w.kpCount++] = k | 0.0f;
        w.kpTime = d["kt"] | (int64_t)0;
        w.sfi = d["sfi"] | -1.0f; w.bz = d["bz"] | 1000.0f; w.bt = d["bt"] | -1.0f; w.windKms = d["ws"] | -1.0f;
        w.xray = S(d["xr"]);
        m.spaceOk = (d["ok"] | false) && w.valid();
    } else if (!strcmp(e, "err")) {
        m.lastError = S(d["c"]) + ": " + S(d["msg"]);
    } else if (!strcmp(e, "ok")) {
        // confirmacion de una orden: nada que guardar
    } else {
        known = false;
    }
    m.revision++;
    return known;
}

std::string cmdSimple(const char* c) { JsonDocument d; d["c"] = c; return line(d); }
std::string cmdSub(int s, int g) { JsonDocument d; d["c"] = "sub"; d["sensors"] = s; d["gnss"] = g; return line(d); }
std::string cmdHeard(int from, int cnt) { JsonDocument d; d["c"] = "heard"; d["from"] = from; d["cnt"] = cnt; return line(d); }
std::string cmdMsgs(int from, int cnt) { JsonDocument d; d["c"] = "msgs"; d["from"] = from; d["cnt"] = cnt; return line(d); }
std::string cmdMsgSend(const std::string& to, const std::string& text, bool ack) {
    JsonDocument d; d["c"] = "msg"; d["to"] = to; d["t"] = text.substr(0, 67); d["ack"] = ack; return line(d);
}
std::string cmdMode(int mode) { JsonDocument d; d["c"] = "mode"; d["m"] = mode; return line(d); }
std::string cmdProfile(int i) { JsonDocument d; d["c"] = "profile"; d["i"] = i; return line(d); }
std::string cmdLora(int i) { JsonDocument d; d["c"] = "lora"; d["i"] = i; return line(d); }
std::string cmdScreen(int every) { JsonDocument d; d["c"] = "screen"; d["every"] = every; return line(d); }
std::string cmdSatSel(int i) { JsonDocument d; d["c"] = "satsel"; d["i"] = i; return line(d); }
std::string cmdAlerts(int from, int cnt) { JsonDocument d; d["c"] = "alerts"; d["from"] = from; d["cnt"] = cnt; return line(d); }
std::string cmdAct(const std::string& name) { JsonDocument d; d["c"] = "act"; d["a"] = name; return line(d); }

} // namespace proto
