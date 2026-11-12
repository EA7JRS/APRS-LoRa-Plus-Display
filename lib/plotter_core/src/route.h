/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Seguimiento de ruta (tramo activo, llegada y paso al siguiente waypoint), registro de la derrota y de la sonda, y sonido de
// los avisos por nivel. Los modos de paso de waypoint y los valores de partida del registro siguen los de AvNav (wellenvogel).
#ifndef PLOTTER_ROUTE_H_
#define PLOTTER_ROUTE_H_

#include <cstdint>

#include "geo.h"
#include "inputs.h"

namespace plotter {

// Cuando se pasa al siguiente waypoint
enum class WpSwitch : uint8_t {
    Early,      // unos segundos despues de entrar en el circulo de llegada
    Abeam,      // al quedar el waypoint por el traves (demora a 90 grados o mas del rumbo del tramo)
    Late        // dentro del circulo, cuando la distancia vuelve a crecer
};

struct RouteStatus {
    bool active = false;
    int target = 0;                 // indice del waypoint de destino
    double bearing = 0;             // demora verdadera al destino
    double distanceNm = 0;
    double legBearing = 0;          // rumbo del tramo
    double xteNm = 0;               // > 0: el barco esta a estribor del tramo
    bool inArrival = false;         // dentro del circulo de llegada (alarma de llegada)
    bool abeam = false;             // el waypoint ha quedado por el traves
    bool switched = false;          // en esta llamada se ha pasado al siguiente
    bool finished = false;          // llegada al ultimo waypoint
};

class RouteFollower {
   public:
    // "pts" debe seguir existiendo mientras la ruta este activa. from = waypoint de destino inicial (0 = ir al primero).
    void start(const LatLon* pts, int n, LatLon here, int target = 0);
    void stop() { st_ = RouteStatus(); }
    void setMode(WpSwitch m, uint32_t earlySeconds = 10) { mode_ = m; earlyS_ = earlySeconds; }
    void setArrivalRadiusNm(double nm) { radius_ = nm; }
    void setRhumbLine(bool on) { rhumb_ = on; }     // true: loxodromica; false: ortodromica
    bool skip(LatLon here);                         // pasar a mano al siguiente; false si ya es el ultimo
    const RouteStatus& update(LatLon here, uint32_t nowMs);
    const RouteStatus& status() const { return st_; }

   private:
    void advance(LatLon here);
    const LatLon* pts_ = nullptr;
    int n_ = 0;
    LatLon origin_{0, 0};
    WpSwitch mode_ = WpSwitch::Abeam;
    uint32_t earlyS_ = 10, arrivalMs_ = 0;
    bool wasIn_ = false, rhumb_ = false;
    double radius_ = 0.05, minDist_ = 1e9;
    RouteStatus st_;
};

// Registro de la derrota: un punto cada "intervalMs" si el barco se ha movido mas de "minDistM".
class TrackSampler {
   public:
    explicit TrackSampler(uint32_t intervalMs = 10000, double minDistM = 25.0) : interval_(intervalMs), minDist_(minDistM) {}
    void set(uint32_t intervalMs, double minDistM) { interval_ = intervalMs; minDist_ = minDistM; }
    bool sample(LatLon here, uint32_t nowMs);       // true: guardar este punto
    void reset() { any_ = false; }                  // derrota nueva
    double distanceNm() const { return totalNm_; }  // recorrido de la derrota

   private:
    uint32_t interval_, last_ = 0;
    double minDist_, totalNm_ = 0;
    bool any_ = false;
    LatLon prev_{0, 0};
};

// Linea del registro de sonda: "hora ISO,lat,lon,profundidad" (CSV). Devuelve la longitud, 0 si no cabe.
int depthLogLine(char* out, int size, const char* isoTime, LatLon here, double depthM);

// Sonido de cada nivel de aviso: pitidos por rafaga, cada cuanto se repite (0 = una vez) y si hay que confirmarlo para callarlo
struct AlertSound { uint8_t beeps; uint16_t repeatS; bool needsAck; };
AlertSound alertSound(AlertLevel l);

}  // namespace plotter
#endif
