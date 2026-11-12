/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "protocol.h"
#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace proto {

static std::string S(JsonVariantConst v, const char* d = "") { const char* s = v | (const char*)nullptr; return s ? s : d; }
static std::string line(JsonDocument& d) { std::string s; serializeJson(d, s); s += '\n'; return s; }
static bool has(JsonVariantConst v) { return !v.isNull(); }

// Junta una pagina de una lista que el tracker manda por trozos ('from' = posicion de la primera); 'tot' recorta lo que sobra
template <class T> static void mergePage(std::vector<T>& all, int from, int tot, std::vector<T>&& page) {
    if (from <= 0) {                    // la primera pagina manda: lo de detras se conserva solo si sigue habiendo tanto
        if ((int)all.size() > tot) all.resize(tot < 0 ? 0 : tot);
        for (size_t i = 0; i < page.size(); i++) { if (i < all.size()) all[i] = std::move(page[i]); else all.push_back(std::move(page[i])); }
    } else {
        if ((int)all.size() < from) all.resize(from);
        for (size_t i = 0; i < page.size(); i++) {
            size_t k = from + i;
            if (k < all.size()) all[k] = std::move(page[i]); else all.push_back(std::move(page[i]));
        }
    }
    if (tot >= 0 && (int)all.size() > tot) all.resize(tot);
}

static Heard parseHeard(JsonObjectConst o) {
    Heard h;
    h.call = S(o["c"]); h.path = S(o["path"]); h.cmt = S(o["cmt"]); h.kind = S(o["k"]); h.dev = S(o["dev"]); h.sts = S(o["sts"]);
    std::string sy = S(o["sym"]); for (size_t k = 0; k < 2 && k < sy.size(); k++) h.sym[k] = sy[k];
    h.hasPos = o["lat"].is<double>() && o["lon"].is<double>();
    h.lat = o["lat"] | 0.0; h.lon = o["lon"] | 0.0;
    h.alt = o["alt"] | 0; h.spd = o["spd"] | 0; h.crs = o["crs"] | 0;
    h.rssi = o["rssi"] | 0; h.snr = o["snr"] | 0.0f; h.age = o["age"] | 0; h.n = o["n"] | 0; h.pa = o["pa"] | -1;
    JsonObjectConst w = o["wx"];
    if (!w.isNull()) {
        h.wx.ok = true;
        h.wx.t = w["t"] | NA; h.wx.h = w["h"] | NA; h.wx.p = w["p"] | NA; h.wx.wd = w["wd"] | NA;
        h.wx.ws = w["ws"] | NA; h.wx.wg = w["wg"] | NA; h.wx.r = w["r"] | NA;
    }
    return h;
}

static Msg parseMsg(JsonObjectConst o) {
    Msg g;
    g.i = o["i"] | 0; std::string dd = S(o["d"], "R"); g.dir = dd.empty() ? 'R' : dd[0];
    g.ep = o["ep"] | 0u; g.st = o["st"] | 0; g.ack = o["ack"] | 0; g.call = S(o["c"]); g.text = S(o["t"]);
    return g;
}

static Page parsePage(JsonObjectConst o) {
    Page p;
    p.ric = o["ric"] | 0u; p.func = o["f"] | 0; p.own = o["own"] | false; p.bad = o["bad"] | false; p.age = o["age"] | 0u;
    p.rssi = o["rssi"] | 0; p.text = S(o["t"]);
    return p;
}

static Vessel parseVessel(JsonObjectConst o) {
    Vessel v;
    v.mmsi = o["m"] | 0u; v.cl = S(o["cl"]); v.name = S(o["n"]); v.call = S(o["c"]); v.type = o["t"] | 0; v.st = o["st"] | 0;
    v.hasPos = o["lat"].is<double>() && o["lon"].is<double>(); v.lat = o["lat"] | 0.0; v.lon = o["lon"] | 0.0;
    v.pa = o["pa"] | -1; v.sog = o["sog"] | -1.0f; v.cog = o["cog"] | -1.0f; v.hdg = o["hdg"] | -1; v.age = o["age"] | 0;
    v.nm = o["nm"] | -1.0f; v.brg = o["brg"] | -1; v.cpa = o["cpa"] | -1.0f; v.tcpa = o["tcpa"] | -1.0f; v.danger = o["dng"] | false;
    v.dest = S(o["ds"]); v.len = o["len"] | 0; v.beam = o["bm"] | 0; v.draught = o["dr"] | -1.0f; v.eta = S(o["eta"]); v.imo = o["imo"] | 0;
    v.alt = o["alt"] | -1; v.txt = S(o["txt"]);
    return v;
}

