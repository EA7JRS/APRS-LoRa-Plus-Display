/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Modelo del reloj: lectura de la API de OpenHamClock (formatos de server/routes del proyecto), sol y satelites.
 */
#include "hc_model.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hc {

// (Arduino.h ya define PI como macro)
static const double K_PI = 3.14159265358979323846, D2R = K_PI / 180, R2D = 180 / K_PI;

template <size_t N> static void cpy(char (&dst)[N], const char* src) {
    if (!src) src = "";
    snprintf(dst, N, "%s", src);
}
// numero que puede venir como numero o como texto ("148" en N0NBH, los OMM de Space-Track)
static double num(JsonVariantConst v, double def = NAN) {
    if (v.is<double>()) return v.as<double>();
    if (v.is<const char*>()) {
        const char* s = v.as<const char*>();
        char* end = nullptr;
        double d = strtod(s, &end);
        if (end != s) return d;
    }
    return def;
}

// ---------- bandas ----------
static const struct { double lo, hi; const char* name; uint32_t color; } BANDS[B_COUNT] = {
    {1.8, 2.0, "160m", 0x7C8CF8}, {3.5, 4.0, "80m", 0x5B9BF0}, {5.2, 5.5, "60m", 0x4FD1C5}, {7.0, 7.3, "40m", 0x28B5E1},
    {10.1, 10.15, "30m", 0x9FB0BF}, {14.0, 14.35, "20m", 0xFFB347}, {18.068, 18.168, "17m", 0x3CE68A},
    {21.0, 21.45, "15m", 0xC77DFF}, {24.89, 24.99, "12m", 0xF78FB3}, {28.0, 29.7, "10m", 0xFF6B6B},
    {50.0, 54.0, "6m", 0xF6E05E}, {144.0, 148.0, "2m", 0xE8EEF3}, {0, 0, "", 0x9FB0BF}};
Band bandOf(double mhz) {
    for (int i = 0; i < B_OTHER; i++)
        if (mhz >= BANDS[i].lo && mhz <= BANDS[i].hi) return (Band)i;
    return B_OTHER;
}
const char* bandName(Band b) { return BANDS[b < B_COUNT ? b : B_OTHER].name; }
uint32_t bandColor(Band b) { return BANDS[b < B_COUNT ? b : B_OTHER].color; }

// modo: lo que diga el comentario y, si no dice nada, el segmento de la banda
static void guessMode(char out[6], const char* comment, double mhz) {
    static const char* MODES[] = {"FT8", "FT4", "CW", "SSB", "RTTY", "PSK", "JS8", "FM", "AM", "SSTV", "USB", "LSB", "DIGI"};
    char up[96];
    size_t n = 0;
    for (const char* p = comment ? comment : ""; *p && n < sizeof(up) - 1; p++) up[n++] = (char)toupper((unsigned char)*p);
    up[n] = 0;
    for (const char* mo : MODES) {
        const char* f = strstr(up, mo);
        // palabra entera: "CW" si, "CWT" o "W1CW" no
        if (f && (f == up || !isalnum((unsigned char)f[-1])) && !isalnum((unsigned char)f[strlen(mo)])) {
            snprintf(out, 6, "%s", (!strcmp(mo, "USB") || !strcmp(mo, "LSB")) ? "SSB" : mo);
            return;
        }
    }
    static const double FT8[] = {1.840, 3.573, 5.357, 7.074, 10.136, 14.074, 18.100, 21.074, 24.915, 28.074, 50.313, 144.174};
    for (double f : FT8)
        if (mhz >= f - 0.0005 && mhz <= f + 0.003) { strcpy(out, "FT8"); return; }
    static const double CW_TOP[B_COUNT] = {1.84, 3.57, 5.354, 7.04, 10.13, 14.07, 18.095, 21.07, 24.915, 28.07, 50.1, 144.1, 0};
    Band b = bandOf(mhz);
    if (b == B_OTHER) { out[0] = 0; return; }
    strcpy(out, mhz < CW_TOP[b] ? "CW" : (b == B30 ? "DIGI" : "SSB"));
}

