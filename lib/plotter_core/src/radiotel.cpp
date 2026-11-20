/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "radiotel.h"

#include <cctype>
#include <cmath>
#include <cstdio>

namespace plotter {

namespace {

struct Nat { const char* es; const char* en; const char* helpEs; const char* helpEn; };

// MAYDAY: en el orden de las naturalezas de socorro de la LSD (100 + indice)
const Nat kMayday[] = {
    {"INCENDIO / EXPLOSI\xC3\x93N", "FIRE / EXPLOSION", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"V\xC3\x8D" "A DE AGUA", "FLOODING", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"ABORDAJE", "COLLISION", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"VARADA", "GROUNDING", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"ESCORA, PELIGRO DE ZOZOBRA", "LISTING, IN DANGER OF CAPSIZING", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"HUNDIMIENTO", "SINKING", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"SIN GOBIERNO Y A LA DERIVA", "DISABLED AND ADRIFT", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"PELIGRO GRAVE E INMINENTE", "IN GRAVE AND IMMINENT DANGER", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"ABANDONAMOS EL BARCO", "ABANDONING SHIP", "SOLICITO RESCATE INMEDIATO", "I REQUIRE IMMEDIATE RESCUE"},
    {"PIRATER\xC3\x8D" "A / ATAQUE ARMADO", "PIRACY / ARMED ATTACK", "SOLICITO AUXILIO INMEDIATO", "I REQUIRE IMMEDIATE ASSISTANCE"},
    {"HOMBRE AL AGUA", "MAN OVERBOARD", "SOLICITO AUXILIO INMEDIATO PARA LA B\xC3\x9AQUEDA", "I REQUIRE IMMEDIATE ASSISTANCE TO SEARCH"},
};
const Nat kPan[] = {
    {"AVER\xC3\x8D" "A DE MOTOR, SIN PROPULSI\xC3\x93N", "ENGINE FAILURE, NO PROPULSION", "SOLICITO REMOLQUE", "I REQUIRE A TOW"},
    {"AVER\xC3\x8D" "A DEL TIM\xC3\x93N", "STEERING FAILURE", "SOLICITO ASISTENCIA", "I REQUIRE ASSISTANCE"},
    {"PERSONA ENFERMA O HERIDA A BORDO", "SICK OR INJURED PERSON ON BOARD", "SOLICITO CONSEJO M\xC3\x89" "DICO", "I REQUIRE MEDICAL ADVICE"},
    {"EVACUACI\xC3\x93N M\xC3\x89" "DICA", "MEDICAL EVACUATION", "SOLICITO EVACUACI\xC3\x93N M\xC3\x89" "DICA", "I REQUIRE MEDICAL EVACUATION"},
    {"HOMBRE AL AGUA, LO ESTAMOS BUSCANDO", "MAN OVERBOARD, WE ARE SEARCHING", "SOLICITO QUE LOS BARCOS CERCANOS VIGILEN", "I REQUEST VESSELS IN THE AREA TO KEEP A LOOKOUT"},
    {"SIN COMBUSTIBLE", "OUT OF FUEL", "SOLICITO REMOLQUE", "I REQUIRE A TOW"},
    {"URGENCIA", "URGENCY", "SOLICITO ASISTENCIA", "I REQUIRE ASSISTANCE"},
};
const Nat kSec[] = {
    {"PELIGRO PARA LA NAVEGACI\xC3\x93N", "NAVIGATIONAL HAZARD", "", ""},
    {"OBJETO A LA DERIVA", "DRIFTING OBJECT", "", ""},
    {"AVISO METEOROL\xC3\x93GICO", "WEATHER WARNING", "", ""},
    {"MANIOBRA RESTRINGIDA / REMOLQUE", "RESTRICTED IN ABILITY TO MANOEUVRE / TOWING", "", ""},
    {"AVISO DE SEGURIDAD", "SAFETY MESSAGE", "", ""},
};

const Nat* table(RtKind k, int& n) {
    switch (k) {
        case RtKind::PanPan: n = (int)(sizeof(kPan) / sizeof(kPan[0])); return kPan;
        case RtKind::Securite: n = (int)(sizeof(kSec) / sizeof(kSec[0])); return kSec;
        default: n = (int)(sizeof(kMayday) / sizeof(kMayday[0])); return kMayday;
    }
}
const Nat& nat(RtKind k, int i) {
    int n;
    const Nat* t = table(k, n);
    return t[(i >= 0 && i < n) ? i : n - 1];
}

const char* const kAlpha[26] = {"ALFA", "BRAVO", "CHARLIE", "DELTA", "ECHO", "FOXTROT", "GOLF", "HOTEL", "INDIA", "JULIETT",
                                "KILO", "LIMA", "MIKE", "NOVEMBER", "OSCAR", "PAPA", "QUEBEC", "ROMEO", "SIERRA", "TANGO",
                                "UNIFORM", "VICTOR", "WHISKEY", "X-RAY", "YANKEE", "ZULU"};
const char* const kDigEs[10] = {"CERO", "UNO", "DOS", "TRES", "CUATRO", "CINCO", "SEIS", "SIETE", "OCHO", "NUEVE"};
const char* const kDigEn[10] = {"ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"};

std::string upper(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0xC3 && i + 1 < s.size()) {             // a, e, i, o, u, n con tilde en UTF-8: a mayusculas
            unsigned char d = (unsigned char)s[i + 1];
            if (d >= 0xA0 && d <= 0xBE && d != 0xB7) d = (unsigned char)(d - 0x20);
            o += (char)c; o += (char)d; i++;
        } else o += (char)toupper(c);
    }
    return o;
}

