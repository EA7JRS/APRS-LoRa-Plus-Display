/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Simulador: un barco que navega y las tramas NMEA 0183 que darian sus instrumentos. Para probar la interfaz en tierra
// y en las pruebas del PC. Las tramas pasan por el mismo lector que las reales.
#ifndef PLOTTER_SIM_H_
#define PLOTTER_SIM_H_

#include <cstdint>

#include "geo.h"

namespace plotter {

class NmeaSimulator {
   public:
    explicit NmeaSimulator(LatLon start = {36.5307, -6.2837}, double cogDeg = 247.0, double sogKn = 6.2);
    // Avanza el barco hasta "nowMs". Llamar antes de pedir tramas.
    void step(uint32_t nowMs);
    // Trama numero "i" del ciclo (0 .. count()-1), con suma y sin fin de linea. Devuelve la longitud, 0 si no cabe.
    static constexpr int count() { return 7; }
    int sentence(int i, char* out, int size) const;

    // Blancos AIS de prueba alrededor del barco: tramas pares = posicion, impares = nombre
    static constexpr int aisCount() { return 2 * kTargets; }
    int aisSentence(int i, char* out, int size) const;

    LatLon position() const { return pos_; }
    double sog() const { return sog_; }
    double cog() const { return cog_; }
    double depth() const { return depth_; }

   private:
    LatLon pos_;
    double cog_, sog_, heading_, depth_ = 12.4, twd_ = 285.0, tws_ = 11.0, waterC_ = 18.5;
    static constexpr int kTargets = 5;
    struct Target { uint32_t mmsi; const char* name; LatLon pos; double cog, sog; };
    Target tg_[kTargets];
    uint32_t last_ = 0, t0_ = 0;
    bool started_ = false;
};

}  // namespace plotter
#endif
