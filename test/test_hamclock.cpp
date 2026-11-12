/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Pruebas del modelo del reloj en el PC:
 *   g++ -std=c++17 -I src/hamclock -I <ArduinoJson>/src test/test_hamclock.cpp src/hamclock/hc_model.cpp -o /tmp/t && /tmp/t
 * Con argumentos calcula pases (lo usa tools/hamclock_sim/check_passes.py para compararlos con SGP4):
 *   /tmp/t pass <epoch ISO> <rev/dia> <ecc> <inc> <raan> <argp> <ma> <unix> <lat> <lon>
 */
#include "hc_model.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALLA linea %d: %s\n", __LINE__, #c); } } while (0)
#define NEAR(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) <= (tol))

template <typename F, typename A> static bool parse(hc::Model& m, const char* json, F filter, A apply) {
    JsonDocument f, doc;
    filter(f);
    if (deserializeJson(doc, json, DeserializationOption::Filter(f))) return false;
    return apply(m, doc.as<JsonVariantConst>());
}

int main(int argc, char** argv) {
    using namespace hc;
    if (argc == 12 && !strcmp(argv[1], "pass")) {
        SatElements s{};
        const double D = 3.14159265358979323846 / 180;
        strcpy(s.name, "SAT");
        parseIsoUtc(argv[2], s.epochJd);
        s.n = atof(argv[3]) * 2 * 3.14159265358979323846 / 1440;
        s.ecc = atof(argv[4]); s.inc = atof(argv[5]) * D; s.raan = atof(argv[6]) * D; s.argp = atof(argv[7]) * D; s.ma = atof(argv[8]) * D;
        Pass p[6];
        int n = findPasses(&s, 1, (time_t)atoll(argv[9]), atof(argv[10]), atof(argv[11]), p, 6, 10, 24);
        for (int i = 0; i < n; i++) printf("%lld %.1f\n", (long long)p[i].aos, p[i].maxEl);
        return 0;
    }
    Model m;

    // bandas y locator
    CHECK(bandOf(14.074) == B20); CHECK(bandOf(7.155) == B40); CHECK(bandOf(28.495) == B10); CHECK(bandOf(13.0) == B_OTHER);
    CHECK(!strcmp(bandName(B17), "17m"));
    double la, lo;
    CHECK(gridToLatLon("IM67", la, lo)); NEAR(la, 37.5, 1e-9); NEAR(lo, -7.0, 1e-9);
    CHECK(gridToLatLon("im67xj", la, lo)); NEAR(la, 37.396, 0.01); NEAR(lo, -6.04, 0.01);
    CHECK(!gridToLatLon("ZZ99", la, lo)); CHECK(!gridToLatLon("IM6", la, lo));
    char g[5]; latLonToGrid(35.7, 139.7, g); CHECK(!strcmp(g, "PM95"));
    // Sevilla - Tokio: unos 11 100 km, rumbo 28 grados
    NEAR(distanceKm(37.39, -5.98, 35.7, 139.7), 11130, 120); NEAR(bearingDeg(37.39, -5.98, 35.7, 139.7), 27.7, 0.5);
    greatCircle(0, 0, 0, 90, 0.5, la, lo); NEAR(la, 0, 1e-6); NEAR(lo, 45, 1e-6);

    // sol: equinoccio de marzo de 2026 (20 de marzo, 14:46 UTC) y solsticio de junio
    double dec, slon;
    sunPosition(1774017960, dec, slon); NEAR(dec, 0, 0.1);
    sunPosition(1782000000 + 43200 * 0, dec, slon); NEAR(dec, 23.4, 0.15);            // 21 de junio de 2026
    sunPosition(1767268800, dec, slon); NEAR(slon, 0.8, 0.5); NEAR(dec, -23.0, 0.15); // 1 de enero de 2026, 12:00 UTC
    // orto y ocaso en Sevilla el 11 de noviembre de 2026 (1794355200 = 00:00 UTC): 06:59 y 17:17 UTC, mas o menos
    time_t r, s;
    CHECK(sunRiseSet(1794355200 + 3600, 37.39, -5.98, r, s));
    NEAR((r - 1794355200) / 60.0, 6 * 60 + 59, 4); NEAR((s - 1794355200) / 60.0, 17 * 60 + 17, 4);
    CHECK(!sunRiseSet(1782000000, 80, 0, r, s));                                     // sol de medianoche

    // /api/solar-indices
    CHECK(parse(m, R"({"sfi":{"current":148,"history":[{"date":"x","value":140}]},"kp":{"current":2.33,"history":[
        {"time":"a","value":1},{"time":"b","value":2.67},{"time":"c","value":4}],"forecast":[]},"ssn":{"current":112},
        "bz":{"current":-3.2},"timestamp":"2026-11-11T02:00:00Z"})", filterSolar, applySolar));
    NEAR(m.sfi, 148, 0.01); NEAR(m.kp, 2.33, 0.01); CHECK(m.ssn == 112); NEAR(m.bz, -3.2, 0.01);
    CHECK(m.kpN == 3); NEAR(m.kpHist[1], 2.67, 0.01);
    CHECK(!parse(m, R"({"error":"Failed to fetch solar indices"})", filterSolar, applySolar));

    // /api/n0nbh
    CHECK(parse(m, R"({"source":"N0NBH","solarData":{"solarFlux":"148","aIndex":"8","kIndex":"2","xray":"C1.4","solarWind":"412.3"},
        "bandConditions":[{"name":"80m-40m","time":"day","condition":"Fair"},{"name":"30m-20m","time":"day","condition":"Good"},
        {"name":"80m-40m","time":"night","condition":"Good"},{"name":"30m-20m","time":"night","condition":"Poor"}]})",
        filterN0nbh, applyN0nbh));
    CHECK(m.aIndex == 8); CHECK(m.wind == 412); CHECK(!strcmp(m.xray, "C1.4")); CHECK(m.nBands == 2);
    CHECK(!strcmp(m.bands[0].day, "Fair")); CHECK(!strcmp(m.bands[0].night, "Good")); CHECK(!strcmp(m.bands[1].night, "Poor"));

    // /api/dxcluster/paths: orden por timestamp, modo por comentario o por segmento, posicion nula
    CHECK(parse(m, R"([{"spotter":"EA5ABC","spotterLat":39,"dxCall":"JA1XYZ","dxLat":35.7,"dxLon":139.7,"dxCountry":"Japan",
        "freq":"14.074","comment":"FT8 -12dB","time":"0214z","timestamp":2000},
        {"spotter":"DL1AA","dxCall":"VK3QQ","dxLat":null,"dxLon":null,"freq":"7.155","comment":"","time":"02:10","timestamp":1000},
        {"spotter":"W1CW","dxCall":"ZS6ABC","dxLat":-26.2,"dxLon":28,"freq":"21.025","comment":"tnx W1CW","time":"0215z","timestamp":3000},
        {"spotter":"X","dxCall":"","freq":"1","timestamp":9000}])", filterSpots, applySpots));
    CHECK(m.nSpots == 3);
    CHECK(!strcmp(m.spots[0].dx, "ZS6ABC")); CHECK(!strcmp(m.spots[0].mode, "CW")); CHECK(m.spots[0].band == B15);
    CHECK(!strcmp(m.spots[1].dx, "JA1XYZ")); CHECK(!strcmp(m.spots[1].mode, "FT8")); CHECK(m.spots[1].hasPos);
    CHECK(!strcmp(m.spots[1].time, "0214")); CHECK(!strcmp(m.spots[1].de, "EA5ABC"));
    CHECK(!strcmp(m.spots[2].dx, "VK3QQ")); CHECK(!m.spots[2].hasPos); CHECK(!strcmp(m.spots[2].mode, "SSB"));
    CHECK(!strcmp(m.spots[2].time, "0210")); CHECK(!strcmp(m.spots[2].freq, "7.155"));
    {   // mas spots de los que caben: se quedan los mas nuevos
        std::string big = "[";
        for (int i = 0; i < 60; i++) {
            char b[160];
            snprintf(b, sizeof(b), "%s{\"spotter\":\"A\",\"dxCall\":\"C%d\",\"freq\":\"14.2\",\"timestamp\":%d}", i ? "," : "", i, i);
            big += b;
        }
        big += "]";
        CHECK(parse(m, big.c_str(), filterSpots, applySpots));
        CHECK(m.nSpots == MAX_SPOTS); CHECK(!strcmp(m.spots[0].dx, "C59")); CHECK(!strcmp(m.spots[MAX_SPOTS - 1].dx, "C20"));
    }
    CHECK(!parse(m, R"({"error":"x"})", filterSpots, applySpots));

    // POTA (kHz) y SOTA (MHz)
    CHECK(parse(m, R"([{"spotId":1,"activator":"EA4XX","frequency":"14244","mode":"SSB","reference":"ES-0123","name":"Parque"},
        {"activator":"DL2YY/P","frequency":"7032.5","mode":"CW","reference":"DE-0456"}])", filterPota, applyPota));
    CHECK(m.nPota == 2); CHECK(m.totalPota == 2); CHECK(!strcmp(m.pota[0].freq, "14.244")); CHECK(!strcmp(m.pota[1].freq, "7.033") || !strcmp(m.pota[1].freq, "7.032"));
    CHECK(parse(m, R"([{"id":1,"activatorCallsign":"OE5ZZ/P","summitCode":"OE/TI-012","frequency":"10.118","mode":"cw",
        "summitDetails":{"name":"x"}}])", filterSota, applySota));
    CHECK(m.nSota == 1); CHECK(m.sota[0].sota); CHECK(!strcmp(m.sota[0].mode, "CW")); CHECK(!strcmp(m.sota[0].ref, "OE/TI-012"));

    // satelites: numeros o textos; sin OMM o geoestacionario se descarta
    CHECK(parse(m, R"x({"timestamp":1,"data":{"ISS":{"name":"ISS","norad":25544,"omm":{"OBJECT_NAME":"ISS (ZARYA)",
        "EPOCH":"2026-11-10T12:00:00.000","MEAN_MOTION":15.5,"ECCENTRICITY":0.0005,"INCLINATION":51.64,"RA_OF_ASC_NODE":100,
        "ARG_OF_PERICENTER":50,"MEAN_ANOMALY":200}},"SO-50":{"name":"SO-50","omm":{"EPOCH":"2026-11-10T00:00:00",
        "MEAN_MOTION":"14.75","ECCENTRICITY":"0.006","INCLINATION":"64.55","RA_OF_ASC_NODE":"10","ARG_OF_PERICENTER":"20",
        "MEAN_ANOMALY":"30"}},"QO-100":{"name":"QO-100","omm":{"EPOCH":"2026-11-10T00:00:00","MEAN_MOTION":1.0027}},
        "ROTO":{"name":"ROTO"}}})x", filterSats, applySats));
    CHECK(m.nSats == 2); CHECK(!strcmp(m.sats[0].name, "ISS")); NEAR(m.sats[1].n, 14.75 * 2 * 3.14159265358979 / 1440, 1e-9);
    NEAR(m.sats[0].epochJd, 2440587.5 + 1794312000.0 / 86400, 1e-6);
    Pass p[6];
    int n = findPasses(m.sats, m.nSats, 1794355200, 37.39, -5.98, p, 6);
    CHECK(n >= 2 && n <= 6);
    for (int i = 1; i < n; i++) CHECK(p[i].aos >= p[i - 1].aos);
    for (int i = 0; i < n; i++) CHECK(p[i].maxEl >= 10 && p[i].maxEl <= 90);

    // noticias: una linea, con las rayas tipograficas pasadas a ASCII y sin lo que no es Latin-1
    CHECK(parse(m, "{\"items\":[{\"title\":\"3Y0K \xE2\x80\x94 Bouvet\",\"url\":\"x\"},{\"title\":\"Espa\xC3\xB1" "a \xE6\x97\xA5 ok\"}]}",
                filterNews, applyNews));
    CHECK(!strcmp(m.news, "3Y0K - Bouvet  \xC2\xB7  Espa\xC3\xB1" "a  ok"));

    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