// ---------- /api/solar-indices ----------
void filterSolar(JsonDocument& f) {
    f["sfi"]["current"] = true;
    f["ssn"]["current"] = true;
    f["kp"]["current"] = true;
    f["kp"]["history"][0]["value"] = true;
    f["bz"]["current"] = true;
}
bool applySolar(Model& m, JsonVariantConst v) {
    if (!v["sfi"].is<JsonObjectConst>() && !v["kp"].is<JsonObjectConst>()) return false;
    double d = num(v["sfi"]["current"]);
    if (!std::isnan(d)) m.sfi = (float)d;
    d = num(v["ssn"]["current"]);
    if (!std::isnan(d)) m.ssn = (int)lround(d);
    d = num(v["kp"]["current"]);
    if (!std::isnan(d)) m.kp = (float)d;
    d = num(v["bz"]["current"]);
    if (!std::isnan(d)) m.bz = (float)d;
    JsonArrayConst h = v["kp"]["history"];
    if (h.size()) {
        int skip = (int)h.size() > KP_HIST ? (int)h.size() - KP_HIST : 0, i = 0;
        m.kpN = 0;
        for (JsonVariantConst e : h) {
            if (i++ < skip) continue;
            double k = num(e["value"], 0);
            m.kpHist[m.kpN++] = (float)k;
        }
    }
    m.revSolar++;
    return true;
}

// ---------- /api/n0nbh ----------
void filterN0nbh(JsonDocument& f) {
    f["solarData"]["aIndex"] = true;
    f["solarData"]["xray"] = true;
    f["solarData"]["solarWind"] = true;
    f["bandConditions"][0]["name"] = true;
    f["bandConditions"][0]["time"] = true;
    f["bandConditions"][0]["condition"] = true;
}
bool applyN0nbh(Model& m, JsonVariantConst v) {
    if (!v["solarData"].is<JsonObjectConst>()) return false;
    double d = num(v["solarData"]["aIndex"]);
    if (!std::isnan(d)) m.aIndex = (int)lround(d);
    d = num(v["solarData"]["solarWind"]);
    if (!std::isnan(d)) m.wind = (int)lround(d);
    cpy(m.xray, v["solarData"]["xray"] | "");
    m.nBands = 0;
    for (JsonVariantConst e : v["bandConditions"].as<JsonArrayConst>()) {
        const char* name = e["name"] | "";
        const char* time = e["time"] | "";
        int i = 0;
        for (; i < m.nBands; i++)
            if (!strcmp(m.bands[i].name, name)) break;
        if (i == m.nBands) {
            if (m.nBands == 4) continue;
            m.nBands++;
            cpy(m.bands[i].name, name);
            m.bands[i].day[0] = m.bands[i].night[0] = 0;
        }
        if (!strcmp(time, "day")) cpy(m.bands[i].day, e["condition"] | "");
        else cpy(m.bands[i].night, e["condition"] | "");
    }
    m.revSolar++;
    return true;
}

// ---------- /api/dxcluster/paths ----------
void filterSpots(JsonDocument& f) {
    for (const char* k : {"spotter", "dxCall", "dxLat", "dxLon", "freq", "comment", "time", "timestamp"}) f[0][k] = true;
}
bool applySpots(Model& m, JsonVariantConst v) {
    if (!v.is<JsonArrayConst>()) return false;
    // los mas nuevos primero: el servidor los da sin orden fijo, asi que se ordenan por 'timestamp' (insercion)
    double ts[MAX_SPOTS];
    m.nSpots = 0;
    for (JsonVariantConst e : v.as<JsonArrayConst>()) {
        const char* dx = e["dxCall"] | "";
        if (!*dx) continue;
        double mhz = num(e["freq"], 0), t = num(e["timestamp"], 0);
        int pos = m.nSpots;
        while (pos > 0 && ts[pos - 1] < t) pos--;
        if (pos >= MAX_SPOTS) continue;
        int last = m.nSpots < MAX_SPOTS ? m.nSpots : MAX_SPOTS - 1;
        for (int i = last; i > pos; i--) { m.spots[i] = m.spots[i - 1]; ts[i] = ts[i - 1]; }
        if (m.nSpots < MAX_SPOTS) m.nSpots++;
        Spot& s = m.spots[pos];
        ts[pos] = t;
        snprintf(s.freq, sizeof(s.freq), "%.3f", mhz);
        cpy(s.dx, dx);
        cpy(s.de, e["spotter"] | "");
        guessMode(s.mode, e["comment"] | "", mhz);
        // "0214z", "02:14" o "0214": solo las cifras
        int n = 0;
        for (const char* p = e["time"] | ""; *p && n < 4; p++)
            if (isdigit((unsigned char)*p)) s.time[n++] = *p;
        s.time[n] = 0;
        double la = num(e["dxLat"]), lo = num(e["dxLon"]);
        s.hasPos = !std::isnan(la) && !std::isnan(lo) && fabs(la) <= 90 && fabs(lo) <= 180;
        s.lat = s.hasPos ? (float)la : 0;
        s.lon = s.hasPos ? (float)lo : 0;
        s.band = bandOf(mhz);
    }
    m.revSpots++;
    return true;
}

