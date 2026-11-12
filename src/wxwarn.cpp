/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Avisos meteorologicos (ver wxwarn.h).
 */
#include "wxwarn.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace wxw {

// Contenido de la primera etiqueta <name> o <cap:name> a partir de "from"; npos si no esta
static size_t findTag(const std::string& x, const char* name, size_t from, std::string& val) {
    const size_t nl = strlen(name);
    size_t p = from;
    while ((p = x.find('<', p)) != std::string::npos) {
        size_t q = p + 1;
        if (x.compare(q, 4, "cap:") == 0) q += 4;
        if (x.compare(q, nl, name) == 0 && q + nl < x.size() && (x[q + nl] == '>' || x[q + nl] == ' ')) {
            const size_t gt = x.find('>', q + nl);
            if (gt == std::string::npos) return std::string::npos;
            const size_t end = x.find("</", gt);
            if (end == std::string::npos) return std::string::npos;
            val = x.substr(gt + 1, end - gt - 1);
            return end;
        }
        p++;
    }
    return std::string::npos;
}
static std::string clean(std::string s) {
    static const char* ent[][2] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
    for (auto& e : ent) for (size_t p; (p = s.find(e[0])) != std::string::npos;) s.replace(p, strlen(e[0]), e[1]);
    for (size_t p = 0; (p = s.find("&#", p)) != std::string::npos;) {         // &#241; -> UTF-8
        const size_t e = s.find(';', p);
        if (e == std::string::npos || e - p > 8) { p += 2; continue; }
        const unsigned long cp = s[p + 2] == 'x' ? strtoul(s.c_str() + p + 3, nullptr, 16) : strtoul(s.c_str() + p + 2, nullptr, 10);
        std::string u;
        if (cp < 0x80) u += (char)cp;
        else if (cp < 0x800) { u += (char)(0xC0 | (cp >> 6)); u += (char)(0x80 | (cp & 0x3F)); }
        else { u += (char)(0xE0 | (cp >> 12)); u += (char)(0x80 | ((cp >> 6) & 0x3F)); u += (char)(0x80 | (cp & 0x3F)); }
        s.replace(p, e - p + 1, u);
        p += u.size();
    }
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
static std::string tag(const std::string& x, const char* name) { std::string v; return findTag(x, name, 0, v) == std::string::npos ? "" : clean(v); }
// <parameter> o <geocode> o <eventCode> con ese valueName: su <value>
static std::string named(const std::string& x, const char* valueName) {
    size_t p = x.find(valueName);
    if (p == std::string::npos) return "";
    std::string v;
    return findTag(x, "value", p, v) == std::string::npos ? "" : clean(v);
}
static std::string lower(std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; }

int64_t isoToEpoch(const std::string& s) {
    if (s.size() < 19 || s[4] != '-' || s[10] != 'T') return 0;
    auto n = [&](size_t i, size_t len) { return atoi(s.substr(i, len).c_str()); };
    int y = n(0, 4), m = n(5, 2), d = n(8, 2), hh = n(11, 2), mi = n(14, 2), ss = n(17, 2);
    if (m < 1 || m > 12 || d < 1 || d > 31) return 0;
    // dias desde 1970 (algoritmo civil de Howard Hinnant)
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400), doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t t = (era * 146097 + (int64_t)doe - 719468) * 86400 + hh * 3600 + mi * 60 + ss;
    size_t z = 19;
    while (z < s.size() && s[z] != '+' && s[z] != '-' && s[z] != 'Z') z++;      // decimas de segundo
    if (z < s.size() && s[z] != 'Z' && z + 5 < s.size() + 1) {
        const int off = atoi(s.substr(z + 1, 2).c_str()) * 3600 + (s.size() >= z + 6 ? atoi(s.substr(z + 4, 2).c_str()) * 60 : 0);
        t += s[z] == '+' ? -off : off;
    }
    return t;
}
std::string shortTime(const std::string& s) { return s.size() >= 16 ? s.substr(8, 2) + " " + s.substr(11, 5) : s; }
const char* levelName(int l) { return l == RED ? "red" : l == ORANGE ? "orange" : l == YELLOW ? "yellow" : "-"; }
uint32_t levelColor(int l) { return l == RED ? 0xFF5A5A : l == ORANGE ? 0xFF9A3C : 0xFFD84D; }

// distancia en km entre dos puntos cercanos (plana, de sobra para "a cuanto esta la zona")
static double flatKm(double la1, double lo1, double la2, double lo2) {
    const double dy = (la2 - la1) * 111.2, dx = (lo2 - lo1) * 111.2 * cos((la1 + la2) * M_PI / 360);
    return sqrt(dx * dx + dy * dy);
}
// Poligono CAP: "lat,lon lat,lon ...". Dentro (cruces de un rayo) y distancia al vertice mas cercano.
static bool polygon(const std::string& poly, double lat, double lon, bool& inside, float& distKm) {
    std::vector<double> ys, xs;
    const char* p = poly.c_str();
    while (*p) {
        char* e;
        const double la = strtod(p, &e);
        if (e == p || *e != ',') break;
        p = e + 1;
        const double lo = strtod(p, &e);
        if (e == p) break;
        ys.push_back(la); xs.push_back(lo);
        p = e;
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++;
    }
    const size_t n = ys.size();
    if (n < 3) return false;
    inside = false;
    double best = 1e9;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((ys[i] > lat) != (ys[j] > lat)) && (lon < (xs[j] - xs[i]) * (lat - ys[i]) / (ys[j] - ys[i]) + xs[i])) inside = !inside;
        const double d = flatKm(lat, lon, ys[i], xs[i]);
        if (d < best) best = d;
    }
    distKm = inside ? 0 : (float)best;
    return true;
}

