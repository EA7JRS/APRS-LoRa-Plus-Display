/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter. GPL-3.0-or-later (ver LICENSE).
 */
#pragma once
// Filtro de posicion con el equipo parado: el fix de un GPS baila unos metros aunque la estacion no se mueva, y eso haria
// bailar lo que dependa de la posicion (apuntado de un rotor, situacion en la carta). Congela la posicion mientras esta
// parado y solo sigue el fix en vivo cuando se mueve de verdad (coche / barco). Mismo filtro de velocidad que el tracker
// APRS LoRa+ (gps_extras.cpp) y que el demonio de la Raspberry del rotor (gnss.py): histeresis con dos umbrales y 3 lecturas,
// y un salto grande sin velocidad fiable. Vale para un u-blox local (NMEA RMC con velocidad) o para gpsd / NMEA por la red.
// Compartido por todo el ecosistema (plotter, Display, rotor); el origen es este plotter_core.
#include <math.h>
#include <stdint.h>

namespace plotter {

    class PosHold {
    public:
        bool   enabled  = true;
        double startMs  = 0.97;      // m/s por encima de los cuales, 3 lecturas seguidas, se da por en marcha
        double stopMs   = 0.5;       // m/s por debajo de los cuales, 3 lecturas seguidas, se vuelve a parado
        double maxJumpM = 15.0;      // salto en m que tambien lo da por movido cuando no hay velocidad fiable

        // Entra una posicion (grados) y, si se conoce, la velocidad en m/s (<0 = desconocida). Devuelve la posicion a usar.
        void update(double lat, double lon, double spdMs, double& outLat, double& outLon) {
            if (!enabled) { held_ = false; hlat_ = lat; hlon_ = lon; outLat = lat; outLon = lon; return; }
            if (!have_) { hlat_ = lat; hlon_ = lon; have_ = true; }
            if (spdMs >= 0) {
                if (!moving_) { above_ = spdMs >= startMs ? above_ + 1 : 0; if (above_ >= 3) { moving_ = true; above_ = below_ = 0; } }
                else          { below_ = spdMs <  stopMs  ? below_ + 1 : 0; if (below_ >= 3) { moving_ = false; above_ = below_ = 0; } }
            }
            const bool far = distM(hlat_, hlon_, lat, lon) > maxJumpM;
            if (moving_ || far) {
                hlat_ = lat; hlon_ = lon; held_ = false;
                if (far && !moving_) above_ = below_ = 0;
            } else {
                held_ = true;
            }
            outLat = hlat_; outLon = hlon_;
        }
        bool held() const { return held_; }
        void reset() { have_ = moving_ = held_ = false; above_ = below_ = 0; }

        // Distancia en metros (equirectangular, de sobra a estas escalas)
        static double distM(double la1, double lo1, double la2, double lo2) {
            const double k = cos((la1 + la2) * 0.5 * M_PI / 180.0);
            const double dx = (lo2 - lo1) * k, dy = la2 - la1;
            return sqrt(dx * dx + dy * dy) * 111320.0;
        }

    private:
        bool   have_ = false, moving_ = false, held_ = false;
        int    above_ = 0, below_ = 0;
        double hlat_ = 0, hlon_ = 0;
    };
}