// ---------- /api/pota/spots (el JSON de api.pota.app tal cual: frecuencia en kHz) ----------
void filterPota(JsonDocument& f) {
    for (const char* k : {"activator", "reference", "frequency", "mode"}) f[0][k] = true;
}
bool applyPota(Model& m, JsonVariantConst v) {
    if (!v.is<JsonArrayConst>()) return false;
    m.nPota = m.totalPota = 0;
    for (JsonVariantConst e : v.as<JsonArrayConst>()) {
        const char* call = e["activator"] | "";
        if (!*call) continue;
        m.totalPota++;
        if (m.nPota == MAX_ACTS) continue;
        Activation& a = m.pota[m.nPota++];
        cpy(a.call, call);
        cpy(a.ref, e["reference"] | "");
        snprintf(a.freq, sizeof(a.freq), "%.3f", num(e["frequency"], 0) / 1000);
        cpy(a.mode, e["mode"] | "");
        a.sota = false;
    }
    m.revActs++;
    return true;
}
// ---------- /api/sota/spots (api-db2.sota.org.uk: frecuencia en MHz) ----------
void filterSota(JsonDocument& f) {
    for (const char* k : {"activatorCallsign", "summitCode", "frequency", "mode"}) f[0][k] = true;
}
bool applySota(Model& m, JsonVariantConst v) {
    if (!v.is<JsonArrayConst>()) return false;
    m.nSota = m.totalSota = 0;
    for (JsonVariantConst e : v.as<JsonArrayConst>()) {
        const char* call = e["activatorCallsign"] | "";
        if (!*call) continue;
        m.totalSota++;
        if (m.nSota == MAX_ACTS) continue;
        Activation& a = m.sota[m.nSota++];
        cpy(a.call, call);
        cpy(a.ref, e["summitCode"] | "");
        snprintf(a.freq, sizeof(a.freq), "%.3f", num(e["frequency"], 0));
        cpy(a.mode, e["mode"] | "");
        for (char* p = a.mode; *p; p++) *p = (char)toupper((unsigned char)*p);
        a.sota = true;
    }
    m.revActs++;
    return true;
}

// ---------- /api/satellites/data: { timestamp, data: { CLAVE: { name, omm: {...} } } } ----------
void filterSats(JsonDocument& f) {
    JsonObject s = f["data"]["*"].to<JsonObject>();
    s["name"] = true;
    for (const char* k : {"EPOCH", "MEAN_MOTION", "ECCENTRICITY", "INCLINATION", "RA_OF_ASC_NODE", "ARG_OF_PERICENTER", "MEAN_ANOMALY"})
        s["omm"][k] = true;
}
bool applySats(Model& m, JsonVariantConst v) {
    JsonObjectConst data = v["data"];
    if (data.isNull()) return false;
    int n = 0;
    for (JsonPairConst kv : data) {
        if (n == MAX_SATS) break;
        JsonVariantConst o = kv.value()["omm"];
        SatElements s{};
        double mm = num(o["MEAN_MOTION"]);
        if (std::isnan(mm) || mm < 2 || !parseIsoUtc(o["EPOCH"] | "", s.epochJd)) continue;   // solo orbita baja y media
        cpy(s.name, kv.value()["name"] | kv.key().c_str());
        s.n = mm * 2 * K_PI / 1440;
        s.ecc = num(o["ECCENTRICITY"], 0);
        s.inc = num(o["INCLINATION"], 0) * D2R;
        s.raan = num(o["RA_OF_ASC_NODE"], 0) * D2R;
        s.argp = num(o["ARG_OF_PERICENTER"], 0) * D2R;
        s.ma = num(o["MEAN_ANOMALY"], 0) * D2R;
        m.sats[n++] = s;
    }
    if (!n) return false;
    m.nSats = n;
    return true;
}

