/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "n2k.h"
#include "timesync.h"

#include <cmath>
#include <cstring>

#include "geo.h"

namespace plotter {

namespace {

const double kKnPerMs = 3600.0 / 1852.0;

uint16_t u16(const uint8_t* d, int at) { return (uint16_t)(d[at] | (d[at + 1] << 8)); }
uint32_t u32(const uint8_t* d, int at) { return (uint32_t)d[at] | ((uint32_t)d[at + 1] << 8) | ((uint32_t)d[at + 2] << 16) | ((uint32_t)d[at + 3] << 24); }
int64_t i64(const uint8_t* d, int at) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | d[at + i];
    return (int64_t)v;
}
// Los valores mas altos de cada tamano significan "sin dato" (y el anterior, "fuera de rango")
bool okU16(uint16_t v) { return v < 0xFFFE; }
bool okI16(uint16_t v) { return v != 0x7FFF && v != 0x7FFE; }
bool okU32(uint32_t v) { return v < 0xFFFFFFFEu; }
bool okI32(uint32_t v) { return v != 0x7FFFFFFFu && v != 0x7FFFFFFEu; }
double rad16(uint16_t v) { return rad2deg(v * 0.0001); }
double kelvin(uint16_t v) { return v * 0.01 - 273.15; }

// Texto de longitud fija: se corta en el primer 0x00, 0xFF o '@' y se quitan los espacios del final
void fixedText(const uint8_t* d, int n, char* out, int outSize) {
    int k = 0;
    for (int i = 0; i < n && k < outSize - 1; i++) {
        uint8_t c = d[i];
        if (c == 0 || c == 0xFF || c == '@') break;
        out[k++] = (c >= 32 && c < 127) ? (char)c : ' ';
    }
    while (k > 0 && out[k - 1] == ' ') k--;
    out[k] = 0;
}

bool aisPosition(const uint8_t* d, int len, bool classA, AisReport& r) {
    if (len < 21) return false;
    r = AisReport();
    r.type = d[0] & 0x3F;
    r.mmsi = u32(d, 1);
    if (r.mmsi == 0 || r.mmsi > 999999999u) return false;
    uint32_t lon = u32(d, 5), lat = u32(d, 9);
    if (okI32(lon) && okI32(lat)) {
        r.lon = (int32_t)lon * 1e-7;
        r.lat = (int32_t)lat * 1e-7;
        r.hasPos = std::fabs(r.lat) <= 90 && std::fabs(r.lon) <= 180;
    }
    uint16_t cog = u16(d, 14), sog = u16(d, 16);
    if (okU16(cog)) { r.hasCog = true; r.cogDeg = rad16(cog); }
    if (okU16(sog)) { r.hasSog = true; r.sogKn = sog * 0.01 * kKnPerMs; }
    if (len >= 23) {
        uint16_t hdg = u16(d, 21);
        if (okU16(hdg)) { r.hasHeading = true; r.heading = (int)std::lround(rad16(hdg)) % 360; }
    }
    if (classA && len >= 26) r.navStatus = d[25] & 0x0F;
    return true;
}

void aisDims(double lengthM, double beamM, double fromStarboardM, double fromBowM, AisReport& r) {
    r.hasDim = true;
    r.toBow = (int)std::lround(fromBowM);
    r.toStern = (int)std::lround(lengthM - fromBowM);
    r.toStarboard = (int)std::lround(fromStarboardM);
    r.toPort = (int)std::lround(beamM - fromStarboardM);
    if (r.toStern < 0) r.toStern = 0;
    if (r.toPort < 0) r.toPort = 0;
}

}  // namespace

N2kId n2kParseId(uint32_t canId) {
    N2kId id;
    id.prio = (canId >> 26) & 7;
    id.src = canId & 0xFF;
    uint32_t dp = (canId >> 24) & 3, pf = (canId >> 16) & 0xFF, ps = (canId >> 8) & 0xFF;
    if (pf < 240) { id.pgn = (dp << 16) | (pf << 8); id.dst = (uint8_t)ps; }      // dirigido: el byte bajo es el destinatario
    else { id.pgn = (dp << 16) | (pf << 8) | ps; id.dst = 255; }
    return id;
}

bool n2kIsFastPacket(uint32_t pgn) {
    switch (pgn) {
        case 129029: case 129038: case 129039: case 129040: case 129041: case 129794: case 129809: case 129810: return true;
        default: return false;
    }
}

