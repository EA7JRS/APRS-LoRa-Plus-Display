/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Herramientas de la carta: medir entre dos puntos, datos del punto tocado, texto de una posicion e historial de vistas.
#ifndef PLOTTER_CHARTUI_H_
#define PLOTTER_CHARTUI_H_

#include <cstdint>

#include "geo.h"

namespace plotter {

struct Measure {
    double distanceNm = 0;
    double bearing = 0;             // verdadera, de A a B
    double reciprocal = 0;          // de B a A
    double hours = -1;              // tiempo a la velocidad dada; -1 si no hay velocidad
};
// rhumb = true: loxodromica (rumbo constante); false: ortodromica. speedKn <= 0: sin tiempo.
Measure measure(LatLon a, LatLon b, bool rhumb = true, double speedKn = 0);
// Distancia de una linea de varios tramos (medida encadenada)
double pathLengthNm(const LatLon* pts, int n, bool rhumb = true);

// "36°31,840′N 006°17,020′W". decimals = decimales de los minutos (0-4); comma = coma decimal.
int formatLatLon(char* out, int size, LatLon p, int decimals = 3, bool comma = true);
// "02 h 35 min", "45 min", "3 d 04 h"
int formatDuration(char* out, int size, double hours);

// Vistas anteriores de la carta (centro y zoom) para el boton "volver"
struct ChartView { LatLon center{0, 0}; int zoom = 0; };
class ViewHistory {
   public:
    void push(const ChartView& v);              // no guarda una vista igual a la ultima
    bool back(ChartView& v);                    // false si no hay anterior
    int size() const { return n_; }
    void clear() { n_ = 0; }

   private:
    static constexpr int kMax = 12;
    ChartView v_[kMax];
    int n_ = 0;
};

}  // namespace plotter
#endif