// ---------- /api/dxnews: { items: [ { title } ] } ----------
void filterNews(JsonDocument& f) { f["items"][0]["title"] = true; }
bool applyNews(Model& m, JsonVariantConst v) {
    JsonArrayConst items = v["items"];
    if (items.isNull()) return false;
    size_t n = 0;
    m.news[0] = 0;
    for (JsonVariantConst e : items) {
        const char* t = e["title"] | "";
        if (!*t) continue;
        size_t need = strlen(t) + 6;
        if (n + need >= sizeof(m.news)) break;
        // la letra de la pantalla llega hasta Latin-1: las rayas y comillas tipograficas se pasan a ASCII
        if (n) n += (size_t)snprintf(m.news + n, sizeof(m.news) - n, "  \xC2\xB7  ");
        for (const unsigned char* p = (const unsigned char*)t; *p;) {
            if (p[0] == 0xE2 && p[1] == 0x80 && p[2]) {
                unsigned char c = p[2];
                m.news[n++] = (c == 0x93 || c == 0x94) ? '-' : (c == 0x98 || c == 0x99) ? '\'' : (c == 0x9C || c == 0x9D) ? '"' : ' ';
                p += 3;
            } else if (*p >= 0xC4) {                       // fuera de Latin-1: se salta el caracter entero
                p++;
                while ((*p & 0xC0) == 0x80) p++;
            } else {
                m.news[n++] = (char)*p++;
            }
        }
        m.news[n] = 0;
    }
    m.revNews++;
    return true;
}

// ---------- geografia ----------
bool gridToLatLon(const char* g, double& lat, double& lon) {
    size_t n = g ? strlen(g) : 0;
    if (n != 4 && n != 6) return false;
    int A = toupper((unsigned char)g[0]) - 'A', B = toupper((unsigned char)g[1]) - 'A';
    if (A < 0 || A > 17 || B < 0 || B > 17 || !isdigit((unsigned char)g[2]) || !isdigit((unsigned char)g[3])) return false;
    lon = -180 + A * 20 + (g[2] - '0') * 2;
    lat = -90 + B * 10 + (g[3] - '0');
    if (n == 6) {
        int a = toupper((unsigned char)g[4]) - 'A', b = toupper((unsigned char)g[5]) - 'A';
        if (a < 0 || a > 23 || b < 0 || b > 23) return false;
        lon += (a + 0.5) * 5.0 / 60;
        lat += (b + 0.5) * 2.5 / 60;
    } else {
        lon += 1;
        lat += 0.5;
    }
    return true;
}
void latLonToGrid(double lat, double lon, char out[5]) {
    lon = fmin(fmax(lon + 180, 0), 359.9999);
    lat = fmin(fmax(lat + 90, 0), 179.9999);
    out[0] = (char)('A' + (int)(lon / 20));
    out[1] = (char)('A' + (int)(lat / 10));
    out[2] = (char)('0' + (int)(fmod(lon, 20) / 2));
    out[3] = (char)('0' + (int)fmod(lat, 10));
    out[4] = 0;
}
double distanceKm(double lat1, double lon1, double lat2, double lon2) {
    double a = pow(sin((lat2 - lat1) * D2R / 2), 2) + cos(lat1 * D2R) * cos(lat2 * D2R) * pow(sin((lon2 - lon1) * D2R / 2), 2);
    return 6371.0 * 2 * asin(fmin(1, sqrt(a)));
}
double bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    double dl = (lon2 - lon1) * D2R;
    double y = sin(dl) * cos(lat2 * D2R);
    double x = cos(lat1 * D2R) * sin(lat2 * D2R) - sin(lat1 * D2R) * cos(lat2 * D2R) * cos(dl);
    return fmod(atan2(y, x) * R2D + 360, 360);
}
void greatCircle(double lat1, double lon1, double lat2, double lon2, double f, double& lat, double& lon) {
    double d = distanceKm(lat1, lon1, lat2, lon2) / 6371.0;
    if (d < 1e-9) { lat = lat1; lon = lon1; return; }
    double A = sin((1 - f) * d) / sin(d), B = sin(f * d) / sin(d);
    double x = A * cos(lat1 * D2R) * cos(lon1 * D2R) + B * cos(lat2 * D2R) * cos(lon2 * D2R);
    double y = A * cos(lat1 * D2R) * sin(lon1 * D2R) + B * cos(lat2 * D2R) * sin(lon2 * D2R);
    double z = A * sin(lat1 * D2R) + B * sin(lat2 * D2R);
    lat = atan2(z, sqrt(x * x + y * y)) * R2D;
    lon = atan2(y, x) * R2D;
}

