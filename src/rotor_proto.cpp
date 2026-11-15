/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "rotor_proto.h"
#include <ArduinoJson.h>
#include <cstring>

namespace rotp {

static std::string S(JsonVariantConst v, const char* d = "") { const char* s = v | (const char*)nullptr; return s ? s : d; }
static std::string line(JsonDocument& d) { std::string s; serializeJson(d, s); s += '\n'; return s; }

bool apply(Model& m, const char* text) {
    JsonDocument d;
    if (deserializeJson(d, text) != DeserializationError::Ok) return false;
    const char* e = d["e"] | (const char*)nullptr;
    if (!e) return false;
    bool known = true;
    if (!strcmp(e, "info")) {
        Info& i = m.info;
        i = Info();
        i.ok = true; i.hasEl = d["hasEl"] | false; i.pin = d["pin"] | false; i.cat = d["cat"] | false; i.wind = d["wind"] | false;
        i.minAz = d["lim"]["minAz"] | 0.0f; i.maxAz = d["lim"]["maxAz"] | 360.0f; i.minEl = d["lim"]["minEl"] | 0.0f; i.maxEl = d["lim"]["maxEl"] | 90.0f;
        i.dev = S(d["dev"]); i.ver = S(d["ver"]); i.board = S(d["board"]); i.name = S(d["name"]); i.drv = S(d["drv"]); i.call = S(d["call"]);
        i.ip = S(d["ip"]); i.rotctld = d["rotctld"] | 0; i.rigctld = d["rigctld"] | 0; i.up = d["up"] | 0u;
        i.capCal = d["caps"]["cal"] | false; i.capMove = d["caps"]["move"] | true;
    } else if (!strcmp(e, "pos")) {
        Pos& p = m.pos;
        p.ok = true; p.az = d["az"] | 0.0f; p.hasEl = !d["el"].isNull(); p.el = d["el"] | 0.0f; p.st = d["st"] | 0; p.moving = d["mv"] | false;
        p.fault = S(d["f"], "OK"); p.maz = d["maz"] | p.az; p.hasTarget = !d["taz"].isNull(); p.taz = d["taz"] | 0.0f; p.tel = d["tel"] | 0.0f;
        p.aprs = S(d["aprs"], p.aprs.c_str());
        JsonObjectConst w = d["wind"];
        p.wind = Wind();
        if (!w.isNull()) {
            Wind& x = p.wind;
            x.ok = true; x.on = w["on"] | false; x.park = w["park"] | 0.0f; x.locked = w["lk"] | false; x.ws = w["ws"] | -1.0f; x.wg = w["wg"] | -1.0f;
            x.wd = w["wd"] | -1; x.peak = w["pk"] | -1.0f; x.age = w["age"] | 0; x.src = S(w["src"]); x.hold = w["hold"] | 0; x.local = w["local"] | false;
        }
    } else if (!strcmp(e, "cat")) {
        Cat& c = m.cat;
        c.ok = true; c.rig = S(d["rig"]); c.link = d["l"] | false; c.f = d["f"] | 0u; c.mode = S(d["m"]); c.tf = d["tf"] | 0u; c.split = d["sp"] | false;
        c.vfo = S(d["v"], "A"); c.ptt = d["ptt"] | false; c.sm = d["sm"] | -1; c.tone = d["tn"] | 0.0f; c.pttAllowed = d["pa"] | false;
        c.trackEn = d["tr"]["en"] | false; c.trackOn = d["tr"]["on"] | false; c.trackName = S(d["tr"]["n"]); c.dop = d["tr"]["dop"] | 0;
    } else if (!strcmp(e, "raw")) {
        Raw& r = m.raw;
        r.ok = true; r.link = d["link"] | false; r.hasAz = !d["az"].isNull(); r.hasEl = !d["el"].isNull(); r.az = d["az"] | 0; r.el = d["el"] | 0; r.unit = S(d["unit"]);
    } else if (!strcmp(e, "cfg")) {
        int i = d["i"] | 0, n = d["n"] | 0;
        if (i == 0) { m.cfgText.clear(); m.cfgGot = 0; m.cfgDone = false; }
        if (i == m.cfgGot) { m.cfgText += S(d["d"]); m.cfgGot++; }
        m.cfgN = n;
        m.cfgDone = n > 0 && m.cfgGot >= n;
    } else if (!strcmp(e, "ok")) {
        if (S(d["c"]) == "auth") { m.authOk = true; m.authFailed = false; }
        m.okSeq++;
    } else if (!strcmp(e, "err")) {
        std::string c = S(d["c"]), why = S(d["m"]);
        if (c == "auth") { m.authOk = false; m.authFailed = true; }
        if (why == "auth required") m.authOk = false;
        m.lastError = (c.empty() ? "" : c + ": ") + why;
        m.errSeq++;
    } else {
        known = false;
    }
    m.revision++;
    return known;
}

std::string cmd(const char* c) { JsonDocument d; d["c"] = c; return line(d); }
std::string auth(const std::string& pin) { JsonDocument d; d["c"] = "auth"; d["pin"] = pin; return line(d); }
std::string sub(int ms) { JsonDocument d; d["c"] = "sub"; d["ms"] = ms; return line(d); }
std::string goTo(float az, float el, bool withEl) {
    JsonDocument d; d["c"] = "pos"; d["az"] = (int)(az * 10 + (az >= 0 ? 0.5f : -0.5f)) / 10.0; if (withEl) d["el"] = (int)(el * 10 + 0.5f) / 10.0; return line(d);
}
std::string move(const char* dir) { JsonDocument d; d["c"] = "move"; d["d"] = dir; return line(d); }
std::string catSet(const char* k, double v) { JsonDocument d; d["c"] = "catset"; d[k] = v; return line(d); }
std::string catSetText(const char* k, const std::string& v) { JsonDocument d; d["c"] = "catset"; d[k] = v; return line(d); }
std::string catSetFlag(const char* k, bool v) { JsonDocument d; d["c"] = "catset"; d[k] = v; return line(d); }
std::string catTrack(bool on, const std::string& name, uint32_t dl, uint32_t ul, double rr) {
    JsonDocument d; d["c"] = "cattrack"; d["on"] = on;
    if (on) { d["name"] = name; d["dl"] = dl; d["ul"] = ul; d["rr"] = rr; }
    return line(d);
}
std::string wx(float ws, float wg, int wd, int age, const std::string& src) {
    JsonDocument d; d["c"] = "wx"; d["ws"] = ws; if (wg >= 0) d["wg"] = wg; if (wd >= 0) d["wd"] = wd; d["age"] = age; d["src"] = src; return line(d);
}
std::string cal(const char* axis, const char* pt, float deg) { JsonDocument d; d["c"] = "cal"; d["axis"] = axis; d["pt"] = pt; d["deg"] = deg; return line(d); }
std::string here(const char* axis, float deg) { JsonDocument d; d["c"] = "here"; d["axis"] = axis; d["deg"] = deg; return line(d); }

std::vector<std::string> cfgSet(const std::string& json) {
    std::vector<std::string> pieces;
    size_t pos = 0;
    while (pos < json.size()) {
        size_t end = pos + 180 < json.size() ? pos + 180 : json.size();
        while (end > pos + 1 && end < json.size() && ((uint8_t)json[end] & 0xC0) == 0x80) end--;
        pieces.push_back(json.substr(pos, end - pos));
        pos = end;
    }
    std::vector<std::string> out;
    for (size_t i = 0; i < pieces.size(); i++) {
        JsonDocument d; d["c"] = "cfgset"; d["i"] = (int)i; d["n"] = (int)pieces.size(); d["d"] = pieces[i];
        out.push_back(line(d));
    }
    return out;
}

std::string satMode(const std::string& mode) {
    std::string u;
    for (char c : mode) u += (char)toupper((unsigned char)c);
    if (u.find("FM") != std::string::npos && u.find("AFSK") == std::string::npos) return "FM";
    if (u.find("SSB") != std::string::npos || u.find("USB") != std::string::npos || u.find("LINEAR") != std::string::npos) return "USB";
    if (u.find("LSB") != std::string::npos) return "LSB";
    if (u.find("CW") != std::string::npos) return "CW";
    if (u.find("AFSK") != std::string::npos || u.find("APRS") != std::string::npos) return "FM";
    if (u.find("FSK") != std::string::npos || u.find("BPSK") != std::string::npos || u.find("GMSK") != std::string::npos) return "PKTUSB";
    return "FM";
}

std::string sLabel(int sm) {
    if (sm < 0) return "S-";
    if (sm <= 9) return "S" + std::to_string(sm);
    return "S9+" + std::to_string((sm - 9) * 10);
}

} // namespace rotp