static void parseFm(Fm& f, JsonObjectConst o) {
    f.ok = true;
    f.f = o["f"] | f.f; f.tx = o["tx"] | f.tx; f.sh = o["sh"] | f.sh; f.ofs = o["ofs"] | f.ofs; f.tm = o["tm"] | f.tm; f.t = o["t"] | f.t;
    f.d = o["d"] | f.d; f.inv = o["inv"] | f.inv; f.w = o["w"] | f.w; f.hp = o["hp"] | f.hp; f.n = o["n"] | f.n; f.m = o["m"] | f.m;
    f.i = o["i"] | f.i; f.step = o["step"] | f.step; f.sq = o["sq"] | f.sq; f.sqm = o["sqm"] | f.sqm; f.sqa = o["sqa"] | f.sqa;
    f.nr = o["nr"] | f.nr; f.edit = o["edit"] | f.edit; f.busy = o["busy"] | false; f.ptt = o["ptt"] | false; f.txs = o["txs"] | f.txs;
    f.rssi = o["rssi"] | 0; f.scan = o["scan"] | 0; f.arts = o["arts"] | 0; f.eai = o["eai"] | 0; f.aprs = o["aprs"] | f.aprs;
    f.qsyb = o["qsyb"] | f.qsyb; f.tok = o["tok"] | f.tok; f.spk = o["spk"] | f.spk; f.note = S(o["note"]);
    JsonObjectConst s = o["sat"];
    f.sat = FmSat();
    if (!s.isNull()) {
        FmSat& t = f.sat;
        t.ok = true; t.n = S(s["n"]); t.f = s["f"] | 0u; t.up = s["up"] | 0u; t.dop = s["dop"] | 0; t.xup = s["xup"] | false;
        t.aprs = s["aprs"] | false; t.tone = s["tone"] | 0; t.sent = s["sent"] | 0; t.heard = s["heard"] | 0;
        t.az = s["az"] | 0.0f; t.el = s["el"] | 0.0f; t.txok = s["txok"] | false; t.why = S(s["why"]);
    }
}

static FmMem parseMem(JsonObjectConst o) {
    FmMem m;
    m.i = o["i"] | 0; m.n = S(o["n"]); m.f = o["f"] | 0u; m.sh = o["sh"] | 0; m.ofs = o["ofs"] | 0; m.tm = o["tm"] | 0; m.t = o["t"] | 0;
    m.d = o["d"] | 0; m.inv = o["inv"] | false; m.w = o["w"] | true; m.hp = o["hp"] | false;
    return m;
}