// ---------- sol (formulas de baja precision del Astronomical Almanac, de sobra para el mapa) ----------
double julianDate(time_t t) { return (double)t / 86400.0 + 2440587.5; }
double gmstRad(double jd) {
    double g = fmod(280.46061837 + 360.98564736629 * (jd - 2451545.0), 360.0);
    if (g < 0) g += 360;
    return g * D2R;
}
static void sunRaDec(double jd, double& ra, double& dec) {
    double n = jd - 2451545.0;
    double L = fmod(280.460 + 0.9856474 * n, 360.0) * D2R, g = fmod(357.528 + 0.9856003 * n, 360.0) * D2R;
    double lam = L + (1.915 * sin(g) + 0.020 * sin(2 * g)) * D2R;
    double eps = (23.439 - 0.0000004 * n) * D2R;
    ra = atan2(cos(eps) * sin(lam), cos(lam));
    dec = asin(sin(eps) * sin(lam));
}
void sunPosition(time_t t, double& decDeg, double& lonDeg) {
    double jd = julianDate(t), ra, dec;
    sunRaDec(jd, ra, dec);
    decDeg = dec * R2D;
    lonDeg = fmod((ra - gmstRad(jd)) * R2D + 540.0 + 360.0, 360.0) - 180.0;
}
bool sunRiseSet(time_t t, double lat, double lon, time_t& rise, time_t& set) {
    // mediodia solar del dia (UTC) de t en esa longitud
    time_t day = t - (t % 86400);
    time_t noon = day + 43200 - (time_t)(lon * 240);
    for (int i = 0; i < 2; i++) {                       // dos vueltas: la ecuacion del tiempo corrige el mediodia
        double dec, slon;
        sunPosition(noon, dec, slon);
        noon += (time_t)((slon - lon) * 240);
    }
    double dec, slon;
    sunPosition(noon, dec, slon);
    double c = (sin(-0.833 * D2R) - sin(lat * D2R) * sin(dec * D2R)) / (cos(lat * D2R) * cos(dec * D2R));
    if (c < -1 || c > 1) return false;
    time_t half = (time_t)(acos(c) * R2D * 240);
    rise = noon - half;
    set = noon + half;
    return true;
}

// ---------- satelites ----------
bool parseIsoUtc(const char* s, double& jd) {
    int Y, M, D, h, m;
    double sec;
    if (!s || sscanf(s, "%d-%d-%dT%d:%d:%lf", &Y, &M, &D, &h, &m, &sec) != 6) return false;
    // dias desde 1970 (algoritmo civil de H. Hinnant)
    int y = Y - (M <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400), doy = (153u * (unsigned)(M + (M > 2 ? -3 : 9)) + 2) / 5 + (unsigned)D - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    double days = (double)era * 146097 + (double)doe - 719468;
    jd = days + 2440587.5 + (h * 3600 + m * 60 + sec) / 86400.0;
    return true;
}

