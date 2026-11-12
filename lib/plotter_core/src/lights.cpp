/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lights.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace plotter {

double geographicRangeNm(double eye, double light) {
    if (eye < 0) eye = 0;
    if (light < 0) light = 0;
    return 2.03 * (std::sqrt(eye) + std::sqrt(light));
}

// Allard: E = I * T^d / d^2. El alcance nominal D es con T = 0,74 por milla (10 mn de visibilidad). Con visibilidad V,
// T = 0,05^(1/V) (contraste umbral 0,05). Se busca d con T^d / d^2 = 0,74^D / D^2.
double luminousRangeNm(double nominal, double vis) {
    if (nominal <= 0 || vis <= 0) return 0;
    const double lnT0 = std::log(0.05) / 10.0, lnT = std::log(0.05) / vis;
    const double target = nominal * lnT0 - 2 * std::log(nominal);         // ln del umbral relativo
    double lo = 1e-4, hi = 200.0;
    for (int i = 0; i < 80; i++) {
        double mid = 0.5 * (lo + hi);
        if (mid * lnT - 2 * std::log(mid) > target) lo = mid;               // aun se ve: mas lejos
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

double sightingRangeNm(double nominal, double vis, double eye, double light) {
    double a = luminousRangeNm(nominal, vis), b = geographicRangeNm(eye, light);
    return a < b ? a : b;
}

double swingRadiusM(double rode, double depth, double bow) {
    double h = depth + bow;
    if (rode <= h || h < 0) return 0;
    return std::sqrt(rode * rode - h * h);
}

double scopeRatio(double rode, double depth, double bow) {
    double h = depth + bow;
    return h > 0 ? rode / h : 0;
}

namespace {

bool starts(const char*& p, const char* word) {
    size_t n = std::strlen(word);
    for (size_t i = 0; i < n; i++)
        if (std::tolower(static_cast<unsigned char>(p[i])) != std::tolower(static_cast<unsigned char>(word[i]))) return false;
    p += n;
    return true;
}

void skip(const char*& p) {
    while (*p == ' ' || *p == '.') p++;
}

}  // namespace

bool parseLightCharacter(const char* text, LightCharacter& c) {
    c = LightCharacter();
    if (!text) return false;
    const char* p = text;
    skip(p);
    if (starts(p, "Gp")) skip(p);                                  // "GpD(2)", "Gp Oc(2)"
    // De la abreviatura mas larga a la mas corta
    struct { const char* w; LightClass k; } const kw[] = {
        {"LFl", LightClass::LongFlash}, {"L.Fl", LightClass::LongFlash}, {"DL", LightClass::LongFlash},
        {"VQ", LightClass::VeryQuick}, {"UQ", LightClass::UltraQuick}, {"Rp", LightClass::VeryQuick},
        {"Iso", LightClass::Isophase}, {"Oc", LightClass::Occulting}, {"Mo", LightClass::Morse},
        {"Alt", LightClass::Alternating}, {"Al", LightClass::Alternating}, {"Fl", LightClass::Flashing},
        {"Ct", LightClass::Quick}, {"Q", LightClass::Quick}, {"FD", LightClass::Flashing},
        {"F", LightClass::Fixed}, {"D", LightClass::Flashing},
    };
    for (const auto& k : kw)
        if (starts(p, k.w)) { c.cls = k.k; break; }
    if (c.cls == LightClass::Unknown) return false;
    skip(p);
    if (*p == '(') {
        p++;
        if (c.cls == LightClass::Morse) {
            if (!std::isalpha(static_cast<unsigned char>(*p))) return false;
            c.morse = char(std::toupper(static_cast<unsigned char>(*p++)));
        } else {
            char* e;
            long g = std::strtol(p, &e, 10);
            if (e == p || g < 1 || g > 12) return false;
            c.group = uint8_t(g);
            p = e;
            if (*p == '+') {
                g = std::strtol(p + 1, &e, 10);
                if (e == p + 1 || g < 1 || g > 12) return false;
                c.group2 = uint8_t(g);
                p = e;
            }
        }
        if (*p != ')') return false;
        p++;
    }
    skip(p);
    if (*p == '+') {                                               // "+LFl", "+DL"
        p++;
        skip(p);
        if (starts(p, "LFl") || starts(p, "L.Fl") || starts(p, "DL")) c.longFlashAfter = true;
        else return false;
        skip(p);
    }
    // Color: W / B blanco, R rojo, G / V verde, Y / A amarillo, Bu azul
    if (starts(p, "Bu")) c.color = LightColor::Blue;
    else if (*p && !std::isdigit(static_cast<unsigned char>(*p))) {
        switch (std::toupper(static_cast<unsigned char>(*p))) {
            case 'W': case 'B': c.color = LightColor::White; break;
            case 'R': c.color = LightColor::Red; break;
            case 'G': case 'V': c.color = LightColor::Green; break;
            case 'Y': case 'A': c.color = LightColor::Yellow; break;
            default: return false;
        }
        p++;
    }
    skip(p);
    if (std::isdigit(static_cast<unsigned char>(*p))) {
        char* e;
        c.periodS = std::strtod(p, &e);
        if (*e == ',') {                                           // coma decimal
            char* e2;
            long frac = std::strtol(e + 1, &e2, 10);
            double scale = 1;
            for (const char* q = e + 1; q < e2; q++) scale *= 10;
            c.periodS += frac / scale;
        }
    }
    return true;
}

bool lightIsOn(const LightCharacter& c, uint32_t tMs) {
    double step = 0, on = 0;                                       // paso entre destellos y duracion de cada uno, en segundos
    switch (c.cls) {
        case LightClass::Fixed: case LightClass::Unknown: case LightClass::Alternating: return true;
        case LightClass::Isophase: {
            double per = c.periodS > 0 ? c.periodS : 4.0;
            return std::fmod(tMs / 1000.0, per) < per / 2;
        }
        case LightClass::Quick: step = 1.0; on = 0.3; break;
        case LightClass::VeryQuick: step = 0.5; on = 0.15; break;
        case LightClass::UltraQuick: step = 0.25; on = 0.08; break;
        case LightClass::LongFlash: step = 3.0; on = 2.0; break;
        case LightClass::Flashing: step = 2.0; on = 0.5; break;
        case LightClass::Occulting: step = 2.0; on = 1.0; break;   // aqui "on" es la ocultacion
        case LightClass::Morse: step = 0; on = 0; break;
    }
    bool quick = c.cls == LightClass::Quick || c.cls == LightClass::VeryQuick || c.cls == LightClass::UltraQuick;
    if (quick && c.periodS <= 0 && c.group <= 1 && !c.longFlashAfter) return std::fmod(tMs / 1000.0, step) < on;   // continuo
    double per = c.periodS > 0 ? c.periodS : 10.0;
    double t = std::fmod(tMs / 1000.0, per);
    if (c.cls == LightClass::Morse) {
        static const char* const code[26] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
                                             "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.."};
        const char* m = (c.morse >= 'A' && c.morse <= 'Z') ? code[c.morse - 'A'] : ".-";
        double at = 0;
        for (; *m; m++) {
            double d = *m == '.' ? 0.5 : 1.5;
            if (t >= at && t < at + d) return true;
            at += d + 0.5;
        }
        return false;
    }
    int total = c.group + c.group2;
    double span = total * step + (c.group2 ? step : 0) + (c.longFlashAfter ? 2.5 : 0);
    if (span > per) { double k = per / span; step *= k; on *= k; }     // no cabe en el periodo: se aprieta
    bool pulse = false;
    double at = 0;
    for (int i = 0; i < total; i++) {
        if (c.group2 && i == c.group) at += step;                       // pausa entre los dos grupos
        if (t >= at && t < at + on) pulse = true;
        at += step;
    }
    if (c.longFlashAfter && t >= at && t < at + 2.0) pulse = true;
    return c.cls == LightClass::Occulting ? !pulse : pulse;
}

bool parseLightRecord(const char* line, LightRecord& out) {
    out = LightRecord();
    if (!line) return false;
    while (*line == ' ' || *line == '\t') line++;
    if (!*line || *line == '#') return false;
    const char* f[6];
    size_t len[6];
    int n = 0;
    const char* p = line;
    while (n < 6) {
        f[n] = p;
        const char* e = std::strchr(p, ';');
        len[n] = e ? size_t(e - p) : std::strlen(p);
        n++;
        if (!e) break;
        p = e + 1;
    }
    if (n < 3 || len[0] == 0) return false;
    auto copy = [](char* dst, size_t cap, const char* src, size_t l) {
        while (l > 0 && (src[l - 1] == ' ' || src[l - 1] == '\r' || src[l - 1] == '\n')) l--;
        if (l >= cap) l = cap - 1;
        std::memcpy(dst, src, l);
        dst[l] = 0;
    };
    auto num = [&](int i, double& v) {
        if (i >= n || len[i] == 0) return false;
        char b[24];
        size_t l = len[i] < sizeof(b) - 1 ? len[i] : sizeof(b) - 1;
        std::memcpy(b, f[i], l);
        b[l] = 0;
        for (char* q = b; *q; q++) if (*q == ',') *q = '.';
        char* e;
        v = std::strtod(b, &e);
        return e != b;
    };
    copy(out.name, sizeof(out.name), f[0], len[0]);
    if (!num(1, out.lat) || !num(2, out.lon) || out.lat < -90 || out.lat > 90 || out.lon < -180 || out.lon > 180) return false;
    if (n > 3) {
        copy(out.characterText, sizeof(out.characterText), f[3], len[3]);
        parseLightCharacter(out.characterText, out.character);
    }
    num(4, out.heightM);
    num(5, out.nominalNm);
    return true;
}

double lightSightingRangeNm(const LightRecord& l, double vis, double eye) {
    if (l.nominalNm <= 0 || l.heightM <= 0) return 0;
    return sightingRangeNm(l.nominalNm, vis, eye, l.heightM);
}

bool lightCharacterFromSeamark(const char* ch, const char* group, const char* period, const char* colour, LightCharacter& out,
                               char* textOut, int textSize) {
    out = LightCharacter();
    if (!ch || !*ch) return false;
    // Se arma la abreviatura internacional y se lee con el mismo lector: "Q+LFl" con grupo 6 -> "Q(6)+LFl"
    char t[40];
    const char* plus = std::strchr(ch, '+');
    size_t head = plus ? size_t(plus - ch) : std::strlen(ch);
    if (head == 0 || head > 8) return false;
    int n = std::snprintf(t, sizeof(t), "%.*s", int(head), ch);
    if (group && *group && std::strcmp(group, "1") != 0) n += std::snprintf(t + n, sizeof(t) - size_t(n), "(%.8s)", group);
    if (plus) n += std::snprintf(t + n, sizeof(t) - size_t(n), "%.6s", plus);
    char c = 0;
    if (colour && *colour) {
        if (!std::strncmp(colour, "white", 5)) c = 'W';
        else if (!std::strncmp(colour, "red", 3)) c = 'R';
        else if (!std::strncmp(colour, "green", 5)) c = 'G';
        else if (!std::strncmp(colour, "yellow", 6) || !std::strncmp(colour, "amber", 5) || !std::strncmp(colour, "orange", 6)) c = 'Y';
    }
    bool blue = colour && !std::strncmp(colour, "blue", 4);
    if (c) n += std::snprintf(t + n, sizeof(t) - size_t(n), " %c", c);
    else if (blue) n += std::snprintf(t + n, sizeof(t) - size_t(n), " Bu");
    if (period && *period) n += std::snprintf(t + n, sizeof(t) - size_t(n), " %.6ss", period);
    if (n <= 0 || n >= int(sizeof(t))) return false;
    if (textOut && textSize > 0) std::snprintf(textOut, size_t(textSize), "%s", t);
    return parseLightCharacter(t, out);
}

bool lightRecordFromSeamark(const char* name, double lat, double lon, const char* ch, const char* group, const char* period,
                            const char* colour, const char* height, const char* range, LightRecord& out) {
    out = LightRecord();
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
    std::snprintf(out.name, sizeof(out.name), "%s", (name && *name) ? name : "Luz");
    for (char* q = out.name; *q; q++) if (*q == ';') *q = ',';         // el punto y coma separa campos en la lista
    out.lat = lat;
    out.lon = lon;
    lightCharacterFromSeamark(ch, group, period, colour, out.character, out.characterText, int(sizeof(out.characterText)));
    if (height && *height) out.heightM = std::atof(height);
    if (range && *range) out.nominalNm = std::atof(range);
    return true;
}

int overpassLightsUrl(char* out, int size, double s, double w, double n, double e) {
    // [out:json];node["seamark:light:character"](s,w,n,e);out; con los caracteres reservados codificados
    int k = std::snprintf(out, size_t(size),
                          "%s?data=%%5Bout%%3Ajson%%5D%%3Bnode%%5B%%22seamark%%3Alight%%3Acharacter%%22%%5D(%.4f,%.4f,%.4f,%.4f)%%3Bout%%3B",
                          kOverpassUrl, s, w, n, e);
    return (k > 0 && k < size) ? k : 0;
}

int formatLightRecord(char* out, int size, const LightRecord& l) {
    char h[16] = "", r[16] = "";
    if (l.heightM > 0) std::snprintf(h, sizeof(h), "%g", l.heightM);
    if (l.nominalNm > 0) std::snprintf(r, sizeof(r), "%g", l.nominalNm);
    int k = std::snprintf(out, size_t(size), "%s;%.5f;%.5f;%s;%s;%s", l.name, l.lat, l.lon, l.characterText, h, r);
    return (k > 0 && k < size) ? k : 0;
}

const char* ialaMarkNameEs(IalaMark m) {
    static const char* const k[] = {"Sin identificar", "Lateral de babor", "Lateral de estribor", "Canal principal a estribor",
                                    "Canal principal a babor", "Cardinal norte", "Cardinal este", "Cardinal sur", "Cardinal oeste",
                                    "Peligro aislado", "Aguas navegables", "Marca especial", "Naufragio reciente", "Luz",
                                    "Enfilacion", "Racon", "Estructura"};
    return k[int(m) <= 16 ? int(m) : 0];
}

IalaMark ialaMarkFromAisAton(int t, bool* floating) {
    if (floating) *floating = t >= 20 && t <= 31;
    static const IalaMark fixedOrFloat[11] = {IalaMark::CardinalN, IalaMark::CardinalE, IalaMark::CardinalS, IalaMark::CardinalW,
                                              IalaMark::PortHand, IalaMark::StarboardHand, IalaMark::PreferredChannelPort,
                                              IalaMark::PreferredChannelStarboard, IalaMark::IsolatedDanger, IalaMark::SafeWater,
                                              IalaMark::Special};
    if (t >= 9 && t <= 19) return fixedOrFloat[t - 9];
    if (t >= 20 && t <= 30) return fixedOrFloat[t - 20];
    switch (t) {
        case 2: return IalaMark::Racon;
        case 3: case 31: return IalaMark::Structure;
        case 4: return IalaMark::EmergencyWreck;
        case 5: case 6: return IalaMark::Light;
        case 7: case 8: return IalaMark::LeadingLight;
        default: return IalaMark::Unknown;
    }
}

IalaMark ialaMarkFromLight(const LightCharacter& c, bool regionB) {
    bool quick = c.cls == LightClass::Quick || c.cls == LightClass::VeryQuick || c.cls == LightClass::UltraQuick;
    if (c.color == LightColor::Yellow) return IalaMark::Special;
    if (c.color == LightColor::Blue) return IalaMark::EmergencyWreck;
    if (c.color == LightColor::Red || c.color == LightColor::Green) {
        bool red = c.color == LightColor::Red;
        bool port = red != regionB;                                // region A: rojo a babor
        if (c.cls == LightClass::Flashing && c.group == 2 && c.group2 == 1)
            return port ? IalaMark::PreferredChannelPort : IalaMark::PreferredChannelStarboard;
        return port ? IalaMark::PortHand : IalaMark::StarboardHand;
    }
    if (c.color != LightColor::White) return IalaMark::Unknown;
    if (quick) {
        if (c.longFlashAfter && c.group == 6) return IalaMark::CardinalS;
        if (c.group == 3) return IalaMark::CardinalE;
        if (c.group == 9) return IalaMark::CardinalW;
        if (c.group <= 1) return IalaMark::CardinalN;
        return IalaMark::Unknown;
    }
    if (c.cls == LightClass::Flashing && c.group == 2 && c.group2 == 0) return IalaMark::IsolatedDanger;
    if (c.cls == LightClass::Isophase || c.cls == LightClass::Occulting || c.cls == LightClass::LongFlash ||
        (c.cls == LightClass::Morse && c.morse == 'A'))
        return IalaMark::SafeWater;
    return IalaMark::Light;                                        // faro o luz de puerto
}

}  // namespace plotter