bool N2kFastPacket::feed(uint32_t pgn, uint8_t src, const uint8_t* d, int len, uint32_t nowMs) {
    if (len < 2) return false;
    uint8_t seq = d[0] >> 5, frame = d[0] & 0x1F;
    Slot* s = nullptr;
    for (Slot& k : s_)
        if (k.used && k.pgn == pgn && k.src == src) { s = &k; break; }
    if (frame == 0) {
        int total = d[1];
        if (s && s->have < s->total) dropped_++;          // empieza otro sin acabar el anterior
        if (!s) {
            for (Slot& k : s_)
                if (!k.used) { s = &k; break; }
            if (!s) {                                     // todos ocupados: se pisa el mas viejo
                s = &s_[0];
                for (Slot& k : s_)
                    if ((uint32_t)(nowMs - k.t) > (uint32_t)(nowMs - s->t)) s = &k;
                dropped_++;
            }
        }
        if (total < 1 || total > kMaxLen) { s->used = false; return false; }
        s->used = true; s->pgn = pgn; s->src = src; s->seq = seq; s->next = 1; s->total = total; s->t = nowMs;
        int n = len - 2;
        if (n > total) n = total;
        std::memcpy(s->buf, d + 2, (size_t)n);
        s->have = n;
    } else {
        if (!s || s->seq != seq || s->next != frame || (uint32_t)(nowMs - s->t) > 750) {   // trama suelta, perdida o tardia
            if (s) { s->used = false; dropped_++; }
            return false;
        }
        int n = len - 1;
        if (n > s->total - s->have) n = s->total - s->have;
        std::memcpy(s->buf + s->have, d + 1, (size_t)n);
        s->have += n;
        s->next++;
        s->t = nowMs;
    }
    if (s->have < s->total) return false;
    std::memcpy(done_, s->buf, (size_t)s->total);
    doneLen_ = s->total;
    s->used = false;
    return true;
}

N2kResult N2kReader::feedFrame(uint32_t canId, const uint8_t* d, int len, Source src, uint32_t nowMs, DataStore& store) {
    stats_.frames++;
    if (len < 1 || len > 8) return N2kResult::None;
    N2kId id = n2kParseId(canId);
    if (n2kIsFastPacket(id.pgn)) {
        if (!fast_.feed(id.pgn, id.src, d, len, nowMs)) return N2kResult::Pending;
        return feedMessage(id.pgn, fast_.data(), fast_.length(), src, nowMs, store);
    }
    return feedMessage(id.pgn, d, len, src, nowMs, store);
}

