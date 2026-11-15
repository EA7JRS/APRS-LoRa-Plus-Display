/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "dxcluster.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace dx {

static std::string upper(std::string s) { for (char& c : s) c = (char)toupper((unsigned char)c); return s; }
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\x07"), b = s.find_last_not_of(" \t\r\n\x07");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string band(double k) {
    struct B { double lo, hi; const char* n; };
    static const B T[] = {{1800, 2000, "160m"}, {3500, 4000, "80m"}, {5250, 5450, "60m"}, {7000, 7300, "40m"}, {10100, 10150, "30m"},
        {14000, 14350, "20m"}, {18068, 18168, "17m"}, {21000, 21450, "15m"}, {24890, 24990, "12m"}, {28000, 29700, "10m"},
        {50000, 54000, "6m"}, {70000, 70500, "4m"}, {144000, 148000, "2m"}, {430000, 440000, "70cm"}, {1240000, 1300000, "23cm"}};
    for (const B& b : T) if (k >= b.lo && k <= b.hi) return b.n;
    return "";
}

std::string mode(double k, const std::string& comment) {
    std::string c = upper(comment);
    auto word = [&](const char* w) {
        size_t p = 0, n = strlen(w);
        while ((p = c.find(w, p)) != std::string::npos) {
            bool l = p == 0 || !isalnum((unsigned char)c[p - 1]), r = p + n >= c.size() || !isalnum((unsigned char)c[p + n]);
            if (l && r) return true;
            p += n;
        }
        return false;
    };
    for (const char* w : {"FT8", "FT4", "RTTY", "PSK31", "PSK", "JT65", "JS8", "MSK144", "Q65", "FST4", "WSPR", "SSTV", "OLIVIA", "DIGI", "DATA"})
        if (word(w)) return "DIG";
    if (word("CW") || c.find("WPM") != std::string::npos) return "CW";
    if (word("SSB") || word("USB") || word("LSB") || word("AM") || word("FM") || word("PHONE")) return "PHO";
    // FT8 / FT4 en sus frecuencias de siempre
    static const double ft[] = {1840, 3573, 5357, 7074, 10136, 14074, 18100, 21074, 24915, 28074, 50313, 3575.5, 7047.5, 10140, 14080, 18104, 21140, 24919, 28180};
    for (double f : ft) if (k >= f - 1 && k <= f + 3) return "DIG";
    // plan de bandas de la IARU R1 (aproximado): CW abajo, digitales en medio, fonia arriba
    struct P { double lo, cwEnd, digEnd; };
    static const P T[] = {{1800, 1838, 1843}, {3500, 3570, 3600}, {7000, 7040, 7060}, {10100, 10140, 10150}, {14000, 14070, 14100},
        {18068, 18095, 18111}, {21000, 21070, 21150}, {24890, 24915, 24931}, {28000, 28070, 28300}, {50000, 50100, 50300}, {144000, 144150, 144400}};
    for (const P& p : T) if (k >= p.lo && k < p.lo + 5000 && band(k) == band(p.lo)) {
        if (k < p.cwEnd) return "CW";
        if (k < p.digEnd) return "DIG";
        return "PHO";
    }
    return "PHO";
}

bool parse(const std::string& raw, Spot& s) {
    std::string l = trim(raw);
    s = Spot();
    // en directo: "DX de EA7JRS-#:    7023.0  DL1ABC       CW 18 dB 24 WPM CQ                    1234Z"
    if (l.size() > 6 && (l.compare(0, 6, "DX de ") == 0 || l.compare(0, 6, "DX DE ") == 0)) {
        size_t colon = l.find(':', 6);
        if (colon == std::string::npos) return false;
        s.spotter = trim(l.substr(6, colon - 6));
        std::string rest = trim(l.substr(colon + 1));
        char* end = nullptr;
        s.khz = strtod(rest.c_str(), &end);
        if (s.khz <= 0 || !end) return false;
        rest = trim(std::string(end));
        size_t sp = rest.find(' ');
        s.call = upper(sp == std::string::npos ? rest : rest.substr(0, sp));
        rest = sp == std::string::npos ? "" : trim(rest.substr(sp));
        // la hora va al final: "1234Z"
        if (rest.size() >= 5 && rest.back() == 'Z' && isdigit((unsigned char)rest[rest.size() - 2])) {
            size_t t = rest.find_last_of(' ');
            s.time = t == std::string::npos ? rest : rest.substr(t + 1);
            rest = t == std::string::npos ? "" : trim(rest.substr(0, t));
        }
        s.comment = rest;
    } else {
        // sh/dx: "  7023.0  DL1ABC      10-Nov-2026 1234Z  CW 18 dB                  <EA7JRS-#>"
        char* end = nullptr;
        s.khz = strtod(l.c_str(), &end);
        if (s.khz < 1800 || !end || end == l.c_str()) return false;
        std::string rest = trim(std::string(end));
        size_t sp = rest.find(' ');
        if (sp == std::string::npos) return false;
        s.call = upper(rest.substr(0, sp));
        rest = trim(rest.substr(sp));
        size_t lt = rest.rfind('<'), gt = rest.rfind('>');
        if (lt != std::string::npos && gt != std::string::npos && gt > lt) { s.spotter = rest.substr(lt + 1, gt - lt - 1); rest = trim(rest.substr(0, lt)); }
        sp = rest.find(' ');                                            // fecha
        if (sp != std::string::npos && rest.find('-') < sp) rest = trim(rest.substr(sp));
        if (rest.size() >= 5 && rest[4] == 'Z') { s.time = rest.substr(0, 5); rest = trim(rest.substr(5)); }
        s.comment = rest;
        if (s.spotter.empty()) return false;
    }
    if (s.call.empty() || s.khz <= 0) return false;
    s.skimmer = s.spotter.find("-#") != std::string::npos;
    s.band = band(s.khz);
    s.mode = mode(s.khz, s.comment);
    std::string c = upper(s.comment);
    for (const char* a : {"POTA", "SOTA", "IOTA", "WWFF", "DME"}) if (c.find(a) != std::string::npos) { s.activity = a; break; }
    return true;
}

bool pass(const Filter& f, const Spot& s) {
    if (s.skimmer) { if (s.mode == "CW" ? !f.rbnCw : !f.rbnDig) return false; }
    else if (!f.human) return false;
    if (s.mode == "CW" && !f.cw) return false;
    if (s.mode == "PHO" && !f.pho) return false;
    if (s.mode == "DIG" && !f.dig) return false;
    if (!f.band.empty() && s.band != f.band) return false;
    if (!f.activity.empty() && s.activity != f.activity) return false;
    return true;
}

} // namespace dx