bool applyEvent(Model& m, const char* text) {
    JsonDocument d;
    if (deserializeJson(d, text) != DeserializationError::Ok) return false;
    const char* e = d["e"] | (const char*)nullptr;
    if (!e) return false;
    bool known = true;

    if (!strcmp(e, "info")) {
        Info& i = m.info;
        i = Info();
        i.ok = true; i.v = d["v"] | 0; i.fw = S(d["fw"]); i.board = S(d["board"]); i.mode = d["mode"] | 0;
        JsonObjectConst c = d["caps"];
        i.capOta = c["ota"] | false; i.capRotor = c["rotor"] | false; i.capNmea = c["nmea"] | false; i.capCw = c["cw"] | false;
        i.capDapnet = c["dapnet"] | false; i.capRkey = c["rkey"] | false; i.capGpsaid = c["gpsaid"] | false; i.capM17 = c["m17"] | false;
        i.capFm = c["fm"] | false; i.capSatFav = c["satFav"] | false; i.capSatFm = c["satFm"] | false; i.capWxnet = c["wxnet"] | false;
        i.capAis = c["ais"] | false; i.capNetreq = c["netreq"] | false;
        i.radio = S(c["radio"], "lora"); i.bcast = S(c["bcast"], "none"); i.satMax = c["satMax"] | 64000;
        for (JsonObjectConst o : d["modes"].as<JsonArrayConst>()) { i.modeIds.push_back(o["m"] | 0); i.modeNames.push_back(S(o["n"])); }
        i.profile = d["profile"] | 0; i.lora = d["lora"] | 0;
        for (JsonObjectConst o : d["profiles"].as<JsonArrayConst>()) i.profiles.push_back({S(o["call"]), S(o["label"])});
        for (JsonObjectConst o : d["loras"].as<JsonArrayConst>()) i.loras.push_back({o["f"] | 0u, o["sf"] | 0, o["cr"] | 0, o["fsk"] | false});
        for (JsonVariantConst s : d["cwMsgs"].as<JsonArrayConst>()) i.cwMsgs.push_back(S(s));
        JsonObjectConst sa = d["sa868"];
        if (!sa.isNull()) {
            Sa868& r = i.sa868;
            r.present = true; r.rev = sa["rev"] | 0; r.fw = S(sa["fw"]); r.band = S(sa["band"]); r.ok = sa["ok"] | false; r.modem = S(sa["modem"]);
            r.rx = sa["rx"] | 0u; r.tx = sa["tx"] | 0u; r.m17 = sa["m17"] | 0u; r.dapnet = sa["dapnet"] | 0u; r.spkEsp = sa["spkEsp"] | false;
            r.txul = sa["txul"] | false; r.txulav = sa["txulav"] | false; r.mac = S(sa["mac"]);
        }
        i.rst = S(d["rst"]); i.rstAt = S(d["rstAt"]); i.rstTask = S(d["rstTask"]); i.pmuOff = S(d["pmuOff"]);
    } else if (!strcmp(e, "status")) {
        Status& s = m.status;
        s.ok = true; s.mode = d["mode"] | 0; s.profile = d["profile"] | 0; s.lora = d["lora"] | 0;
        s.digi = d["digi"] | false; s.smart = d["smart"] | false; s.sos = d["sos"] | false; s.bleKiss = d["bleKiss"] | true;
        s.batt = d["batt"] | -1.0f; s.up = d["up"] | 0u; s.f = d["f"] | 0u; s.mod = S(d["mod"]);
    } else if (!strcmp(e, "sensors")) {
        Sensors& s = m.sensors;
        s.ok = true;
        s.batt = d["batt"] | s.batt; s.temp = d["temp"] | s.temp; s.hum = d["hum"] | s.hum; s.press = d["press"] | s.press;
        s.qnh = d["qnh"] | s.qnh; s.baroAlt = d["baroAlt"] | s.baroAlt; s.heading = d["heading"] | s.heading;
        s.battPct = d["battPct"] | s.battPct; s.imuTemp = d["imuTemp"] | s.imuTemp; s.boardTemp = d["boardTemp"] | s.boardTemp;
        s.gas = d["gas"] | s.gas; s.iaq = d["iaq"] | s.iaq; s.vbus = d["vbus"] | s.vbus; s.pmuTemp = d["pmuTemp"] | s.pmuTemp;
        s.battMa = d["battMa"] | s.battMa; s.heap = d["heap"] | s.heap;
        if (has(d["moving"])) s.moving = (d["moving"] | false) ? 1 : 0;
        if (has(d["charging"])) s.charging = (d["charging"] | false) ? 1 : 0;
        if (has(d["flat"])) s.flat = (d["flat"] | false) ? 1 : 0;
        if (has(d["magCal"])) s.magCal = (d["magCal"] | false) ? 1 : 0;
        for (int k = 0; k < 3; k++) {
            if (d["accel"].is<JsonArrayConst>()) { s.hasAccel = true; s.accel[k] = d["accel"][k] | 0.0f; }
            if (d["gyro"].is<JsonArrayConst>()) { s.hasGyro = true; s.gyro[k] = d["gyro"][k] | 0.0f; }
        }
        s.qnhSrc = S(d["qs"], s.qnhSrc.c_str()); s.iaqLabel = S(d["iaqLabel"], s.iaqLabel.c_str());
        s.wxSensor = S(d["wxSensor"], s.wxSensor.c_str()); s.chg = S(d["chg"], s.chg.c_str());
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
        g.spd = d["spd"] | 0.0f; g.crs = d["crs"] | 0.0f; g.sats = d["sats"] | 0; g.hdop = d["hdop"] | 0.0f; g.age = d["age"] | 0u; g.utc = S(d["utc"]);
    } else if (!strcmp(e, "sky")) {
        Sky& k = m.sky;
        k = Sky();
        k.ok = true; k.pd = d["pd"] | -1.0f; k.hd = d["hd"] | -1.0f; k.vd = d["vd"] | -1.0f; k.err = d["err"] | -1.0f; k.used = d["used"] | -1;
        for (JsonArrayConst a : d["s"].as<JsonArrayConst>()) {
            SkySat s;
            std::string sys = S(a[0], "N"); s.sys = sys.empty() ? 'N' : sys[0];
            s.prn = a[1] | 0; s.el = a[2] | -1; s.az = a[3] | -1; s.snr = a[4] | -1; s.used = (a[5] | 0) != 0;
            k.sats.push_back(s);
        }
    } else if (!strcmp(e, "heard")) {
        m.heardTot = d["tot"] | 0; m.heardPkts = d["pkts"] | m.heardPkts; m.heardAtMs = m.now;
        std::vector<Heard> v;
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) v.push_back(parseHeard(o));
        mergePage(m.heard, d["from"] | 0, m.heardTot, std::move(v));
    } else if (!strcmp(e, "msgs")) {
        m.msgTot = d["tot"] | 0; m.msgUnread = d["unread"] | 0;
        std::vector<Msg> v;
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) v.push_back(parseMsg(o));
        mergePage(m.msgs, d["from"] | 0, m.msgTot, std::move(v));
    } else if (!strcmp(e, "msg")) {
        m.msgTot = d["tot"] | m.msgTot; m.msgUnread = d["unread"] | m.msgUnread;   // la pantalla vuelve a pedir la lista
        m.msgSeq++;
    } else if (!strcmp(e, "sat")) {
        Sat& s = m.sat;
        s.ok = d["ok"] | false; s.name = S(d["n"]); s.az = d["az"] | 0.0f; s.el = d["el"] | 0.0f; s.rng = d["rng"] | 0.0f;
        s.aos = d["aos"] | 0u; s.los = d["los"] | 0u; s.maxEl = d["max"] | 0; s.id = d["id"] | 0; s.i = d["i"] | -1; s.geo = d["geo"] | false;
        s.tx = d["tx"] | 0; s.tle = d["tle"] | -1.0f; s.pos = S(d["pos"]); s.rr = d["rr"] | 0.0f; s.fav = d["fav"] | false; s.cat = d["cat"] | 0;
        s.tot = d["tot"] | 0; s.nfav = d["nfav"] | 0; s.stamp = S(d["stamp"]);
    } else if (!strcmp(e, "satlist")) {
        m.satListTot = d["tot"] | 0; m.satListSel = d["sel"] | -1;
        std::vector<SatEntry> v;
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) {
            SatEntry s;
            s.i = o["i"] | 0; s.id = o["id"] | 0; s.name = S(o["n"]); s.c = S(o["c"]); s.t = o["t"] | 0; s.b = o["b"] | 0;
            s.fav = o["fav"] | false; s.in = o["in"] | true;
            v.push_back(s);
        }
        mergePage(m.satList, d["from"] | 0, m.satListTot, std::move(v));
    } else if (!strcmp(e, "sattxlist")) {
        m.satTxs.clear();
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) {
            SatTx t;
            t.i = o["i"] | 0; t.m = S(o["m"]); t.b = o["b"] | 0; t.d = S(o["d"]); t.up = o["up"] | 0u; t.dn = o["dn"] | 0u;
            t.rx = S(o["rx"], "n/a"); t.tx = S(o["tx"], "n/a"); t.rxd = o["rxd"] | 0u; t.txd = o["txd"] | 0u; t.tone = o["tone"] | 0.0f; t.aprs = o["aprs"] | false;
            m.satTxs.push_back(t);
        }
    } else if (!strcmp(e, "satrx")) {
        SatRx& r = m.satRx;
        r.on = d["on"] | false;
        if (r.on) {
            r.k = S(d["k"], r.k.c_str()); r.f = d["f"] | r.f; r.dop = d["dop"] | r.dop; r.fine = d["fine"] | r.fine; r.rssi = d["rssi"] | r.rssi;
            r.n = d["n"] | r.n; r.tx = d["tx"] | r.tx;
            std::string t = S(d["txt"]);
            if (!t.empty()) { r.lines.push_back(t); if (r.lines.size() > 30) r.lines.erase(r.lines.begin()); }
        }
    } else if (!strcmp(e, "rot")) {
        Rot& r = m.rot;
        r.ok = true; r.trk = d["trk"] | false; r.az = d["az"] | 0.0f; r.el = d["el"] | 0.0f; r.km = d["km"] | 0.0f;
        r.src = d["src"] | 0; r.name = S(d["name"]); r.mob = d["mob"] | false; r.abs = d["abs"] | 0.0f; r.hdg = d["hdg"] | 0.0f;
        r.hasAct = has(d["aaz"]); r.aaz = d["aaz"] | 0.0f; r.ael = d["ael"] | 0.0f;
    } else if (!strcmp(e, "targets")) {
        Targets& t = m.targets;
        t = Targets();
        t.ok = true; t.sel = d["sel"] | -1; t.src = d["src"] | 0; t.ff = S(d["ff"]); t.fm = d["fm"] | 0; t.pr = d["pr"] | false; t.pm = d["pm"] | 0;
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>())
            t.items.push_back({o["i"] | 0, S(o["n"]), o["lat"] | 0.0, o["lon"] | 0.0, o["alt"] | 0, o["f"] | 0u});
    } else if (!strcmp(e, "screen")) {
        Screen& s = m.screen;
        s.ok = true; s.title = S(d["t"]); s.right = S(d["r"]); s.p = d["p"] | 0; s.cur = d["cur"] | 0; s.on = d["on"] | true; s.rows.clear();
        for (JsonVariantConst r : d["rows"].as<JsonArrayConst>()) s.rows.push_back(S(r));
    } else if (!strcmp(e, "page")) {
        m.pages.insert(m.pages.begin(), parsePage(d.as<JsonObjectConst>()));
        if (m.pages.size() > 30) m.pages.pop_back();
    } else if (!strcmp(e, "dapnet")) {
        m.pages.clear();
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) m.pages.push_back(parsePage(o));
    } else if (!strcmp(e, "led")) {
        Leds& l = m.leds;
        l.ok = true; l.msg = d["msg"] | false; l.unread = d["unread"] | 0; l.emg = d["emg"] | false; l.flash = d["fl"] | false;
        l.ec = S(d["ec"]); l.et = S(d["et"]); l.ek = S(d["ek"]); l.em = d["em"] | false; l.eq = d["eq"] | l.eq; l.eo = d["eo"] | false;
    } else if (!strcmp(e, "rf")) {
        std::string s = S(d["s"]);
        if (s == "tx") m.rf.txSinceMs = m.now ? m.now : 1;
        else if (s == "idle") m.rf.txSinceMs = 0;
        else if (s == "rx") { m.rf.rxAtMs = m.now ? m.now : 1; m.rf.rssi = d["rssi"] | 0; m.rf.snr = d["snr"] | 0.0f; }
    } else if (!strcmp(e, "eai")) {
        Eai& a = m.eai;
        a.ok = true; a.sos = d["sos"] | 0; a.sent = d["sent"] | 0; a.left = d["left"] | 0; a.by = S(d["by"]); a.fm = d["fm"] | false;
        a.fmLeft = d["fmLeft"] | 0; a.fmBy = S(d["fmBy"]); a.fmMode = d["fmMode"] | 0; a.md = d["md"] | false; a.mdLeft = d["mdLeft"] | 0; a.mdMin = d["mdMin"] | 0;
    } else if (!strcmp(e, "cwrx")) {
        CwRx& c = m.cwrx;
        bool on = d["on"] | false;
        if (on && !c.on) c.text.clear();
        c.on = on;
        std::string t = S(d["t"]);
        if (!t.empty()) { c.text += t; if (c.text.size() > 600) c.text.erase(0, c.text.size() - 600); }
        c.dbm = d["dbm"] | c.dbm; c.snr = d["snr"] | c.snr; c.wpm = d["wpm"] | c.wpm; c.f = d["f"] | c.f;
    } else if (!strcmp(e, "fm")) {
        parseFm(m.fm, d.as<JsonObjectConst>());
    } else if (!strcmp(e, "fmmems")) {
        m.fmMemMax = d["max"] | 32;
        m.fmMems.clear();
        for (JsonObjectConst o : d["mem"].as<JsonArrayConst>()) m.fmMems.push_back(parseMem(o));
    } else if (!strcmp(e, "bcast")) {
        Bcast& b = m.bcast;
        b.ok = true; b.chip = S(d["chip"]); b.on = d["on"] | false; b.band = d["band"] | 0; b.f = d["f"] | 0u; b.vol = d["vol"] | 0;
        b.mute = d["mute"] | false; b.mono = d["mono"] | false; b.rssi = d["rssi"] | 0; b.snr = d["snr"] | 0; b.st = d["st"] | false;
        b.seek = d["seek"] | false; b.ps = S(d["ps"]); b.rt = S(d["rt"]);
        if (d["bands"].is<JsonArrayConst>()) { b.bands.clear(); for (JsonVariantConst v : d["bands"].as<JsonArrayConst>()) b.bands.push_back(v | 0); }
        if (d["presets"].is<JsonArrayConst>()) { b.presets.clear(); for (JsonVariantConst v : d["presets"].as<JsonArrayConst>()) b.presets.push_back(v | 0u); }
    } else if (!strcmp(e, "aislist")) {
        m.aisTot = d["tot"] | 0; m.aisMsgs = d["msgs"] | m.aisMsgs; m.aisSrc = S(d["src"], m.aisSrc.c_str()); m.aisCh = S(d["ch"], m.aisCh.c_str());
        std::vector<Vessel> v;
        for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) v.push_back(parseVessel(o));
        mergePage(m.ais, d["from"] | 0, m.aisTot, std::move(v));
    } else if (!strcmp(e, "ais")) {
        Vessel v = parseVessel(d.as<JsonObjectConst>());
        m.aisMsgs = d["msgs"] | m.aisMsgs;
        bool found = false;
        for (Vessel& x : m.ais) if (x.mmsi == v.mmsi) { x = v; found = true; break; }
        if (!found && v.mmsi) { m.ais.push_back(v); if ((int)m.ais.size() > m.aisTot) m.aisTot = (int)m.ais.size(); }
    } else if (!strcmp(e, "actlist")) {
        if (d["end"] | false) { m.actionsDone = true; }
        else {
            ActionPage p;
            p.p = d["p"] | 0; p.title = S(d["pt"]);
            for (JsonObjectConst o : d["items"].as<JsonArrayConst>()) p.items.push_back({S(o["n"]), S(o["l"]), o["ok"] | true});
            bool rep = false;
            for (ActionPage& x : m.actions) if (x.p == p.p) { x = p; rep = true; }
            if (!rep) m.actions.push_back(p);
        }
    } else if (!strcmp(e, "cfg")) {
        uint32_t o = d["o"] | 0u;
        m.cfgTotal = d["n"] | 0u;
        std::string piece = S(d["d"]);
        if (o == 0) { m.cfgText.clear(); m.cfgDone = false; }
        if (o == m.cfgText.size()) m.cfgText += piece;      // un trozo repetido o desordenado no se mezcla
        m.cfgDone = m.cfgText.size() >= m.cfgTotal;
    } else if (!strcmp(e, "ota")) {
        m.otaOff = d["off"] | m.otaOff; m.otaDone = d["done"] | false;
    } else if (!strcmp(e, "netreq")) {
        m.netReq = S(d["what"]);
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
        m.lastErrorCmd = S(d["c"]);
        m.lastError = S(d["c"]) + ": " + S(d["msg"]);
        m.errSeq++;
    } else if (!strcmp(e, "ok")) {
        m.lastOk = S(d["c"]);
        m.okSeq++;
    } else {
        known = false;
    }
    m.revision++;
    return known;
}