N2kResult N2kReader::feedMessage(uint32_t pgn, const uint8_t* d, int len, Source src, uint32_t now, DataStore& st) {
    stats_.messages++;
    N2kResult res = N2kResult::Data;
    switch (pgn) {
        case 127245:                                       // timon
            if (len < 6 || !okI16(u16(d, 4))) return N2kResult::None;
            st.set(Key::RudderAngle, rad2deg((int16_t)u16(d, 4) * 0.0001), src, now);
            break;
        case 127250: {                                     // rumbo
            if (len < 8 || !okU16(u16(d, 1))) return N2kResult::None;
            double h = rad16(u16(d, 1));
            int ref = d[7] & 3;
            if (ref == 0) st.set(Key::Heading, norm360(h), src, now);
            else if (ref == 1) {
                st.set(Key::HeadingMag, norm360(h), src, now);
                if (okI16(u16(d, 5))) st.set(Key::Heading, norm360(h + rad2deg((int16_t)u16(d, 5) * 0.0001)), src, now);   // con declinacion
            } else return N2kResult::None;
            break;
        }
        case 127488:                                       // motor (solo el primero)
            if (len < 3 || d[0] != 0 || !okU16(u16(d, 1))) return N2kResult::None;
            st.set(Key::Rpm, u16(d, 1) * 0.25, src, now);
            break;
        case 127508:                                       // bateria (solo la primera)
            if (len < 3 || d[0] != 0 || !okI16(u16(d, 1))) return N2kResult::None;
            st.set(Key::BatteryV, (int16_t)u16(d, 1) * 0.01, src, now);
            break;
        case 128259:                                       // corredera
            if (len < 3 || !okU16(u16(d, 1))) return N2kResult::None;
            st.set(Key::Stw, u16(d, 1) * 0.01 * kKnPerMs, src, now);
            break;
        case 128267: {                                     // sonda: bajo el transductor mas el desfase, como DPT
            if (len < 5 || !okU32(u32(d, 1))) return N2kResult::None;
            double depth = u32(d, 1) * 0.01;
            if (len >= 7 && okI16(u16(d, 5))) depth += (int16_t)u16(d, 5) * 0.001;
            st.set(Key::Depth, depth, src, now);
            break;
        }
        case 129025: {                                     // posicion
            if (len < 8 || !okI32(u32(d, 0)) || !okI32(u32(d, 4))) return N2kResult::None;
            double la = (int32_t)u32(d, 0) * 1e-7, lo = (int32_t)u32(d, 4) * 1e-7;
            if (std::fabs(la) > 90 || std::fabs(lo) > 180) return N2kResult::None;
            st.set(Key::Lat, la, src, now);
            st.set(Key::Lon, lo, src, now);
            break;
        }
        case 129026: {                                     // rumbo y velocidad sobre el fondo
            if (len < 6) return N2kResult::None;
            bool any = false;
            if ((d[1] & 3) == 0 && okU16(u16(d, 2))) { st.set(Key::Cog, norm360(rad16(u16(d, 2))), src, now); any = true; }
            if (okU16(u16(d, 4))) { st.set(Key::Sog, u16(d, 4) * 0.01 * kKnPerMs, src, now); any = true; }
            if (!any) return N2kResult::None;
            break;
        }
        case 126992: {                                     // hora del sistema (fuente en los 4 bits bajos de d[1]: 0 = GPS)
            if (len < 8) return N2kResult::None;
            const int64_t t = n2kUtc(u16(d, 2), u32(d, 4));
            if (t < 0) return N2kResult::None;
            utc_ = t; utcNew_ = true;
            break;
        }
        case 129029: {                                     // posicion GNSS completa
            if (len < 31) return N2kResult::None;
            { const int64_t t = n2kUtc(u16(d, 1), u32(d, 3)); if (t >= 0) { utc_ = t; utcNew_ = true; } }
            int64_t la = i64(d, 7), lo = i64(d, 15), al = i64(d, 23);
            if (la == INT64_MAX || lo == INT64_MAX) return N2kResult::None;
            double lat = (double)la * 1e-16, lon = (double)lo * 1e-16;
            if (std::fabs(lat) > 90 || std::fabs(lon) > 180) return N2kResult::None;
            st.set(Key::Lat, lat, src, now);
            st.set(Key::Lon, lon, src, now);
            if (al != INT64_MAX) st.set(Key::Altitude, (double)al * 1e-6, src, now);
            if (len >= 34 && d[33] != 0xFF) st.set(Key::SatsUsed, d[33], src, now);
            if (len >= 36 && okI16(u16(d, 34))) st.set(Key::Hdop, (int16_t)u16(d, 34) * 0.01, src, now);
            break;
        }
        case 130306: {                                     // viento
            if (len < 6 || !okU16(u16(d, 1))) return N2kResult::None;
            double kn = u16(d, 1) * 0.01 * kKnPerMs;
            bool hasAngle = okU16(u16(d, 3));
            double ang = hasAngle ? rad16(u16(d, 3)) : 0;
            int ref = d[5] & 7;
            if (ref == 2) {                                // aparente: angulo desde la proa, negativo por babor
                st.set(Key::Aws, kn, src, now);
                if (hasAngle) st.set(Key::Awa, norm180(ang), src, now);
            } else if (ref == 0) {                         // real, direccion respecto al norte verdadero
                st.set(Key::Tws, kn, src, now);
                if (hasAngle) st.set(Key::Twd, norm360(ang), src, now);
            } else if (ref == 3 || ref == 4) st.set(Key::Tws, kn, src, now);   // real respecto a la proa: solo la velocidad
            else return N2kResult::None;
            break;
        }
        case 130310: {                                     // agua, aire y presion (formato antiguo)
            if (len < 7) return N2kResult::None;
            bool any = false;
            if (okU16(u16(d, 1))) { st.set(Key::WaterTemp, kelvin(u16(d, 1)), src, now); any = true; }
            if (okU16(u16(d, 3))) { st.set(Key::AirTemp, kelvin(u16(d, 3)), src, now); any = true; }
            if (okU16(u16(d, 5))) { st.set(Key::Pressure, u16(d, 5) * 1.0, src, now); any = true; }      // unidades de 100 Pa = hPa
            if (!any) return N2kResult::None;
            break;
        }
        case 130311: {                                     // temperatura, humedad y presion
            if (len < 8) return N2kResult::None;
            bool any = false;
            int tsrc = d[1] & 0x3F, hsrc = d[1] >> 6;
            if (okU16(u16(d, 2)) && tsrc <= 1) { st.set(tsrc == 0 ? Key::WaterTemp : Key::AirTemp, kelvin(u16(d, 2)), src, now); any = true; }
            if (okI16(u16(d, 4)) && hsrc == 1) { st.set(Key::Humidity, (int16_t)u16(d, 4) * 0.004, src, now); any = true; }
            if (okU16(u16(d, 6))) { st.set(Key::Pressure, u16(d, 6) * 1.0, src, now); any = true; }
            if (!any) return N2kResult::None;
            break;
        }
        case 130312:                                       // temperatura: 0 agua de mar, 1 aire exterior
            if (len < 5 || d[2] > 1 || !okU16(u16(d, 3))) return N2kResult::None;
            st.set(d[2] == 0 ? Key::WaterTemp : Key::AirTemp, kelvin(u16(d, 3)), src, now);
            break;
        case 130314:                                       // presion: 0 atmosferica
            if (len < 7 || d[2] != 0 || !okI32(u32(d, 3))) return N2kResult::None;
            st.set(Key::Pressure, (int32_t)u32(d, 3) * 0.1 / 100.0, src, now);
            break;
        case 129038:                                       // AIS clase A, posicion
        case 129039:                                       // AIS clase B, posicion
            if (!aisPosition(d, len, pgn == 129038, ais_)) return N2kResult::None;
            res = N2kResult::Ais;
            break;
        case 129794: {                                     // AIS clase A, datos del buque y del viaje
            if (len < 54) return N2kResult::None;
            ais_ = AisReport();
            ais_.type = 5;
            ais_.mmsi = u32(d, 1);
            if (ais_.mmsi == 0 || ais_.mmsi > 999999999u) return N2kResult::None;
            ais_.hasStatic = true;
            fixedText(d + 9, 7, ais_.callsign, sizeof(ais_.callsign));
            fixedText(d + 16, 20, ais_.name, sizeof(ais_.name));
            if (d[36] != 0xFF) ais_.shipType = d[36];
            if (okU16(u16(d, 37)) && okU16(u16(d, 39)) && okU16(u16(d, 41)) && okU16(u16(d, 43)))
                aisDims(u16(d, 37) * 0.1, u16(d, 39) * 0.1, u16(d, 41) * 0.1, u16(d, 43) * 0.1, ais_);
            ais_.hasVoyage = true;
            if (okU16(u16(d, 51))) ais_.draughtM = u16(d, 51) * 0.01;
            if (len >= 73) fixedText(d + 53, 20, ais_.destination, sizeof(ais_.destination));
            res = N2kResult::Ais;
            break;
        }
        case 129809:                                       // AIS clase B, nombre
            if (len < 25) return N2kResult::None;
            ais_ = AisReport();
            ais_.type = 24;
            ais_.mmsi = u32(d, 1);
            if (ais_.mmsi == 0 || ais_.mmsi > 999999999u) return N2kResult::None;
            ais_.hasStatic = true;
            fixedText(d + 5, 20, ais_.name, sizeof(ais_.name));
            res = N2kResult::Ais;
            break;
        case 129810:                                       // AIS clase B, tipo, distintivo y dimensiones
            if (len < 28) return N2kResult::None;
            ais_ = AisReport();
            ais_.type = 24;
            ais_.mmsi = u32(d, 1);
            if (ais_.mmsi == 0 || ais_.mmsi > 999999999u) return N2kResult::None;
            ais_.hasStatic = true;
            if (d[5] != 0xFF) ais_.shipType = d[5];
            fixedText(d + 13, 7, ais_.callsign, sizeof(ais_.callsign));
            if (okU16(u16(d, 20)) && okU16(u16(d, 22)) && okU16(u16(d, 24)) && okU16(u16(d, 26)))
                aisDims(u16(d, 20) * 0.1, u16(d, 22) * 0.1, u16(d, 24) * 0.1, u16(d, 26) * 0.1, ais_);
            res = N2kResult::Ais;
            break;
        default:
            return N2kResult::Unknown;
    }
    stats_.known++;
    if (res == N2kResult::Ais) stats_.ais++;
    return res;
}

}  // namespace plotter
