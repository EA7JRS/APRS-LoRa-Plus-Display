/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "navwarn.h"

#include <cctype>
#include <cstdlib>

namespace plotter {

namespace {

std::string unescape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '&') { o += s[i]; continue; }
        size_t e = s.find(';', i);
        if (e == std::string::npos || e - i > 8) { o += s[i]; continue; }
        std::string ent = s.substr(i + 1, e - i - 1);
        if (ent == "amp") o += '&';
        else if (ent == "lt") o += '<';
        else if (ent == "gt") o += '>';
        else if (ent == "quot") o += '"';
        else if (ent == "apos") o += '\'';
        else if (!ent.empty() && ent[0] == '#') {
            long cp = ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X') ? std::strtol(ent.c_str() + 2, nullptr, 16) : std::strtol(ent.c_str() + 1, nullptr, 10);
            if (cp == 10 || cp == 13) o += '\n';
            else if (cp >= 32 && cp < 0x80) o += char(cp);
            else if (cp >= 0x80 && cp < 0x800) { o += char(0xC0 | (cp >> 6)); o += char(0x80 | (cp & 0x3F)); }
            else if (cp >= 0x800 && cp < 0x10000) { o += char(0xE0 | (cp >> 12)); o += char(0x80 | ((cp >> 6) & 0x3F)); o += char(0x80 | (cp & 0x3F)); }
        } else { o += s[i]; continue; }
        i = e;
    }
    return o;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) a++;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) b--;
    return s.substr(a, b - a);
}

// Contenido del primer <tag>...</tag> dentro de [from, to)
std::string child(const std::string& xml, size_t from, size_t to, const char* tag) {
    std::string open = std::string("<") + tag + ">", close = std::string("</") + tag + ">";
    size_t a = xml.find(open, from);
    if (a == std::string::npos || a >= to) return "";
    a += open.size();
    size_t b = xml.find(close, a);
    if (b == std::string::npos || b > to) return "";
    return trim(unescape(xml.substr(a, b - a)));
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Lee "GG-MM.mmm" o "GG MM,mmm" (o "GGG...") seguido del hemisferio. Devuelve true y avanza "i" si casa.
bool readCoord(const std::string& s, size_t& i, int degDigits, const char* hemis, double& value) {
    size_t p = i;
    int deg = 0;
    for (int k = 0; k < degDigits; k++) {
        if (p >= s.size() || !isDigit(s[p])) return false;
        deg = deg * 10 + (s[p++] - '0');
    }
    if (p < s.size() && isDigit(s[p])) return false;               // mas digitos de la cuenta: no es esto
    if (p >= s.size() || (s[p] != '-' && s[p] != ' ')) return false;
    p++;
    if (p + 1 >= s.size() || !isDigit(s[p]) || !isDigit(s[p + 1])) return false;
    double min = (s[p] - '0') * 10 + (s[p + 1] - '0');
    p += 2;
    if (p < s.size() && (s[p] == '.' || s[p] == ',') && p + 1 < s.size() && isDigit(s[p + 1])) {
        p++;
        double scale = 0.1;
        while (p < s.size() && isDigit(s[p])) { min += (s[p++] - '0') * scale; scale /= 10; }
    }
    while (p < s.size() && s[p] == ' ') p++;
    if (p >= s.size() || (s[p] != hemis[0] && s[p] != hemis[1])) return false;
    if (p + 1 < s.size() && std::isalpha(static_cast<unsigned char>(s[p + 1]))) return false;      // "N" suelta, no "NE" ni una palabra
    if (min >= 60.0) return false;
    value = deg + min / 60.0;
    if (s[p] == hemis[1]) value = -value;
    i = p + 1;
    return true;
}

}  // namespace

bool navWarningsParseIhm(const std::string& xml, std::vector<NavWarning>& out, std::string* generated) {
    out.clear();
    size_t root = xml.find("<dataroot");
    if (root == std::string::npos) return false;
    if (generated) {
        generated->clear();
        size_t end = xml.find('>', root), g = xml.find("generated=\"", root);
        if (g != std::string::npos && g < end) {
            g += 11;
            size_t q = xml.find('"', g);
            if (q != std::string::npos) *generated = xml.substr(g, q - g);
        }
    }
    static const char kOpen[] = "<NAVAREASVIGOR>", kClose[] = "</NAVAREASVIGOR>";
    size_t pos = root;
    for (;;) {
        size_t a = xml.find(kOpen, pos);
        if (a == std::string::npos) break;
        size_t b = xml.find(kClose, a);
        if (b == std::string::npos) break;
        NavWarning w;
        w.number = child(xml, a, b, "nnunaf");
        if (w.number.empty()) w.number = child(xml, a, b, "nnuna");
        w.date = child(xml, a, b, "nfemi").substr(0, 10);
        w.areaEs = child(xml, a, b, "nlocae");
        w.areaEn = child(xml, a, b, "nlocai");
        w.titleEs = child(xml, a, b, "nasun");
        w.titleEn = child(xml, a, b, "nasuni");
        w.textEs = child(xml, a, b, "ntees");
        w.textEn = child(xml, a, b, "ntein");
        navWarningPositions(w.textEn.empty() ? w.textEs : w.textEn, w.positions);
        if (!w.number.empty()) out.push_back(w);
        pos = b + sizeof(kClose) - 1;
    }
    return true;
}

int navWarningPositions(const std::string& s, std::vector<LatLon>& out) {
    int added = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (!isDigit(s[i]) || (i > 0 && (isDigit(s[i - 1]) || s[i - 1] == '.' || s[i - 1] == ','))) continue;
        size_t p = i;
        double lat, lon;
        if (!readCoord(s, p, 2, "NS", lat)) continue;
        while (p < s.size() && (s[p] == ' ' || s[p] == ',' || s[p] == '-' || s[p] == '/' || s[p] == '\n' || s[p] == '\r')) p++;
        if (!readCoord(s, p, 3, "EW", lon)) continue;
        if (lat > 90 || lat < -90 || lon > 180 || lon < -180) continue;
        out.push_back({lat, lon});
        added++;
        i = p - 1;
    }
    return added;
}

double navWarningDistanceNm(const NavWarning& w, LatLon here) {
    double best = -1;
    for (const LatLon& p : w.positions) {
        double d = gcDistanceNm(here, p);
        if (best < 0 || d < best) best = d;
    }
    return best;
}

}  // namespace plotter