// ---------- ordenes ----------
Cmd::Cmd(const char* c) : doc_(new JsonDocument()) { (*(JsonDocument*)doc_)["c"] = c; }
Cmd::~Cmd() { delete (JsonDocument*)doc_; }
Cmd& Cmd::num(const char* k, double v) { (*(JsonDocument*)doc_)[k] = v; return *this; }
Cmd& Cmd::num(const char* k, int v) { (*(JsonDocument*)doc_)[k] = v; return *this; }
Cmd& Cmd::num(const char* k, uint32_t v) { (*(JsonDocument*)doc_)[k] = v; return *this; }
Cmd& Cmd::flag(const char* k, bool v) { (*(JsonDocument*)doc_)[k] = v; return *this; }
Cmd& Cmd::text(const char* k, const std::string& v) { (*(JsonDocument*)doc_)[k] = v; return *this; }
std::string Cmd::str() const { return line(*(JsonDocument*)doc_); }

std::string cmdSimple(const char* c) { JsonDocument d; d["c"] = c; return line(d); }
std::string cmdSub(int s, int g, int k) { JsonDocument d; d["c"] = "sub"; d["sensors"] = s; d["gnss"] = g; if (k) d["sky"] = k; return line(d); }
std::string cmdHeard(int from, int cnt) { JsonDocument d; d["c"] = "heard"; d["from"] = from; d["cnt"] = cnt; return line(d); }
std::string cmdMsgs(int from, int cnt) { JsonDocument d; d["c"] = "msgs"; d["from"] = from; d["cnt"] = cnt; return line(d); }
std::string cmdMsgSend(const std::string& to, const std::string& text, bool ack) {
    JsonDocument d; d["c"] = "msg"; d["to"] = to; d["t"] = text.substr(0, 67); d["ack"] = ack; return line(d);
}
std::string cmdMode(int mode) { JsonDocument d; d["c"] = "mode"; d["m"] = mode; return line(d); }
std::string cmdProfile(int i) { JsonDocument d; d["c"] = "profile"; d["i"] = i; return line(d); }
std::string cmdLora(int i) { JsonDocument d; d["c"] = "lora"; d["i"] = i; return line(d); }
std::string cmdScreen(int every, int page) { JsonDocument d; d["c"] = "screen"; d["every"] = every; if (page >= 0) d["p"] = page; return line(d); }
std::string cmdSatSel(int i) { JsonDocument d; d["c"] = "satsel"; d["i"] = i; return line(d); }
std::string cmdAlerts(int from, int cnt) { JsonDocument d; d["c"] = "alerts"; d["from"] = from; d["cnt"] = cnt; return line(d); }
std::string cmdAct(const std::string& name) { JsonDocument d; d["c"] = "act"; d["a"] = name; return line(d); }

