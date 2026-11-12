/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Cielo GNSS a partir de GSV (satelites a la vista) y GSA (usados y DOP), y la orden de arranque para un gpsd de la red.
#ifndef PLOTTER_GNSS_H_
#define PLOTTER_GNSS_H_

#include <cstdint>

namespace plotter {

enum class GnssSystem : uint8_t { Gps, Glonass, Galileo, Beidou, Qzss, Navic, Sbas, Other };

struct GnssSat {
    GnssSystem sys = GnssSystem::Other;
    uint16_t prn = 0;
    int8_t elev = -1;           // grados; -1 sin dato
    int16_t az = -1;            // grados; -1 sin dato
    int8_t snr = -1;            // dB-Hz; -1 = no se recibe
    bool used = false;
};

GnssSystem gnssSystemFromTalker(const char* talker);        // "GP", "GL", "GA", "GB" / "BD", "GQ", "GI"
const char* gnssSystemName(GnssSystem s);

class GnssSky {
   public:
    static constexpr int kMax = 64;
    // Una trama GSV ya troceada: f[0] = "GPGSV", f[1] total, f[2] numero, f[3] satelites, y grupos de prn, elevacion, azimut, snr
    void feedGsv(const char* const* f, int n, uint32_t nowMs);
    // Una trama GSA: f[2] tipo de fix, f[3..14] satelites usados, f[15..17] PDOP, HDOP, VDOP, f[18] sistema (NMEA 4.10)
    void feedGsa(const char* const* f, int n, uint32_t nowMs);
    int count() const { return n_; }
    const GnssSat& sat(int i) const { return s_[i]; }
    int usedCount() const;
    int fixMode() const { return fix_; }                    // 0 sin dato, 1 sin fix, 2 = 2D, 3 = 3D
    double pdop() const { return pdop_; }
    double hdop() const { return hdop_; }
    double vdop() const { return vdop_; }
    void expire(uint32_t nowMs, uint32_t maxAgeMs = 10000); // quita constelaciones que han dejado de llegar

   private:
    struct Group { GnssSystem sys; uint8_t sig; uint32_t t; bool on; };
    void dropSystem(GnssSystem sys, uint8_t sig);
    GnssSat s_[kMax];
    uint8_t sig_[kMax] = {};
    GnssSystem org_[kMax] = {};        // constelacion del emisor de la trama
    int n_ = 0, fix_ = 0;
    double pdop_ = 0, hdop_ = 0, vdop_ = 0;
    static constexpr int kUsedMax = 48;
    struct Used { GnssSystem sys; uint16_t prn; bool anySys; };
    Used used_[kUsedMax];
    int nUsed_ = 0;
    uint32_t usedT_ = 0;
    Group grp_[12] = {};
};

constexpr uint16_t kGpsdPort = 2947;
// Orden para que gpsd reenvie el NMEA de sus receptores (las lineas que empiezan por '{' son suyas y se ignoran).
constexpr const char* kGpsdWatchNmea = "?WATCH={\"enable\":true,\"nmea\":true};\r\n";

}  // namespace plotter
#endif
