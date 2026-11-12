/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Tabla de blancos AIS: junta los mensajes de cada MMSI, descarta posiciones imposibles, calcula distancia, demora y
// punto de maxima aproximacion (CPA) respecto al barco propio, marca los peligrosos y los perdidos.
#ifndef PLOTTER_AISTARGETS_H_
#define PLOTTER_AISTARGETS_H_

#include <cstdint>

#include "ais.h"
#include "motion.h"

namespace plotter {

struct AisTarget {
    uint32_t mmsi = 0;
    AisKind kind = AisKind::Vessel;
    bool classB = false;
    char name[35] = "";
    char callsign[8] = "";
    int shipType = -1, navStatus = -1, atonType = -1;
    bool hasPos = false;
    double lat = 0, lon = 0;
    bool hasSog = false, hasCog = false, hasHeading = false;
    double sogKn = 0, cogDeg = 0;
    int heading = 0;
    int lengthM = 0, beamM = 0;
    uint32_t lastMs = 0;            // ultimo mensaje de cualquier tipo
    uint32_t posMs = 0;             // ultima posicion aceptada
    // Respecto al barco propio (compute())
    bool hasRange = false;
    double rangeNm = 0, bearing = 0;
    bool hasCpa = false;
    double cpaNm = 0, tcpaMin = 0;
    bool dangerous = false;         // CPA y TCPA por debajo de los limites
    bool lost = false;              // sin posicion nueva desde hace mas de lo normal
    bool silenced = false;          // alarma callada para este buque
    // Interno: posicion pendiente de confirmar tras un salto
    bool pending = false;
    double pendLat = 0, pendLon = 0;
    uint32_t pendMs = 0;
};

class AisTargets {
   public:
    static constexpr int kMax = 100;
    void setLimits(double cpaNm, double tcpaMin) { cpaLimit_ = cpaNm; tcpaLimit_ = tcpaMin; }
    void setTimeouts(uint32_t lostMs, uint32_t removeMs) { lostMs_ = lostMs; removeMs_ = removeMs; }
    // Mete un mensaje. Devuelve el indice del blanco, -1 si no se guarda (barco propio, mensaje de seguridad, tabla llena).
    int update(const AisReport& r, uint32_t nowMs);
    // Recalcula distancias y CPA y quita los caducados. hasOwn = false: sin posicion propia (no hay distancias).
    void compute(bool hasOwn, const Mover& own, uint32_t nowMs);
    int count() const { return n_; }
    const AisTarget& at(int i) const { return t_[i]; }
    int find(uint32_t mmsi) const;
    bool silence(uint32_t mmsi, bool on);
    int dangerousCount() const;
    // Indices ordenados: primero los peligrosos (por TCPA), luego el resto por distancia; los que no tienen posicion, al final.
    int sorted(int* idx, int maxN) const;
    uint32_t rejectedJumps() const { return jumps_; }

   private:
    AisTarget t_[kMax];
    int n_ = 0;
    double cpaLimit_ = 0.5, tcpaLimit_ = 20.0;
    uint32_t lostMs_ = 6 * 60000u, removeMs_ = 12 * 60000u, jumps_ = 0;
};

// Nombre corto del tipo de buque (codigo AIS de dos cifras): "Carguero", "Petrolero", "Pesquero", "Velero"...
const char* aisShipTypeNameEs(int shipType);

}  // namespace plotter
#endif
