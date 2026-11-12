/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "aistargets.h"

#include <cstdio>
#include <cstring>

namespace plotter {

int AisTargets::find(uint32_t mmsi) const {
    for (int i = 0; i < n_; i++)
        if (t_[i].mmsi == mmsi) return i;
    return -1;
}

int AisTargets::update(const AisReport& r, uint32_t now) {
    if (r.own || r.mmsi == 0 || r.kind == AisKind::Safety) return -1;
    int i = find(r.mmsi);
    if (i < 0) {
        if (n_ >= kMax) {                                       // llena: se sustituye el que lleva mas tiempo callado
            int old = 0;
            for (int k = 1; k < n_; k++)
                if (uint32_t(now - t_[k].lastMs) > uint32_t(now - t_[old].lastMs)) old = k;
            if (uint32_t(now - t_[old].lastMs) < 60000u) return -1;
            i = old;
        } else {
            i = n_++;
        }
        t_[i] = AisTarget();
        t_[i].mmsi = r.mmsi;
    }
    AisTarget& t = t_[i];
    t.lastMs = now;
    t.kind = r.kind;
    if (r.type == 18 || r.type == 19 || r.type == 24) t.classB = true;
    if (r.name[0]) std::snprintf(t.name, sizeof(t.name), "%s", r.name);
    if (r.callsign[0]) std::snprintf(t.callsign, sizeof(t.callsign), "%s", r.callsign);
    if (r.shipType >= 0) t.shipType = r.shipType;
    if (r.navStatus >= 0) t.navStatus = r.navStatus;
    if (r.atonType >= 0) t.atonType = r.atonType;
    if (r.hasDim) { t.lengthM = r.toBow + r.toStern; t.beamM = r.toPort + r.toStarboard; }
    if (r.hasSog) { t.sogKn = r.sogKn; t.hasSog = true; }
    if (r.hasCog) { t.cogDeg = r.cogDeg; t.hasCog = true; }
    if (r.hasHeading) { t.heading = r.heading; t.hasHeading = true; }
    if (r.hasPos) {
        bool aircraft = r.kind == AisKind::SarAircraft;
        long secs = long(uint32_t(now - t.posMs) / 1000u);
        bool stopped = t.hasSog && t.sogKn < 0.3;
        AisPosVerdict v = aisJudgePosition(t.hasPos, t.lat, t.lon, stopped, secs, r.lat, r.lon, aircraft);
        bool accept = v != AisPosVerdict::Jump;
        if (!accept) {
            // Un salto se acepta si la posicion siguiente lo confirma (el barco de verdad esta alli)
            if (t.pending && aisJudgePosition(true, t.pendLat, t.pendLon, false, long(uint32_t(now - t.pendMs) / 1000u), r.lat, r.lon,
                                              aircraft) == AisPosVerdict::Valid)
                accept = true;
            else {
                t.pending = true; t.pendLat = r.lat; t.pendLon = r.lon; t.pendMs = now;
                jumps_++;
            }
        }
        if (accept) {
            t.lat = r.lat; t.lon = r.lon; t.hasPos = true; t.posMs = now; t.pending = false; t.lost = false;
        }
    }
    return i;
}

void AisTargets::compute(bool hasOwn, const Mover& own, uint32_t now) {
    int w = 0;
    for (int i = 0; i < n_; i++) {
        AisTarget& t = t_[i];
        bool fixed = t.kind == AisKind::Aton || t.kind == AisKind::BaseStation;
        uint32_t quiet = uint32_t(now - t.lastMs);
        if (quiet > (fixed ? removeMs_ * 3 : removeMs_)) continue;      // caducado: fuera
        t.lost = !fixed && t.hasPos && uint32_t(now - t.posMs) > lostMs_;
        t.hasRange = t.hasCpa = t.dangerous = false;
        if (hasOwn && t.hasPos) {
            t.rangeNm = gcDistanceNm(own.pos, {t.lat, t.lon});
            t.bearing = gcInitialBearing(own.pos, {t.lat, t.lon});
            t.hasRange = true;
            if (!fixed && !t.lost) {
                Mover m{{t.lat, t.lon}, t.hasSog ? t.sogKn : 0.0, t.hasCog ? t.cogDeg : 0.0};
                cpa(own, m, t.cpaNm, t.tcpaMin);
                t.hasCpa = true;
                t.dangerous = t.cpaNm <= cpaLimit_ && t.tcpaMin >= 0 && t.tcpaMin <= tcpaLimit_ && (own.sogKn > 0.3 || m.sogKn > 0.3);
            }
        }
        if (w != i) t_[w] = t;
        w++;
    }
    n_ = w;
}

bool AisTargets::silence(uint32_t mmsi, bool on) {
    int i = find(mmsi);
    if (i < 0) return false;
    t_[i].silenced = on;
    return true;
}

int AisTargets::dangerousCount() const {
    int c = 0;
    for (int i = 0; i < n_; i++)
        if (t_[i].dangerous && !t_[i].silenced) c++;
    return c;
}

int AisTargets::sorted(int* idx, int maxN) const {
    int n = n_ < maxN ? n_ : maxN, total = 0;
    auto before = [&](int a, int b) {
        const AisTarget& x = t_[a]; const AisTarget& y = t_[b];
        if (x.dangerous != y.dangerous) return x.dangerous;
        if (x.dangerous) return x.tcpaMin < y.tcpaMin;
        if (x.hasRange != y.hasRange) return x.hasRange;
        if (x.hasRange) return x.rangeNm < y.rangeNm;
        return x.mmsi < y.mmsi;
    };
    for (int i = 0; i < n_; i++) {                              // insercion: como mucho 100 blancos
        int pos = total < n ? total : n;
        while (pos > 0 && before(i, idx[pos - 1])) pos--;
        if (pos >= n) continue;
        int last = total < n ? total : n - 1;
        for (int k = last; k > pos; k--) idx[k] = idx[k - 1];
        idx[pos] = i;
        if (total < n) total++;
    }
    return total;
}

const char* aisShipTypeNameEs(int t) {
    if (t == 30) return "Pesquero";
    if (t == 31 || t == 32 || t == 52) return "Remolcador";
    if (t == 33) return "Draga";
    if (t == 35) return "Militar";
    if (t == 36) return "Velero";
    if (t == 37) return "Recreo";
    if (t == 50) return "Pr\xC3\xA1" "ctico";
    if (t == 51) return "Salvamento";
    if (t == 55) return "Polic\xC3\xAD" "a";
    if (t >= 40 && t <= 49) return "Alta velocidad";
    if (t >= 60 && t <= 69) return "Pasaje";
    if (t >= 70 && t <= 79) return "Carguero";
    if (t >= 80 && t <= 89) return "Petrolero";
    return "Buque";
}

}  // namespace plotter