std::string triple(const std::string& w, bool en) { return w + (en ? " " : ", ") + w + (en ? " " : ", ") + w; }

std::string hhmm(int m) {
    char b[16];
    snprintf(b, sizeof(b), "%02d%02d UTC", (m / 60) % 24, m % 60);
    return b;
}

}  // namespace

int rtNatureCount(RtKind k) { int n; table(k, n); return n; }
const char* rtNatureName(RtKind k, int i, RtLang l) { const Nat& t = nat(k, i); return l == RtLang::En ? t.en : t.es; }
int rtNatureFromDsc(int d) { return d >= 100 && d <= 110 ? d - 100 : -1; }

std::string rtPhonetic(const std::string& cs, RtLang l) {
    std::string o;
    for (char ch : cs) {
        const unsigned char c = (unsigned char)toupper((unsigned char)ch);
        const char* w = nullptr;
        if (c >= 'A' && c <= 'Z') w = kAlpha[c - 'A'];
        else if (c >= '0' && c <= '9') w = (l == RtLang::En ? kDigEn : kDigEs)[c - '0'];
        if (!w) continue;
        if (!o.empty()) o += ' ';
        o += w;
    }
    return o;
}

std::string rtDigits(const std::string& num, RtLang l) {
    std::string o;
    for (char c : num) {
        if (c < '0' || c > '9') continue;
        if (!o.empty()) o += ' ';
        o += (l == RtLang::En ? kDigEn : kDigEs)[c - '0'];
    }
    return o;
}

std::string rtPosition(double lat, double lon, RtLang l) {
    const bool en = l == RtLang::En;
    auto part = [&](double v, int degW, const char* pos, const char* neg) {
        const double a = std::fabs(v);
        int d = (int)a;
        double m = std::round((a - d) * 600.0) / 10.0;
        if (m >= 60.0) { d++; m = 0; }
        char b[96];
        snprintf(b, sizeof(b), "%0*d %s %04.1f %s %s", degW, d, en ? "DEGREES" : "GRADOS", m, en ? "MINUTES" : "MINUTOS", v >= 0 ? pos : neg);
        std::string s = b;
        if (!en) for (char& c : s) if (c == '.') c = ',';
        return s;
    };
    return part(lat, 2, en ? "NORTH" : "NORTE", en ? "SOUTH" : "SUR") + ", " + part(lon, 3, en ? "EAST" : "ESTE", en ? "WEST" : "OESTE");
}

