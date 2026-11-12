/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Tabla unica de datos: cada valor guarda su fuente y su hora. Varias fuentes para un mismo dato: gana la de
// mayor prioridad mientras siga llegando; si se calla, entra la siguiente.
#ifndef PLOTTER_STORE_H_
#define PLOTTER_STORE_H_

#include <cstdint>

namespace plotter {

enum class Key : uint8_t {
    Lat, Lon, Sog, Cog, Heading, Stw, Depth, WaterTemp, Awa, Aws, Twd, Tws, Pressure, AirTemp, Rpm, BatteryV,
    HeadingMag, Pitch, Roll, RudderAngle, Altitude, Hdop, SatsUsed,
    Gust, Humidity, Visibility, Count
};

// Orden de prioridad: menor = mejor. "Uart" y no "Serial": Arduino define Serial como macro
enum class Source : uint8_t { Sensor = 0, Can = 1, Uart = 2, Tcp = 3, Ble = 4, Internet = 5, None = 255 };

enum class Quality : uint8_t { Measured, Computed, Estimated };

class DataStore {
   public:
    // timeoutMs: tiempo sin datos tras el que un valor deja de valer (por fuente)
    void setTimeout(Source s, uint32_t timeoutMs);
    // true si el valor se acepta (fuente mejor o igual, o el valor actual ha caducado)
    bool set(Key k, double v, Source s, uint32_t nowMs, Quality q = Quality::Measured);
    bool get(Key k, uint32_t nowMs, double& v) const;
    Source source(Key k, uint32_t nowMs) const;             // None si no hay valor vigente
    Quality quality(Key k) const { return e_[int(k)].q; }
    uint32_t ageMs(Key k, uint32_t nowMs) const;

   private:
    struct Entry { double v = 0; uint32_t t = 0; Source s = Source::None; Quality q = Quality::Measured; };
    bool fresh(const Entry& e, uint32_t nowMs) const;
    uint32_t timeoutOf(Source s) const;
    Entry e_[int(Key::Count)];
    uint32_t to_[6] = {10000, 10000, 10000, 15000, 30000, 7200000};   // internet: 2 h
};

}  // namespace plotter
#endif
