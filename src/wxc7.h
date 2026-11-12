/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Calculos meteorologicos para lo que mandan las estaciones APRS, los sensores del tracker y el tiempo de internet.
// La escala Beaufort viene del nucleo de APRS LoRa+ Plotter (wind.cpp). Lo demas son formulas publicadas: punto de rocio
// (Magnus, coeficientes de Alduchov y Eskridge 1996), sensacion por viento (indice JAG/TI de 2001, el de Canada y EE. UU.),
// indice de calor (regresion de Rothfusz del NWS, con sus dos ajustes) y tendencia de la presion (codigo de la OMM a 3 h).
// C++ puro, probado en el PC (test/test_extra.cpp).
#pragma once
#include <cstdint>

namespace wxc {

constexpr float NA = -999.0f;
inline bool has(float v) { return v > -900.0f; }

int beaufort(double knots);                 // 0..12, con el viento redondeado al nudo
const char* beaufortName(int force);        // "Calm", "Light air"... (nombres de la OMM en ingles, como el resto de la interfaz)
inline double kmhToKn(double kmh) { return kmh / 1.852; }

float dewPointC(float tC, float rhPct);                 // NA si falta algo
float windChillC(float tC, float windKmh);              // solo con t <= 10 C y viento > 4,8 km/h; si no, NA
float heatIndexC(float tC, float rhPct);                // solo con t >= 27 C y humedad >= 40 %; si no, NA
// La que toque de las dos (o NA si la sensacion es la temperatura misma). kind: 1 frio por viento, 2 calor, 0 ninguna.
float feelsLikeC(float tC, float rhPct, float windKmh, int* kind = nullptr);
// Altura de la base de las nubes convectivas sobre la estacion (regla de 125 m por grado de separacion), en metros
float cloudBaseM(float tC, float dewC);
// Presion al nivel del mar desde la de la estacion (formula de reduccion con la temperatura; la de la OACI si falta)
float seaLevelHpa(float stationHpa, float altM, float tC);

// Tendencia de la presion: se le dan lecturas con su hora y contesta el cambio en 3 h (extrapolado si hay al menos 1 h).
class PressureTrend {
   public:
    void add(uint32_t seconds, float hpa);              // seconds: reloj que no retrocede (tiempo desde el arranque)
    bool delta3h(uint32_t nowSeconds, float& hpa) const;
    static const char* name(float d3h);                 // "steady", "rising", "rising fast", "falling", "falling fast"
   private:
    static constexpr int N = 40;                        // una lectura cada 5 min como mucho: 3 h 20 min
    uint32_t t_[N] = {};
    float p_[N] = {};
    int n_ = 0, head_ = 0;
};

// Unidades de la interfaz: 0 metrico (km, km/h, C), 1 nautico (mn, kn, C), 2 imperial (mi, mph, F). Como units del plotter.
struct Unit { double k; const char* label; };
Unit distUnit(int system);                  // por km
Unit speedUnit(int system);                 // por km/h
float tempShown(float c, int system);
const char* tempLabel(int system);

} // namespace wxc