std::string radiotelMessage(const RtInput& in) {
    const bool en = in.lang == RtLang::En;
    const std::string name = in.name.empty() ? (en ? "[VESSEL NAME]" : "[NOMBRE DEL BARCO]") : upper(in.name);
    std::string ident;                                   // distintivo y MMSI
    if (!in.callsign.empty()) ident += std::string(en ? "CALL SIGN " : "DISTINTIVO ") + rtPhonetic(in.callsign, in.lang);
    if (!in.mmsi.empty()) { if (!ident.empty()) ident += ", "; ident += "MMSI " + rtDigits(in.mmsi, in.lang); }
    auto position = [&](bool has, double la, double lo, int t) {
        std::string s = en ? "POSITION " : "SITUACI\xC3\x93N ";
        if (!has) return s + (en ? "[POSITION]" : "[SITUACI\xC3\x93N]");
        s += rtPosition(la, lo, in.lang);
        if (t >= 0) s += std::string(en ? " AT " : " A LAS ") + hhmm(t);
        return s;
    };
    auto persons = [&](int n) {
        char b[48];
        if (n == 1) snprintf(b, sizeof(b), "%s", en ? "1 PERSON ON BOARD" : "1 PERSONA A BORDO");
        else snprintf(b, sizeof(b), "%d %s", n, en ? "PERSONS ON BOARD" : "PERSONAS A BORDO");
        return std::string(b);
    };
    const Nat& t = nat(in.kind, in.nature);
    std::string m;
    auto line = [&](const std::string& s) { m += s; m += '\n'; };
    const std::string allSt = triple(en ? "ALL STATIONS" : "A TODAS LAS ESTACIONES", en);
    switch (in.kind) {
        case RtKind::Mayday:
            line(triple("MAYDAY", en));
            line(std::string(en ? "THIS IS " : "AQU\xC3\x8D ") + triple(name, en));
            if (!ident.empty()) line(ident);
            line("MAYDAY " + name + (in.callsign.empty() ? "" : ", " + upper(in.callsign)));
            line(position(in.hasPos, in.lat, in.lon, in.utcMin));
            line(en ? t.en : t.es);
            line(en ? t.helpEn : t.helpEs);
            break;
        case RtKind::MaydayRelay: {
            const std::string rn = in.relayName.empty() ? (en ? "UNKNOWN VESSEL" : "BUQUE DESCONOCIDO") : upper(in.relayName);
            line(triple("MAYDAY RELAY", en));
            line(allSt);
            line(std::string(en ? "THIS IS " : "AQU\xC3\x8D ") + triple(name, en));
            if (!ident.empty()) line(ident);
            line(std::string(en ? "RECEIVED THE FOLLOWING MAYDAY FROM " : "HE RECIBIDO EL SIGUIENTE MAYDAY DE ") + rn +
                 (in.relayMmsi.empty() ? "" : ", MMSI " + rtDigits(in.relayMmsi, in.lang)));
            line(position(in.relayHasPos, in.relayLat, in.relayLon, in.relayUtcMin));
            line(en ? t.en : t.es);
            if (in.relayPersons >= 0) line(persons(in.relayPersons));
            line(en ? "IMMEDIATE ASSISTANCE REQUIRED" : "NECESITA AUXILIO INMEDIATO");
            if (!in.extra.empty()) line(upper(in.extra));
            line(en ? "OVER" : "CAMBIO");
            return m;
        }
        case RtKind::PanPan:
            line(triple("PAN PAN", en));
            line(allSt);
            line(std::string(en ? "THIS IS " : "AQU\xC3\x8D ") + triple(name, en));
            if (!ident.empty()) line(ident);
            line(position(in.hasPos, in.lat, in.lon, in.utcMin));
            line(en ? t.en : t.es);
            line(en ? t.helpEn : t.helpEs);
            break;
        case RtKind::Securite:
            line(triple("S\xC3\x89" "CURIT\xC3\x89", en));
            line(allSt);
            line(std::string(en ? "THIS IS " : "AQU\xC3\x8D ") + triple(name, en));
            if (!ident.empty()) line(ident);
            line(en ? t.en : t.es);
            if (!in.extra.empty()) line(upper(in.extra));
            if (in.hasPos) line(std::string(en ? "IN POSITION " : "EN SITUACI\xC3\x93N ") + rtPosition(in.lat, in.lon, in.lang));
            line(en ? "OUT" : "TERMINADO");
            return m;
    }
    if (in.persons >= 0) line(persons(in.persons));
    if (!in.description.empty()) line(upper(in.description));
    if (!in.extra.empty()) line(upper(in.extra));
    line(en ? "OVER" : "CAMBIO");
    return m;
}

}  // namespace plotter
