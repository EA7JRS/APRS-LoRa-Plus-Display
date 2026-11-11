/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Prueba en el PC del protocolo de la caja del rotor y del DX Cluster:
// g++ -std=c++17 -I src -I <ArduinoJson>/src test/test_extra.cpp src/rotor_proto.cpp src/dxcluster.cpp -o /tmp/te && /tmp/te
#include "rotor_proto.h"
#include "dxcluster.h"
#include "wxcalc.h"
#include "astro.h"
#include <cmath>
#include <cstdio>
#include <cstring>
static int fails = 0, total = 0;
#define NEAR(a, b, tol) CHECK(std::fabs((double)(a) - (double)(b)) <= (tol))
#define CHECK(c) do { total++; if (!(c)) { fails++; printf("FALLA %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
int main() {
    rotp::Model m;
    CHECK(rotp::apply(m, R"J({"e":"info","api":1,"lim":{"minAz":0,"maxAz":450,"minEl":0,"maxEl":180},"hasEl":true,"pin":true,"dev":"APRS LoRa+ Rotor","ver":"5.4.1","board":"esp32s3","name":"APRS-Rotor","drv":"direct (relays)","call":"EA7JRS-12","caps":{"cal":true,"move":true},"cat":true,"rigctld":4532,"wind":true})J"));
    CHECK(m.info.ok && m.info.hasEl && m.info.maxAz == 450 && m.info.cat && m.info.rigctld == 4532 && m.info.capCal && m.info.wind && m.info.call == "EA7JRS-12");
    CHECK(rotp::apply(m, R"J({"e":"pos","az":123.4,"el":45.0,"st":38,"mv":true,"f":"OK","maz":483.4,"taz":125.0,"tel":45.0,"wind":{"on":true,"park":15,"lk":true,"ws":20,"wg":28.4,"wd":250,"pk":28.4,"age":150,"src":"OWM","hold":870}})J"));
    CHECK(m.pos.ok && m.pos.moving && m.pos.hasEl && m.pos.maz > 483 && m.pos.hasTarget && m.pos.wind.locked && m.pos.wind.hold == 870 && m.pos.wind.src == "OWM");
    CHECK(rotp::apply(m, R"J({"e":"pos","az":10,"f":"STALL AZ"})J") && !m.pos.hasEl && !m.pos.hasTarget && m.pos.fault == "STALL AZ" && !m.pos.wind.ok);
    CHECK(rotp::apply(m, R"J({"e":"cat","rig":"IC-9700","l":true,"f":436795000,"m":"FM","tf":145850000,"sp":true,"v":"A","ptt":false,"sm":11,"tn":67.0,"pa":true,"tr":{"en":true,"on":true,"n":"SO-50","dop":-4210}})J"));
    CHECK(m.cat.link && m.cat.f == 436795000u && m.cat.split && m.cat.sm == 11 && m.cat.pttAllowed && m.cat.trackOn && m.cat.dop == -4210 && m.cat.tone > 66);
    CHECK(rotp::sLabel(11) == "S9+20" && rotp::sLabel(7) == "S7" && rotp::sLabel(-1) == "S-");
    CHECK(rotp::apply(m, R"J({"e":"raw","az":2105,"el":1460,"unit":"mV"})J") && m.raw.az == 2105 && m.raw.unit == "mV");
    CHECK(rotp::apply(m, R"J({"e":"err","c":"auth","m":"bad pin"})J") && m.authFailed && !m.authOk);
    CHECK(rotp::apply(m, R"J({"e":"ok","c":"auth"})J") && m.authOk && !m.authFailed);
    CHECK(rotp::apply(m, R"J({"e":"err","c":"pos","m":"auth required"})J") && !m.authOk && m.lastError == "pos: auth required");
    CHECK(rotp::apply(m, R"J({"e":"cfg","i":0,"n":2,"d":"{\"a\":"})J") && !m.cfgDone);
    CHECK(rotp::apply(m, R"J({"e":"cfg","i":1,"n":2,"d":"1}"})J") && m.cfgDone && m.cfgText == "{\"a\":1}");
    CHECK(rotp::goTo(123.44f, 45, true) == "{\"c\":\"pos\",\"az\":123.4,\"el\":45}\n");
    CHECK(rotp::goTo(90, 0, false) == "{\"c\":\"pos\",\"az\":90}\n");
    CHECK(rotp::catTrack(false, "", 0, 0, 0) == "{\"c\":\"cattrack\",\"on\":false}\n");
    CHECK(rotp::catTrack(true, "SO-50", 436795000, 145850000, -1.5).find("\"rr\":-1.5") != std::string::npos);
    CHECK(rotp::wx(10, -1, 250, 60, "OWM") == "{\"c\":\"wx\",\"ws\":10,\"wd\":250,\"age\":60,\"src\":\"OWM\"}\n");
    { std::string big(400, 'x'); auto v = rotp::cfgSet(big); CHECK(v.size() == 3 && v[0].find("\"i\":0,\"n\":3") != std::string::npos); }
    CHECK(rotp::satMode("FM voice") == "FM" && rotp::satMode("USB linear") == "USB" && rotp::satMode("CW beacon") == "CW" && rotp::satMode("GMSK 9k6") == "PKTUSB" && rotp::satMode("AFSK APRS") == "FM");

    dx::Spot s;
    CHECK(dx::parse("DX de EA7JRS-#:   7023.0  DL1ABC       CW 18 dB 24 WPM CQ                    1234Z", s));
    CHECK(s.spotter == "EA7JRS-#" && s.skimmer && s.call == "DL1ABC" && s.khz > 7022.9 && s.time == "1234Z" && s.band == "40m" && s.mode == "CW");
    CHECK(dx::parse("DX de EA4URE:    14205.0  K1ABC        SSB POTA K-1234 59 in Madrid  1300Z\x07\x07", s));
    CHECK(!s.skimmer && s.mode == "PHO" && s.activity == "POTA" && s.time == "1300Z" && s.comment.find("Madrid") != std::string::npos);
    CHECK(dx::parse("DX de W3LPL:     14074.0  JA1XYZ       FT8 -12 dB                   0102Z", s) && s.mode == "DIG");
    CHECK(dx::parse("DX de EA1AB:     21074.5  VK2ABC                                     0102Z", s) && s.mode == "DIG");
    CHECK(dx::parse("DX de EA1AB:     14250.0  VK2ABC                                     0102Z", s) && s.mode == "PHO");
    CHECK(dx::parse("  7023.0  DL1ABC      10-Nov-2026 1234Z  CW 18 dB                  <EA7JRS-#>", s));
    CHECK(s.call == "DL1ABC" && s.spotter == "EA7JRS-#" && s.time == "1234Z" && s.comment == "CW 18 dB");
    CHECK(!dx::parse("login: ", s) && !dx::parse("Hello EA7JRS, this is EA4URE", s));
    CHECK(dx::band(144300) == "2m" && dx::band(5000) == "");
    dx::Filter f;
    dx::parse("DX de EA7JRS-#:   7023.0  DL1ABC       CW 18 dB 24 WPM CQ                    1234Z", s);
    CHECK(dx::pass(f, s)); f.rbnCw = false; CHECK(!dx::pass(f, s)); f.rbnCw = true; f.band = "20m"; CHECK(!dx::pass(f, s));

    // calculos meteorologicos
    CHECK(wxc::beaufort(0) == 0 && wxc::beaufort(3) == 1 && wxc::beaufort(17) == 5 && wxc::beaufort(34) == 8 && wxc::beaufort(70) == 12);
    CHECK(!strcmp(wxc::beaufortName(8), "Gale") && !strcmp(wxc::beaufortName(0), "Calm"));
    NEAR(wxc::dewPointC(20, 50), 9.3, 0.15);                    // tablas psicrometricas: 9,3 C
    NEAR(wxc::dewPointC(30, 100), 30, 0.01);
    CHECK(!wxc::has(wxc::dewPointC(20, wxc::NA)) && !wxc::has(wxc::dewPointC(wxc::NA, 50)));
    NEAR(wxc::windChillC(-10, 30), -19.5, 0.1);                 // tabla de Environment Canada: -20
    NEAR(wxc::windChillC(5, 20), 1.1, 0.2);
    CHECK(!wxc::has(wxc::windChillC(15, 30)) && !wxc::has(wxc::windChillC(0, 3)));
    NEAR(wxc::heatIndexC(32.2222f, 70), 41.1, 0.3);             // tabla del NWS: 90 F y 70 % = 105-106 F
    CHECK(!wxc::has(wxc::heatIndexC(20, 80)));
    { int k = -1; NEAR(wxc::feelsLikeC(-10, 50, 30, &k), -19.5, 0.1); CHECK(k == 1); wxc::feelsLikeC(32.2222f, 70, 5, &k); CHECK(k == 2); CHECK(!wxc::has(wxc::feelsLikeC(18, 50, 10, &k)) && k == 0); }
    NEAR(wxc::cloudBaseM(20, 12), 1000, 0.5);
    NEAR(wxc::seaLevelHpa(1000, 100, 15), 1011.9, 0.3);
    NEAR(wxc::seaLevelHpa(1013.25f, 0, 15), 1013.25, 0.01);
    {
        wxc::PressureTrend tr; float d = 0;
        CHECK(!tr.delta3h(0, d));
        for (int i = 0; i <= 36; i++) tr.add(i * 300, 1015.0f - i * 0.05f);        // 3 h bajando 1,8 hPa
        CHECK(tr.delta3h(36 * 300, d)); NEAR(d, -1.8, 0.05);
        CHECK(!strcmp(wxc::PressureTrend::name(d), "falling") && !strcmp(wxc::PressureTrend::name(0.4f), "steady") && !strcmp(wxc::PressureTrend::name(4), "rising fast"));
        CHECK(!tr.delta3h(36 * 300 + 4000, d));                                     // la ultima lectura ya es vieja
        wxc::PressureTrend t2; t2.add(0, 1010); t2.add(1800, 1011); CHECK(!t2.delta3h(1800, d));   // menos de 1 h
        t2.add(3600, 1012); CHECK(t2.delta3h(3600, d)); NEAR(d, 6, 0.01);           // 2 hPa en 1 h = 6 en 3 h
    }
    NEAR(wxc::distUnit(1).k * 1.852, 1, 1e-9); CHECK(!strcmp(wxc::speedUnit(2).label, "mph")); NEAR(wxc::tempShown(100, 2), 212, 1e-4);
    // sol y luna (las mismas pruebas del nucleo del plotter)
    {
        using namespace plotter;
        double r, s2;
        CHECK(sunTimes(2026, 3, 20, 0, 0, -0.833, r, s2)); NEAR(s2 - r, 12 * 60 + 7, 3);
        CHECK(sunTimes(2026, 11, 11, 36.53, -6.28, -0.833, r, s2)); NEAR(s2 - r, 10 * 60 + 21, 4);
        CHECK(!sunTimes(2026, 12, 21, 80, 0, -0.833, r, s2));
        double alt, az, noon = solarNoonUtcMin(2026, 11, 11, -6.28) / 60;
        sunPosition(2026, 11, 11, noon, 36.53, -6.28, alt, az); NEAR(az, 180, 0.2); NEAR(alt, 90 - 36.53 - 17.3, 0.3);
        double age = moonAgeDays(2026, 11, 9, 7); CHECK(age < 0.7 || age > 29.530588853 - 0.7);
        NEAR(moonIllumination(29.530588853 / 2), 1, 1e-12);
    }
    printf("%d comprobaciones, %d fallos\n", total, fails);
    return fails ? 1 : 0;
}
