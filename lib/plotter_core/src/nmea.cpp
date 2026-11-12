/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "nmea.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "geo.h"

namespace plotter {

uint8_t nmeaChecksum(const char* body) {
    uint8_t c = 0;
    for (const char* p = body; *p && *p != '*'; p++) c ^= uint8_t(*p);
    return c;
}

bool nmeaChecksumOk(const char* line) {
    if (!line || (line[0] != '$' && line[0] != '!')) return false;
    const char* star = std::strchr(line, '*');
    if (!star || !star[1] || !star[2]) return false;
    char hex[3] = {star[1], star[2], 0};
    char* end;
    long v = std::strtol(hex, &end, 16);
    return *end == 0 && uint8_t(v) == nmeaChecksum(line + 1);
}

const char* nmeaSentenceStart(const char* line) {
    if (!line || line[0] != '\\') return line;
    const char* end = std::strchr(line + 1, '\\');
    return end ? end + 1 : line;
}

namespace {

constexpr int kMaxFields = 40;           // las tramas propietarias (P...) llevan muchos campos

struct Fields {
    char buf[100];
    const char* f[kMaxFields];
    int n = 0;
    bool has(int i) const { return i < n && f[i][0] != 0; }
    double num(int i) const { return std::atof(f[i]); }
};

bool split(const char* line, Fields& out) {
    const char* star = std::strchr(line, '*');
    size_t len = star ? size_t(star - line - 1) : std::strlen(line + 1);
    if (len >= sizeof(out.buf)) return false;
    std::memcpy(out.buf, line + 1, len);
    out.buf[len] = 0;
    out.n = 0;
    char* p = out.buf;
    out.f[out.n++] = p;
    for (; *p; p++)
        if (*p == ',') {
            *p = 0;
            if (out.n >= kMaxFields) return false;
            out.f[out.n++] = p + 1;
        }
    return true;
}

// ddmm.mmmm + hemisferio -> grados
bool coord(const Fields& f, int i, double& deg) {
    if (!f.has(i) || !f.has(i + 1)) return false;
    double v = f.num(i);
    int d = int(v / 100);
    deg = d + (v - d * 100) / 60.0;
    char h = f.f[i + 1][0];
    if (h == 'S' || h == 'W') deg = -deg;
    return true;
}

}  // namespace

NmeaResult Nmea0183::feed(const char* line, Source src, uint32_t now, DataStore& st) {
    tagTime_ = 0;
    tagSource_[0] = 0;
    if (!line) { stats_.malformed++; return NmeaResult::Bad; }
    const char* body = nmeaSentenceStart(line);
    if (body != line) {                                           // bloque de etiquetas: campos "clave:valor" separados por comas
        for (const char* p = line + 1; p < body - 1 && *p != '*';) {
            const char* e = p;
            while (e < body - 1 && *e != ',' && *e != '*') e++;
            if (e - p > 2 && p[1] == ':') {
                if (p[0] == 'c') {
                    unsigned long long t = std::strtoull(p + 2, nullptr, 10);
                    if (t > 99999999999ULL) t /= 1000;            // algunos equipos la dan en milisegundos
                    tagTime_ = uint32_t(t);
                } else if (p[0] == 's') {
                    size_t n = size_t(e - p - 2);
                    if (n >= sizeof(tagSource_)) n = sizeof(tagSource_) - 1;
                    std::memcpy(tagSource_, p + 2, n);
                    tagSource_[n] = 0;
                }
            }
            p = (e < body - 1 && *e == ',') ? e + 1 : body;
        }
        line = body;
    }
    if (line[0] == '{') return NmeaResult::Ignored;               // linea JSON de gpsd (VERSION, DEVICES, WATCH)
    if (!nmeaChecksumOk(line)) { stats_.badChecksum++; return NmeaResult::Bad; }
    Fields f;
    if (!split(line, f) || std::strlen(f.f[0]) < 5) { stats_.malformed++; return NmeaResult::Bad; }
    stats_.good++;
    const char* id = f.f[0] + std::strlen(f.f[0]) - 3;            // sin el emisor (GP, II, AI...)

    if (!std::strcmp(id, "VDM") || !std::strcmp(id, "VDO")) {
        if (f.n < 7) { stats_.malformed++; return NmeaResult::Bad; }
        int seq = f.has(3) ? std::atoi(f.f[3]) : -1;
        if (dec_.feed(std::atoi(f.f[1]), std::atoi(f.f[2]), seq, f.f[5], std::atoi(f.f[6]), id[2] == 'O', ais_)) {
            if (ais_.oversized) stats_.aisOversized++;
            return NmeaResult::Ais;
        }
        if (dec_.lastError() == AisError::Unsupported) stats_.aisUnsupported++;
        else if (dec_.lastError() != AisError::Pending) stats_.aisRejected++;
        return NmeaResult::Ignored;
    }
    if (!std::strcmp(id, "GSV")) { sky_.feedGsv(f.f, f.n, now); return NmeaResult::Data; }
    if (!std::strcmp(id, "GSA")) { sky_.feedGsa(f.f, f.n, now); return NmeaResult::Data; }
    if (!std::strcmp(id, "GGA")) {
        if (f.n < 10 || !f.has(6) || f.f[6][0] == '0') return NmeaResult::Ignored;        // calidad 0 = sin posicion
        double la, lo;
        if (coord(f, 2, la) && coord(f, 4, lo)) { st.set(Key::Lat, la, src, now); st.set(Key::Lon, lo, src, now); }
        if (f.has(7)) st.set(Key::SatsUsed, f.num(7), src, now);
        if (f.has(8)) st.set(Key::Hdop, f.num(8), src, now);
        if (f.has(9)) st.set(Key::Altitude, f.num(9), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "RMC")) {
        if (f.n < 9 || !f.has(2) || f.f[2][0] != 'A') return NmeaResult::Ignored;         // sin posicion valida
        double la, lo;
        if (coord(f, 3, la) && coord(f, 5, lo)) { st.set(Key::Lat, la, src, now); st.set(Key::Lon, lo, src, now); }
        if (f.has(7)) st.set(Key::Sog, f.num(7), src, now);
        if (f.has(8)) st.set(Key::Cog, f.num(8), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "GLL")) {
        if (f.n < 7 || !f.has(6) || f.f[6][0] != 'A') return NmeaResult::Ignored;
        double la, lo;
        if (!coord(f, 1, la) || !coord(f, 3, lo)) return NmeaResult::Ignored;
        st.set(Key::Lat, la, src, now); st.set(Key::Lon, lo, src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "VTG")) {
        if (f.n < 6) return NmeaResult::Ignored;
        if (f.has(1)) st.set(Key::Cog, f.num(1), src, now);
        if (f.has(5)) st.set(Key::Sog, f.num(5), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "DPT")) {                                                        // profundidad bajo el transductor + desfase
        if (!f.has(1)) return NmeaResult::Ignored;
        st.set(Key::Depth, f.num(1) + (f.has(2) ? f.num(2) : 0), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "DBT")) {
        if (f.n < 4 || !f.has(3)) return NmeaResult::Ignored;
        st.set(Key::Depth, f.num(3), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "MWV")) {
        if (f.n < 6 || !f.has(5) || f.f[5][0] != 'A' || !f.has(1) || !f.has(3) || !f.has(4)) return NmeaResult::Ignored;
        if (f.f[2][0] != 'R') return NmeaResult::Ignored;                                 // el real lo calcula el plotter
        double kn = f.num(3);
        char u = f.f[4][0];
        if (u == 'M') kn *= 3600.0 / 1852.0;
        else if (u == 'K') kn /= 1.852;
        st.set(Key::Awa, norm180(f.num(1)), src, now);
        st.set(Key::Aws, kn, src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "HDT")) {
        if (!f.has(1)) return NmeaResult::Ignored;
        st.set(Key::Heading, f.num(1), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "VHW")) {
        if (f.n < 6) return NmeaResult::Ignored;
        if (f.has(1)) st.set(Key::Heading, f.num(1), src, now);
        if (f.has(5)) st.set(Key::Stw, f.num(5), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "MTW")) {
        if (!f.has(1)) return NmeaResult::Ignored;
        st.set(Key::WaterTemp, f.num(1), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "HDM") || !std::strcmp(id, "HDG")) {      // rumbo magnetico: el verdadero sale al aplicar desvio y declinacion
        if (!f.has(1)) return NmeaResult::Ignored;
        st.set(Key::HeadingMag, norm360(f.num(1)), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "RSA")) {                                 // angulo del timon (estribor positivo)
        if (f.n < 3 || !f.has(1) || f.f[2][0] != 'A') return NmeaResult::Ignored;
        st.set(Key::RudderAngle, f.num(1), src, now);
        return NmeaResult::Data;
    }
    if (!std::strcmp(id, "XDR")) {                                 // grupos de tipo, valor, unidad y nombre: cabeceo y escora
        bool any = false;
        for (int i = 1; i + 3 < f.n; i += 4) {
            if (f.f[i][0] != 'A' || !f.has(i + 1)) continue;
            const char* name = f.f[i + 3];
            if (!std::strcmp(name, "PTCH") || !std::strcmp(name, "PITCH")) { st.set(Key::Pitch, f.num(i + 1), src, now); any = true; }
            else if (!std::strcmp(name, "ROLL") || !std::strcmp(name, "HEEL")) { st.set(Key::Roll, f.num(i + 1), src, now); any = true; }
        }
        return any ? NmeaResult::Data : NmeaResult::Ignored;
    }
    return NmeaResult::Ignored;
}

int nmeaApb(char* out, int size, double xteNm, bool arrived, bool perp, double brgOD, const char* wpt, double brgPD, double hts,
            bool magnetic) {
    char body[96];
    char m = magnetic ? 'M' : 'T';
    double x = xteNm < 0 ? -xteNm : xteNm;
    if (x > 9.99) x = 9.99;
    char id[10];
    int k = 0;
    for (const char* p = wpt ? wpt : ""; *p && k < 9; p++)
        if (*p != ',' && *p != '*' && *p >= 32 && *p < 127) id[k++] = *p;      // sin separadores dentro del nombre
    id[k] = 0;
    int n = std::snprintf(body, sizeof(body), "ECAPB,A,A,%.2f,%c,N,%c,%c,%.1f,%c,%s,%.1f,%c,%.1f,%c", x, xteNm > 0 ? 'L' : 'R',
                          arrived ? 'A' : 'V', perp ? 'A' : 'V', norm360(brgOD), m, id, norm360(brgPD), m, norm360(hts), m);
    if (n <= 0 || n >= int(sizeof(body))) return 0;
    int w = std::snprintf(out, size_t(size), "$%s*%02X\r\n", body, nmeaChecksum(body));
    return (w > 0 && w < size) ? w : 0;
}

static void cleanId(const char* in, char* out, int size) {
    int k = 0;
    for (const char* p = in ? in : ""; *p && k < size - 1; p++)
        if (*p != ',' && *p != '*' && *p >= 32 && *p < 127) out[k++] = *p;
    out[k] = 0;
}

int nmeaRmb(char* out, int size, double xteNm, const char* originId, const char* destId, double destLat, double destLon,
            double rangeNm, double bearingTrue, double closingKn, bool arrived) {
    char body[128], o[10], d[10];
    cleanId(originId, o, sizeof(o));
    cleanId(destId, d, sizeof(d));
    double x = xteNm < 0 ? -xteNm : xteNm;
    if (x > 9.99) x = 9.99;
    if (rangeNm > 999.9) rangeNm = 999.9;
    if (rangeNm < 0) rangeNm = 0;
    double la = destLat < 0 ? -destLat : destLat, lo = destLon < 0 ? -destLon : destLon;
    int laD = int(la), loD = int(lo);
    double laM = (la - laD) * 60.0, loM = (lo - loD) * 60.0;
    if (laM >= 59.99995) { laM = 0; laD++; }             // que el redondeo no escriba 60.0000 minutos
    if (loM >= 59.99995) { loM = 0; loD++; }
    int n = std::snprintf(body, sizeof(body), "ECRMB,A,%.2f,%c,%s,%s,%02d%07.4f,%c,%03d%07.4f,%c,%.1f,%.1f,%.1f,%c,A", x, xteNm > 0 ? 'L' : 'R', o,
                          d, laD, laM, destLat < 0 ? 'S' : 'N', loD, loM, destLon < 0 ? 'W' : 'E', rangeNm, norm360(bearingTrue), closingKn,
                          arrived ? 'A' : 'V');
    if (n <= 0 || n >= int(sizeof(body))) return 0;
    int w = std::snprintf(out, size_t(size), "$%s*%02X\r\n", body, nmeaChecksum(body));
    return (w > 0 && w < size) ? w : 0;
}

}  // namespace plotter