std::vector<std::string> chunked(const char* name, const std::string& text, bool reboot) {
    std::vector<std::string> out;
    { JsonDocument d; d["c"] = name; d["s"] = "begin"; d["n"] = text.size(); if (!reboot) d["reboot"] = false; out.push_back(line(d)); }
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = pos + 180 < text.size() ? pos + 180 : text.size();
        // no cortar un caracter UTF-8 (bytes de continuacion 10xxxxxx) ni dejar la linea de mas de 255 bytes con los escapes JSON
        for (;;) {
            while (end > pos + 1 && end < text.size() && ((uint8_t)text[end] & 0xC0) == 0x80) end--;
            JsonDocument d; d["c"] = name; d["s"] = "c"; d["d"] = text.substr(pos, end - pos);
            std::string l = line(d);
            if (l.size() <= 250 || end - pos <= 8) { out.push_back(l); break; }
            end -= 16;
        }
        pos = end;
    }
    { JsonDocument d; d["c"] = name; d["s"] = "end"; out.push_back(line(d)); }
    return out;
}

// ---------- utilidades ----------
const char* msgStateName(int st) {
    switch (st) { case 0: return "NEW"; case 1: return "READ"; case 2: return "SENT"; case 3: return "WAITING ACK"; case 4: return "ACK"; case 5: return "FAILED"; default: return "?"; }
}

