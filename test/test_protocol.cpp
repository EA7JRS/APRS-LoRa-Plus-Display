/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Prueba en el PC: g++ -std=c++17 -I src -I <ArduinoJson>/src test/test_protocol.cpp src/protocol.cpp -o /tmp/tp && /tmp/tp
#include "protocol.h"
#include <cstdio>
#include <cstring>
static int fails = 0, total = 0;
#define CHECK(c) do { total++; if (!(c)) { fails++; printf("FALLA %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
int main() {
    proto::Model m;
    CHECK(proto::applyEvent(m, R"J({"e":"info","v":1,"fw":"5.3.5","board":"ttgo_t_beam_s3_SUPREME_v3","caps":{"nmea":true,"ota":true,"rotor":true},"mode":0,"modes":[{"m":0,"n":"APRS LoRa"},{"m":9,"n":"Satellites"}]})J"));
    CHECK(m.info.ok && m.info.fw == "5.3.5" && m.info.capOta && m.info.capRotor && m.info.modeIds.size() == 2 && m.info.modeIds[1] == 9 && m.info.modeNames[1] == "Satellites");
    CHECK(proto::applyEvent(m, R"J({"e":"status","mode":0,"profile":1,"lora":0,"digi":true,"smart":true,"sos":false,"batt":4.05,"up":3600})J"));
    CHECK(m.status.profile == 1 && m.status.digi && m.status.up == 3600 && m.status.batt > 4.0f);
    CHECK(proto::applyEvent(m, R"J({"e":"sensors","batt":4.05,"temp":21.4,"hum":55.0,"press":1013.2,"battPct":88})J"));
    CHECK(m.sensors.temp > 21.3f && m.sensors.temp < 21.5f && m.sensors.battPct == 88 && m.sensors.qnh < 0);
    CHECK(proto::applyEvent(m, R"J({"e":"sensors","temp":22.0})J"));
    CHECK(m.sensors.press > 1013.0f);   // lo que no llega se conserva
    CHECK(!m.sensors.net.ok);
    CHECK(proto::applyEvent(m, R"J({"e":"sensors","qnh":1016.0,"qs":"net","net":{"src":"OWM","pl":"Sevilla","t":24.1,"p":1016.0,"ws":5.1,"wg":9.3,"wd":250,"aq":2,"age":420,"fc":"Fair, stable","d3":0.4}})J"));
    CHECK(m.sensors.qnhSrc == "net" && m.sensors.net.ok && m.sensors.net.src == "OWM" && m.sensors.net.place == "Sevilla");
    CHECK(m.sensors.net.forecast == "Fair, stable" && m.sensors.net.d3 > 0.39f && m.sensors.net.d3 < 0.41f);
    CHECK(m.sensors.net.wg > 9.2f && m.sensors.net.wd == 250 && m.sensors.net.aqiOwm == 2 && m.sensors.net.aqiUs == -1 && m.sensors.net.age == 420 && m.sensors.net.h < 0);
    CHECK(proto::applyEvent(m, R"J({"e":"sensors","temp":22.0})J"));
    CHECK(!m.sensors.net.ok && m.sensors.qnhSrc == "net");   // sin "net": caducado en el tracker
    CHECK(proto::applyEvent(m, R"J({"e":"gnss","fix":true,"lat":37.191512,"lon":-3.936701,"alt":512.0,"spd":0.0,"crs":0.0,"sats":11,"hdop":0.8,"utc":"2026-11-05T07:45:00Z"})J"));
    CHECK(m.gnss.fix && m.gnss.sats == 11 && m.gnss.lon < -3.9 && m.gnss.utc == "2026-11-05T07:45:00Z");
    CHECK(proto::applyEvent(m, R"J({"e":"heard","tot":2,"pkts":9,"items":[{"c":"EA7ABC-9","sym":"/>","lat":37.1,"lon":-3.6,"rssi":-97,"snr":6.5,"age":42,"n":3,"cmt":"hola"},{"c":"EB7XYZ","sym":"/-","rssi":-110,"age":900,"n":1}]})J"));
    CHECK(m.heard.size() == 2 && m.heardTot == 2 && m.heard[0].hasPos && !m.heard[1].hasPos && m.heard[0].call == "EA7ABC-9" && m.heard[0].sym[0] == '/' && m.heard[0].cmt == "hola");
    CHECK(proto::applyEvent(m, R"J({"e":"msgs","tot":3,"unread":1,"from":0,"items":[{"i":0,"d":"R","ep":1790000000,"st":0,"c":"EA7ABC-9","t":"Hola"},{"i":1,"d":"T","ep":1789999000,"st":4,"c":"EA7ABC-9","t":"QSL"}]})J"));
    CHECK(m.msgs.size() == 2 && m.msgUnread == 1 && m.msgs[0].dir == 'R' && m.msgs[1].dir == 'T' && m.msgs[1].st == 4 && m.msgs[0].text == "Hola");
    CHECK(proto::applyEvent(m, R"J({"e":"sat","ok":true,"n":"ISS (ZARYA)","id":25544,"az":123.4,"el":45.0,"rng":850,"aos":1790000000,"los":1790000600,"max":62})J"));
    CHECK(m.sat.ok && m.sat.name == "ISS (ZARYA)" && m.sat.maxEl == 62 && m.sat.los == 1790000600u);
    CHECK(proto::applyEvent(m, R"J({"e":"rot","az":123.4,"el":45.0,"trk":true,"src":4,"name":"EA7ABC-9","km":12})J"));
    CHECK(m.rot.trk && m.rot.src == 4 && m.rot.name == "EA7ABC-9");
    CHECK(proto::applyEvent(m, R"J({"e":"screen","p":1,"cur":0,"t":"STATIONS","r":"12:00","rows":["EA7ABC 3km","EB7XYZ 9km"]})J"));
    CHECK(m.screen.rows.size() == 2 && m.screen.title == "STATIONS");
    CHECK(proto::applyEvent(m, R"J({"e":"page","ric":12345,"own":true,"rssi":-97,"t":"Aviso"})J"));
    CHECK(m.pages.size() == 1 && m.pages[0].own && m.pages[0].text == "Aviso");
    CHECK(proto::applyEvent(m, R"J({"e":"err","c":"msg","msg":"solo en modos APRS"})J"));
    CHECK(m.lastError == "msg: solo en modos APRS");
    CHECK(proto::applyEvent(m, R"J({"e":"ok","c":"msg","ack":4})J"));
    CHECK(!proto::applyEvent(m, R"J({"e":"algo_nuevo"})J"));
    CHECK(!proto::applyEvent(m, "no es json"));
    CHECK(!proto::applyEvent(m, R"J({"x":1})J"));
    // Ordenes
    CHECK(proto::cmdSimple("info") == "{\"c\":\"info\"}\n");
    CHECK(proto::cmdSub(5, 2) == "{\"c\":\"sub\",\"sensors\":5,\"gnss\":2}\n");
    CHECK(proto::cmdMsgSend("EA7ABC-9", "Hola \"mundo\"", true) == "{\"c\":\"msg\",\"to\":\"EA7ABC-9\",\"t\":\"Hola \\\"mundo\\\"\",\"ack\":true}\n");
    std::string larga(100, 'x');
    { std::string c = proto::cmdMsgSend("A", larga, false); CHECK(c.find(std::string(67, 'x')) != std::string::npos && c.find(std::string(68, 'x')) == std::string::npos); }
    CHECK(proto::cmdMode(9) == "{\"c\":\"mode\",\"m\":9}\n");
    // ---- lo que usa la interfaz tipo emisora (7B v2) ----
    {
        proto::Model n;
        n.now = 5000;
        CHECK(proto::applyEvent(n, R"J({"e":"info","v":1,"fw":"5.4.1","board":"lilygo_t_twr_plus","caps":{"radio":"sa868","m17":true,"fm":true,"bcast":"RDA5807M","satFm":true,"ais":true,"wxnet":true},"mode":11,"modes":[{"m":0,"n":"APRS AFSK 1200"},{"m":11,"n":"FM"}],"profile":1,"profiles":[{"call":"EA7JRS-7","label":"Coche"},{"call":"EA7JRS-9","label":"Moto"}],"lora":0,"loras":[{"f":433775000,"sf":12,"cr":5,"fsk":false}],"cwMsgs":["CQ DE {CALL}"],"sa868":{"rev":21,"fw":"NiceRF","band":"VHF","ok":true,"modem":"AFSK 1200","rx":144800000,"tx":144800000,"txul":false,"txulav":true,"mac":"AA:BB"}})J"));
        CHECK(n.info.radio == "sa868" && n.info.capM17 && n.info.capFm && n.info.bcast == "RDA5807M" && n.info.capAis && n.info.capWxnet);
        CHECK(n.info.profiles.size() == 2 && n.info.profiles[1].label == "Moto" && n.info.loras[0].f == 433775000u && n.info.loras[0].sf == 12);
        CHECK(n.info.cwMsgs.size() == 1 && n.info.sa868.present && n.info.sa868.rev == 21 && n.info.sa868.txulav && n.info.sa868.rx == 144800000u);
        CHECK(proto::applyEvent(n, R"J({"e":"status","mode":2,"f":432050000,"mod":"CW"})J"));
        CHECK(n.status.f == 432050000u && n.status.mod == "CW");
        CHECK(proto::applyEvent(n, R"J({"e":"heard","tot":14,"pkts":50,"from":0,"items":[{"c":"A1","sym":">/","k":"MOBILE","lat":37.0,"lon":-6.0,"spd":50,"crs":90,"alt":100,"rssi":-90,"snr":5.0,"age":10,"pa":12,"n":2,"dev":"LoRa Tracker","path":"WIDE1-1","sts":"En route","wx":{"t":21,"h":50,"p":1015,"wd":180,"ws":10,"wg":20,"r":3}}]})J"));
        CHECK(n.heard.size() == 1 && n.heard[0].kind == "MOBILE" && n.heard[0].pa == 12 && n.heard[0].wx.ok && n.heard[0].wx.wg > 19 && n.heard[0].symCode() == '>' && n.heard[0].symTable() == '/');
        CHECK(n.heard[0].dev == "LoRa Tracker" && n.heard[0].sts == "En route" && n.heardAtMs == 5000 && n.heardPkts == 50);
        CHECK(proto::applyEvent(n, R"J({"e":"heard","tot":14,"from":12,"items":[{"c":"B12"},{"c":"B13"}]})J"));
        CHECK(n.heard.size() == 14 && n.heard[12].call == "B12" && n.heard[13].call == "B13" && n.heard[0].call == "A1");
        CHECK(proto::applyEvent(n, R"J({"e":"heard","tot":3,"from":0,"items":[{"c":"Z0"}]})J"));
        CHECK(n.heard.size() == 3 && n.heard[0].call == "Z0");
        CHECK(proto::applyEvent(n, R"J({"e":"msgs","tot":25,"unread":0,"from":20,"items":[{"i":20,"d":"T","st":5,"c":"X","t":"t20"}]})J"));
        CHECK(n.msgs.size() == 21 && n.msgs[20].st == 5);
        CHECK(proto::applyEvent(n, R"J({"e":"msg","tot":26,"unread":1,"i":0,"d":"R","c":"Y","t":"nuevo"})J") && n.msgSeq == 1 && n.msgUnread == 1);
        CHECK(proto::applyEvent(n, R"J({"e":"sky","pd":2.0,"hd":1.0,"vd":1.6,"used":8,"s":[["G",5,45,310,38,1],["E",16,55,330,42,0]]})J"));
        CHECK(n.sky.ok && n.sky.sats.size() == 2 && n.sky.sats[0].sys == 'G' && n.sky.sats[0].used && !n.sky.sats[1].used && n.sky.sats[1].snr == 42 && n.sky.used == 8);
        n.now = 7000;
        CHECK(proto::applyEvent(n, R"J({"e":"rf","s":"tx"})J") && n.rf.txSinceMs == 7000);
        CHECK(proto::applyEvent(n, R"J({"e":"rf","s":"idle"})J") && n.rf.txSinceMs == 0);
        CHECK(proto::applyEvent(n, R"J({"e":"rf","s":"rx","rssi":-97,"snr":7.5})J") && n.rf.rxAtMs == 7000 && n.rf.rssi == -97);
        CHECK(proto::applyEvent(n, R"J({"e":"led","msg":true,"unread":2,"emg":true,"ec":"EA7ZZZ-9","et":"Mic-E: Emergency!","ek":"emergency","em":false,"eq":7,"eo":true,"fl":true})J"));
        CHECK(n.leds.msg && n.leds.emg && n.leds.eq == 7 && n.leds.ec == "EA7ZZZ-9" && n.leds.flash && n.leds.eo);
        CHECK(proto::applyEvent(n, R"J({"e":"eai","sos":2,"sent":3,"left":581,"by":"EA7URM-9","fm":true,"fmLeft":590,"fmBy":"mandown","fmMode":1,"md":false})J"));
        CHECK(n.eai.sos == 2 && n.eai.left == 581 && n.eai.fmBy == "mandown");
        CHECK(proto::applyEvent(n, R"J({"e":"cwrx","on":true,"t":"C","dbm":-104,"snr":15,"wpm":18})J"));
        CHECK(proto::applyEvent(n, R"J({"e":"cwrx","on":true,"t":"Q"})J"));
        CHECK(proto::applyEvent(n, R"J({"e":"cwrx","on":true,"f":7023000})J"));
        CHECK(n.cwrx.on && n.cwrx.text == "CQ" && n.cwrx.wpm == 18 && n.cwrx.f == 7023000u);
        CHECK(proto::applyEvent(n, R"J({"e":"fm","f":145500000,"tx":145500000,"sh":0,"tm":2,"t":885,"w":false,"hp":true,"n":3,"m":true,"busy":true,"rssi":-80,"sq":4,"sat":{"n":"ISS","f":145825000,"up":145825000,"dop":1100,"aprs":true,"txok":false,"why":"SAT TX off"}})J"));
        CHECK(n.fm.ok && n.fm.f == 145500000u && n.fm.tm == 2 && n.fm.t == 885 && !n.fm.w && n.fm.hp && n.fm.busy && n.fm.sat.ok && n.fm.sat.aprs && n.fm.sat.why == "SAT TX off");
        CHECK(proto::applyEvent(n, R"J({"e":"fm","f":145600000})J") && !n.fm.busy && !n.fm.sat.ok && n.fm.tm == 2);
        CHECK(proto::applyEvent(n, R"J({"e":"fmmems","max":32,"mem":[{"i":0,"n":"REP","f":145600000,"sh":-1,"ofs":600000,"tm":1,"t":885}]})J"));
        CHECK(n.fmMems.size() == 1 && n.fmMems[0].n == "REP" && n.fmMems[0].sh == -1 && n.fmMems[0].ofs == 600000);
        CHECK(proto::applyEvent(n, R"J({"e":"bcast","chip":"RDA5807M","on":true,"band":0,"f":101300,"vol":7,"rssi":40,"st":true,"ps":"RNE","rt":"Noticias","bands":[0],"presets":[101300,0,0]})J"));
        CHECK(n.bcast.ok && n.bcast.f == 101300u && n.bcast.ps == "RNE" && n.bcast.presets.size() == 3 && n.bcast.bands.size() == 1);
        CHECK(proto::applyEvent(n, R"J({"e":"aislist","tot":2,"from":0,"msgs":120,"src":"SA868 A","ch":"A","items":[{"m":224123450,"cl":"A","n":"BARCO","lat":36.5,"lon":-6.3,"sog":10.2,"cog":185.0,"nm":2.3,"brg":210,"cpa":0.4,"tcpa":8.5,"dng":true}]})J"));
        CHECK(n.ais.size() == 1 && n.ais[0].danger && n.ais[0].mmsi == 224123450u && n.aisSrc == "SA868 A");
        CHECK(proto::applyEvent(n, R"J({"e":"ais","m":224123450,"n":"BARCO","ds":"CADIZ","len":30,"nm":2.0})J") && n.ais.size() == 1 && n.ais[0].dest == "CADIZ");
        CHECK(proto::applyEvent(n, R"J({"e":"ais","m":1,"n":"OTRO"})J") && n.ais.size() == 2);
        CHECK(proto::applyEvent(n, R"J({"e":"actlist","p":2,"pt":"MORSE","items":[{"n":"CW0","l":"Send CW 1"},{"n":"LST","l":"Listen","ok":false}]})J"));
        CHECK(proto::applyEvent(n, R"J({"e":"actlist","end":true,"pages":1})J"));
        CHECK(n.actions.size() == 1 && n.actions[0].title == "MORSE" && !n.actions[0].items[1].ok && n.actionsDone);
        CHECK(proto::applyEvent(n, R"J({"e":"cfg","o":0,"n":10,"b":6,"d":"{\"a\":1"})J") && !n.cfgDone);
        CHECK(proto::applyEvent(n, R"J({"e":"cfg","o":0,"n":10,"b":6,"d":"{\"a\":1"})J") && n.cfgText == "{\"a\":1");
        CHECK(proto::applyEvent(n, R"J({"e":"cfg","o":6,"n":10,"b":4,"d":",\"b\"}"})J") && n.cfgDone && n.cfgText == "{\"a\":1,\"b\"}");
        CHECK(proto::applyEvent(n, R"J({"e":"targets","sel":0,"src":2,"ff":"EA7","fm":30,"pr":true,"pm":300,"items":[{"i":0,"n":"Rep","lat":37.1,"lon":-5.9,"alt":850,"f":145600000}]})J"));
        CHECK(n.targets.ok && n.targets.items.size() == 1 && n.targets.pr && n.targets.ff == "EA7");
        CHECK(proto::applyEvent(n, R"J({"e":"satlist","tot":3,"sel":1,"from":0,"items":[{"i":0,"id":25544,"n":"ISS","c":"U","t":4,"b":3,"fav":true}]})J") && n.satList.size() == 1 && n.satList[0].fav && n.satListSel == 1);
        CHECK(proto::applyEvent(n, R"J({"e":"sattxlist","items":[{"i":0,"m":"FM","d":"Voice","up":145990000,"dn":437800000,"rx":"FM","tx":"FM","tone":67.0,"aprs":false}]})J") && n.satTxs.size() == 1 && n.satTxs[0].tone > 66);
        CHECK(proto::applyEvent(n, R"J({"e":"satrx","on":true,"k":"FSK","f":437425310,"dop":-3120,"n":7,"txt":"hola"})J") && n.satRx.on && n.satRx.lines.size() == 1);
        CHECK(proto::applyEvent(n, R"J({"e":"satrx","on":false})J") && !n.satRx.on && n.satRx.lines.size() == 1);
        CHECK(proto::applyEvent(n, R"J({"e":"err","c":"msg","msg":"not in APRS mode"})J") && n.errSeq == 1 && n.lastErrorCmd == "msg");
        CHECK(proto::applyEvent(n, R"J({"e":"ok","c":"beacon"})J") && n.okSeq == 1 && n.lastOk == "beacon");
        CHECK(proto::applyEvent(n, R"J({"e":"netreq","what":"wx"})J") && n.netReq == "wx");
        CHECK(proto::applyEvent(n, R"J({"e":"ota","off":8192})J") && n.otaOff == 8192 && !n.otaDone);
        CHECK(proto::modeName(n, 11) == "FM" && proto::modeName(n, 9) == "SAT");
    }
    // Cmd, troceo y utilidades
    CHECK(proto::Cmd("fmset").num("f", (uint32_t)145500000).flag("w", true).str() == "{\"c\":\"fmset\",\"f\":145500000,\"w\":true}\n");
    CHECK(proto::Cmd("qsy").text("c", "146.940MHz T100").str() == "{\"c\":\"qsy\",\"c\":\"146.940MHz T100\"}\n" ||
          proto::Cmd("qsy").text("c", "146.940MHz T100").str().find("146.940") != std::string::npos);
    {
        std::string cfg = "{\"x\":\"";
        for (int i = 0; i < 300; i++) cfg += "\xC3\xB1";          // ñ: 2 bytes
        cfg += "\"}";
        auto v = proto::chunked("cfgset", cfg);
        CHECK(v.size() >= 5 && v.front().find("\"s\":\"begin\"") != std::string::npos && v.back().find("\"s\":\"end\"") != std::string::npos);
        std::string joined;
        bool shortOk = true;
        for (size_t i = 1; i + 1 < v.size(); i++) {
            shortOk = shortOk && v[i].size() <= 255;
            proto::Model t;
            // la linea es una orden; sacar el texto con ArduinoJson por el mismo camino: es JSON valido
            size_t a = v[i].find("\"d\":\"") + 5, b = v[i].rfind("\"}");
            std::string piece = v[i].substr(a, b - a);
            for (size_t k = 0; k < piece.size(); k++) if (piece[k] == '\\') { piece.erase(k, 1); }
            joined += piece;
        }
        CHECK(shortOk && joined == cfg);
    }
    CHECK(proto::sMeter(-73, 7000000).label == "S9" && proto::sMeter(-93, 433775000).label == "S9");
    CHECK(proto::sMeter(-63, 7000000).label == "S9+10" && proto::sMeter(-63, 7000000).segments == 10);
    CHECK(proto::sMeter(-105, 433775000).s == 7 && proto::sMeter(-200, 433775000).s == 0);
    CHECK(proto::mhz(433775000) == "433.775" && proto::subKhz(433775250) == "250");
    CHECK(proto::locator(37.3891, -5.9845) == "IM77aj");
    CHECK(proto::locator(48.1467, 11.6083) == "JN58td");
    CHECK(proto::distanceKm(40, -3, 41, -3) > 110 && proto::distanceKm(40, -3, 41, -3) < 112);
    { double b = proto::bearingDeg(40, -3, 40, -2); CHECK(b > 89 && b < 91); }
    CHECK(!strcmp(proto::cardinal16(0), "N") && !strcmp(proto::cardinal16(350), "N") && !strcmp(proto::cardinal16(225), "SW"));
    CHECK(!strcmp(proto::msgStateName(4), "ACK"));
    // Troceo de lineas
    proto::LineBuffer lb; std::vector<std::string> out;
    auto f = [&](const std::string& s) { out.push_back(s); };
    const char* a = "{\"e\":\"ok\"}\n{\"e\":\"st"; const char* b = "atus\"}\r\n\n{\"e\":";
    lb.feed((const uint8_t*)a, strlen(a), f); lb.feed((const uint8_t*)b, strlen(b), f);
    CHECK(out.size() == 2 && out[0] == "{\"e\":\"ok\"}" && out[1] == "{\"e\":\"status\"}");
    printf("%d comprobaciones, %d fallos\n", total, fails);
    return fails ? 1 : 0;
}