// Propagador de Kepler con las derivas seculares de J2 (nodo, perigeo y anomalia media). Sin rozamiento ni terminos
// periodicos: con elementos de pocos dias el error en la hora de un pase es de segundos, de sobra para la lista.
struct SatConst { double a, n, raanDot, argpDot; };
static SatConst satConst(const SatElements& s) {
    const double KE = 0.0743669161331734, K2 = 5.413080e-4;      // unidades de SGP4 (radios terrestres, minutos)
    double ci = cos(s.inc), b2 = 1 - s.ecc * s.ecc;
    double a1 = pow(KE / s.n, 2.0 / 3.0);
    double d1 = 1.5 * K2 * (3 * ci * ci - 1) / (a1 * a1 * pow(b2, 1.5));
    double a0 = a1 * (1 - d1 / 3 - d1 * d1 - 134.0 / 81.0 * d1 * d1 * d1);
    double d0 = 1.5 * K2 * (3 * ci * ci - 1) / (a0 * a0 * pow(b2, 1.5));
    double n0 = s.n / (1 + d0), a = a0 / (1 - d0);               // movimiento medio y semieje de Brouwer
    double p = a * b2, k = 1.5 * K2 * n0 / (p * p);              // K2 = J2 / 2
    SatConst c;
    c.a = a;
    c.raanDot = -2 * k * ci;
    c.argpDot = k * (5 * ci * ci - 1);
    c.n = n0 * (1 + 1.5 * K2 * (3 * ci * ci - 1) / (a * a * pow(b2, 1.5)));
    return c;
}
static double satElev(const SatElements& s, const SatConst& c, double jd, double sinLat, double cosLat, double lonRad) {
    double dt = (jd - s.epochJd) * 1440.0;
    double M = s.ma + c.n * dt, raan = s.raan + c.raanDot * dt, argp = s.argp + c.argpDot * dt;
    double E = M;
    for (int i = 0; i < 6; i++) E -= (E - s.ecc * sin(E) - M) / (1 - s.ecc * cos(E));
    double xp = c.a * (cos(E) - s.ecc), yp = c.a * sqrt(1 - s.ecc * s.ecc) * sin(E);   // plano de la orbita, radios terrestres
    double cw = cos(argp), sw = sin(argp), cO = cos(raan), sO = sin(raan), ci = cos(s.inc), si = sin(s.inc);
    double x = (cO * cw - sO * sw * ci) * xp + (-cO * sw - sO * cw * ci) * yp;
    double y = (sO * cw + cO * sw * ci) * xp + (-sO * sw + cO * cw * ci) * yp;
    double z = (sw * si) * xp + (cw * si) * yp;
    double th = gmstRad(jd) + lonRad;                             // sidereo local: el observador en el mismo sistema
    double ox = cosLat * cos(th), oy = cosLat * sin(th), oz = sinLat;
    double rx = x - ox, ry = y - oy, rz = z - oz;
    double r = sqrt(rx * rx + ry * ry + rz * rz);
    return asin((rx * ox + ry * oy + rz * oz) / r) * R2D;
}
double satElevation(const SatElements& s, double jd, double lat, double lon) {
    return satElev(s, satConst(s), jd, sin(lat * D2R), cos(lat * D2R), lon * D2R);
}
int findPasses(const SatElements* sats, int n, time_t now, double lat, double lon, Pass* out, int max, double minEl, int hours) {
    int count = 0;
    double sl = sin(lat * D2R), cl = cos(lat * D2R), lo = lon * D2R, jd0 = julianDate(now);
    const int STEP = 30, steps = hours * 3600 / STEP;
    for (int i = 0; i < n; i++) {
        SatConst c = satConst(sats[i]);
        bool up = false;
        double best = 0;
        time_t aos = now;
        for (int k = 0; k <= steps; k++) {
            double el = satElev(sats[i], c, jd0 + k * STEP / 86400.0, sl, cl, lo);
            if (el > 0) {
                if (!up) { up = true; best = el; aos = now + (time_t)k * STEP; }
                else if (el > best) best = el;
                continue;
            }
            if (!up) continue;
            up = false;
            if (best < minEl) continue;
            // se inserta por orden de AOS y se queda con los 'max' primeros
            int pos = count;
            while (pos > 0 && out[pos - 1].aos > aos) pos--;
            if (pos >= max) continue;
            int last = count < max ? count : max - 1;
            for (int j = last; j > pos; j--) out[j] = out[j - 1];
            if (count < max) count++;
            cpy(out[pos].name, sats[i].name);
            out[pos].aos = aos;
            out[pos].maxEl = (float)best;
        }
    }
    return count;
}

}