std::string modeName(const Model& m, int id) {
    for (size_t i = 0; i < m.info.modeIds.size() && i < m.info.modeNames.size(); i++)
        if (m.info.modeIds[i] == id) return m.info.modeNames[i];
    static const char* names[] = {"APRS LoRa", "APRS FSK", "MORSE", "WSPR", "LR-FHSS", "DAPNET", "FT8", "FT4", "FST4W", "SAT", "M17", "FM", "AIS"};
    if (id >= 0 && id < 13) return names[id];
    char b[16]; snprintf(b, sizeof b, "%d", id);
    return b;
}

double distanceKm(double la1, double lo1, double la2, double lo2) {
    const double R = 6371.0, k = M_PI / 180.0;
    double dla = (la2 - la1) * k, dlo = (lo2 - lo1) * k;
    double a = sin(dla / 2) * sin(dla / 2) + cos(la1 * k) * cos(la2 * k) * sin(dlo / 2) * sin(dlo / 2);
    return R * 2 * atan2(sqrt(a), sqrt(1 - a));
}
double bearingDeg(double la1, double lo1, double la2, double lo2) {
    const double k = M_PI / 180.0;
    double y = sin((lo2 - lo1) * k) * cos(la2 * k);
    double x = cos(la1 * k) * sin(la2 * k) - sin(la1 * k) * cos(la2 * k) * cos((lo2 - lo1) * k);
    double b = atan2(y, x) / k;
    return b < 0 ? b + 360 : b;
}
const char* cardinal16(double deg) {
    static const char* n[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
    int i = (int)floor(fmod(deg + 11.25 + 720.0, 360.0) / 22.5);
    return n[i & 15];
}

SReading sMeter(double dbm, uint32_t freqHz) {
    SReading r;
    double s9 = freqHz >= 30000000u ? -93.0 : -73.0;
    double diff = dbm - s9;
    if (diff >= 0) { r.s = 9; r.overDb = (int)lround(diff); }
    else { r.s = 9 + (int)floor(diff / 6.0); if (r.s < 0) r.s = 0; }
    r.segments = r.overDb > 0 ? 9 + (int)std::min(6.0, ceil(r.overDb / 10.0)) : r.s;
    char b[16];
    if (r.overDb > 0) snprintf(b, sizeof b, "S9+%d", r.overDb); else snprintf(b, sizeof b, "S%d", r.s);
    r.label = b;
    return r;
}

std::string mhz(uint32_t hz) { char b[16]; snprintf(b, sizeof b, "%u.%03u", (unsigned)(hz / 1000000u), (unsigned)(hz / 1000u % 1000u)); return b; }
std::string subKhz(uint32_t hz) { char b[8]; snprintf(b, sizeof b, "%03u", (unsigned)(hz % 1000u)); return b; }

std::string locator(double lat, double lon) {
    double lo = lon + 180.0, la = lat + 90.0;
    if (lo < 0) lo = 0;
    if (lo >= 360) lo = 359.9999;
    if (la < 0) la = 0;
    if (la >= 180) la = 179.9999;
    char b[7];
    b[0] = (char)('A' + (int)(lo / 20)); b[1] = (char)('A' + (int)(la / 10));
    b[2] = (char)('0' + (int)fmod(lo / 2, 10)); b[3] = (char)('0' + (int)fmod(la, 10));
    b[4] = (char)('a' + (int)fmod(lo * 12, 24)); b[5] = (char)('a' + (int)fmod(la * 24, 24)); b[6] = 0;
    return b;
}

} // namespace proto