bool parse(const std::string& xml, Warning& w, bool havePos, double lat, double lon) {
    w = Warning();
    const bool atom = xml.compare(0, 6, "<entry") == 0;
    const std::string status = lower(tag(xml, "status")), type = lower(tag(xml, atom ? "message_type" : "msgType"));
    if (!status.empty() && status != "actual") return false;                // pruebas y ejercicios
    if (type == "cancel") return false;
    // AEMET repite el aviso en varios idiomas: vale el primer <info>
    std::string info = xml;
    if (!atom) { const size_t a = xml.find("<info"), b = xml.find("</info>"); if (a != std::string::npos && b != std::string::npos && b > a) info = xml.substr(a, b - a); }
    w.source = atom ? "Meteoalarm" : "AEMET";
    w.id = tag(xml, "identifier");
    if (w.id.empty()) w.id = tag(xml, "id");
    w.event = tag(info, "event");
    w.area = tag(info, "areaDesc");
    w.desc = tag(info, "description");
    w.instr = tag(info, "instruction");
    w.onset = tag(info, "onset");
    if (w.onset.empty()) w.onset = tag(info, "effective");
    w.expires = tag(info, "expires");
    w.onsetS = isoToEpoch(w.onset);
    w.expiresS = isoToEpoch(w.expires);
    const std::string nivel = lower(named(info, "AEMET-Meteoalerta nivel")), sev = lower(tag(info, "severity"));
    if (!nivel.empty()) w.level = nivel == "rojo" ? RED : nivel == "naranja" ? ORANGE : nivel == "amarillo" ? YELLOW : NONE;
    else w.level = sev == "extreme" ? RED : sev == "severe" ? ORANGE : sev == "moderate" ? YELLOW : NONE;
    if (w.level == NONE || w.event.empty()) return false;
    w.prob = named(info, "AEMET-Meteoalerta probabilidad");
    w.phen = named(info, "AEMET-Meteoalerta fenomeno");                     // "PR;Lluvias"
    const size_t sc = w.phen.find(';');
    if (sc != std::string::npos) w.phen = w.phen.substr(sc + 1);
    if (w.desc.empty()) {                                                   // "P1;Precipitacion acumulada en una hora;30 mm"
        std::string par = named(info, "AEMET-Meteoalerta parametro");
        const size_t s1 = par.find(';');
        if (s1 != std::string::npos) { par = par.substr(s1 + 1); for (char& c : par) if (c == ';') c = ' '; w.desc = par; }
    }
    const std::string poly = tag(info, "polygon");
    if (havePos && !poly.empty()) w.hasArea = polygon(poly, lat, lon, w.inside, w.distKm);
    return true;
}

// minusculas y sin los acentos mas corrientes (UTF-8 de dos bytes C3 xx)
static std::string fold(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char d = (unsigned char)s[++i] | 0x20;                 // a minuscula
            o += d >= 0xA0 && d <= 0xA5 ? 'a' : d >= 0xA8 && d <= 0xAB ? 'e' : d >= 0xAC && d <= 0xAF ? 'i' : d == 0xB1 ? 'n' : (d >= 0xB2 && d <= 0xB6) ? 'o'
                 : d >= 0xB9 && d <= 0xBC ? 'u' : '?';
        } else o += (char)tolower(c);
    }
    return o;
}
bool areaMatches(const std::string& area, const std::string& filter) {
    const std::string a = fold(area);
    size_t p = 0;
    bool any = false;
    while (p <= filter.size()) {
        size_t q = filter.find(',', p);
        if (q == std::string::npos) q = filter.size();
        std::string word = fold(filter.substr(p, q - p));
        size_t s = 0, e = word.size();
        while (s < e && word[s] == ' ') s++;
        while (e > s && word[e - 1] == ' ') e--;
        word = word.substr(s, e - s);
        if (!word.empty()) { any = true; if (a.find(word) != std::string::npos) return true; }
        p = q + 1;
    }
    return !any;                                                            // sin filtro, todo vale
}
bool expired(const Warning& w, int64_t nowS) { return w.expiresS > 0 && nowS > 0 && nowS > w.expiresS; }
bool before(const Warning& a, const Warning& b) {
    if (a.inside != b.inside) return a.inside;
    if (a.level != b.level) return a.level > b.level;
    if (a.onsetS != b.onsetS) return a.onsetS < b.onsetS;
    return a.area < b.area;
}

bool Scanner::feed(const uint8_t* data, size_t n, std::string& out) {
    if (first_ && n >= 2) { first_ = false; if (data[0] == 0x1F && data[1] == 0x8B) gzip_ = true; }
    if (gzip_) return false;
    if (n) buf_.append((const char*)data, n);
    for (;;) {
        if (!in_) {
            const size_t a = buf_.find("<alert"), e = buf_.find("<entry");
            size_t s = std::string::npos;
            if (a != std::string::npos && (e == std::string::npos || a < e)) { s = a; kind_ = 1; }
            else if (e != std::string::npos) { s = e; kind_ = 2; }
            if (s == std::string::npos) { if (buf_.size() > 8) buf_.erase(0, buf_.size() - 8); return false; }   // puede quedar media etiqueta
            buf_.erase(0, s);
            in_ = true;
        }
        const char* endTag = kind_ == 1 ? "</alert>" : "</entry>";
        const size_t e = buf_.find(endTag);
        if (e == std::string::npos) {
            if (buf_.size() > MAX) { out = buf_.substr(0, MAX); buf_.clear(); in_ = false; blocks_++; return true; }
            return false;
        }
        out = buf_.substr(0, e + strlen(endTag));
        buf_.erase(0, e + strlen(endTag));
        in_ = false;
        blocks_++;
        return true;
    }
}

} // namespace wxw
