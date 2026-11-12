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
    // Troceo de lineas
    proto::LineBuffer lb; std::vector<std::string> out;
    auto f = [&](const std::string& s) { out.push_back(s); };
    const char* a = "{\"e\":\"ok\"}\n{\"e\":\"st"; const char* b = "atus\"}\r\n\n{\"e\":";
    lb.feed((const uint8_t*)a, strlen(a), f); lb.feed((const uint8_t*)b, strlen(b), f);
    CHECK(out.size() == 2 && out[0] == "{\"e\":\"ok\"}" && out[1] == "{\"e\":\"status\"}");
    // Avisos oficiales del tracker
    CHECK(proto::applyEvent(m, R"J({"e":"alertsv","v":7,"n":2,"max":2})J") && m.alertsVerTracker == 7 && m.alertsVer != 7);
    CHECK(proto::applyEvent(m, R"J({"e":"alerts","tot":2,"from":0,"v":7,"items":[{"id":"x1","s":"aemet","lv":2,"ev":"Coastal events","ar":"Litoral de Huelva","tx":"olas 4 m","on":1794500000,"ex":1794550000},{"id":"usgs/a","s":"usgs","lv":1,"ev":"M5.4 Earthquake","ar":"S Cabo San Vicente","tx":"Depth 12 km","on":1,"ex":2,"la":36.1,"lo":-9.2}]})J"));
    CHECK(m.alerts.size() == 2 && m.alertsVer == 7 && !m.alertsPending && m.alerts[0].src == plotter::AlertSrc::Aemet && m.alerts[0].level == 2 && m.alerts[1].hasPos());
    CHECK(proto::applyEvent(m, R"J({"e":"spacewx","ok":true,"g":3,"r":1,"s":0,"gm":2,"kp":[2.33,5,7],"kt":1794500000,"sfi":182,"bz":-12,"xr":"M2.4","age":30})J"));
    CHECK(m.spaceOk && m.space.g == 3 && m.space.kpCount == 3 && m.space.kpNow() == 7 && m.space.xray == "M2.4" && m.space.sfi == 182);
    CHECK(proto::applyEvent(m, R"J({"e":"alertsv","v":8,"n":0,"max":-1})J") && m.alerts.empty() && m.alertsVer == 8);
    CHECK(proto::cmdAlerts(0, 12) == "{\"c\":\"alerts\",\"from\":0,\"cnt\":12}\n");
    printf("%d comprobaciones, %d fallos\n", total, fails);
    return fails ? 1 : 0;
}
