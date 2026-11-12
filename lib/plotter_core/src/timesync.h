/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Hora de los equipos APRS LoRa+: el reloj interno se pone con la hora del GPS (RMC / ZDA de cualquier fuente NMEA 0183, PGN
// 126992 / 129029 de NMEA 2000) o, sin GPS, con NTP; de serie el Real Instituto y Observatorio de la Armada (hora oficial de
// Espana: ntp.roa.es, que reparte entre hora.roa.es y minuto.roa.es) y pool.ntp.org de reserva. La hora de otro equipo
// (el plotter al instrumento, el tracker a la pantalla) cuenta despues de NTP; el reloj de la placa (RTC), al final.
// ClockSync decide cuando poner el reloj: no lo toca una fuente peor mientras la mejor siga al dia (6 h).
#ifndef PLOTTER_TIMESYNC_H_
#define PLOTTER_TIMESYNC_H_

#include <cstdint>

namespace plotter {

constexpr const char* kNtpDefault = "ntp.roa.es";       // hora oficial de Espana (ROA)
constexpr const char* kNtpSecond = "hora.roa.es";
constexpr const char* kNtpFallback = "pool.ntp.org";

// Orden de preferencia: menor = mejor
enum class TimeSource : uint8_t { Gps = 0, Ntp = 1, Peer = 2, Rtc = 3, None = 255 };
const char* timeSourceName(TimeSource s);               // "GPS", "NTP", "peer", "RTC", "-"

// Segundos UNIX de una fecha y hora UTC (anio completo); -1 si no vale
int64_t utcToUnix(int year, int month, int day, int hour, int minute, int second);
// Hora de una sentencia RMC (con estado A) o ZDA, con o sin suma; false si no la trae o no es valida. ms: milisegundos.
bool nmeaUtc(const char* line, int64_t& unixSec, int& ms);
// "2026-11-12T06:00:00Z" (evento gnss del tracker APRS LoRa+, Signal K); false si no vale
bool isoUtc(const char* text, int64_t& unixSec);
// Hora de NMEA 2000: dias desde 1970 y decimas de milisegundo desde medianoche (126992, 129029); -1 si no vale
int64_t n2kUtc(uint16_t days, uint32_t tenthMs);

class ClockSync {
   public:
    static constexpr int64_t kMinValid = 1767225600;     // 2026-01-01: un GPS sin almanaque da 1980 o 2006
    static constexpr int64_t kMaxValid = 4102444800;     // 2100-01-01
    uint32_t keepMs = 6u * 3600u * 1000u;                // la fuente que puso la hora manda durante este tiempo

    // Hora de una fuente. sysUnix: lo que marca el reloj ahora (0 = no se sabe). true = poner el reloj con unixSec.
    // El GPS se pone con dos horas seguidas que cuadran entre si (la primera de un arranque en frio puede estar mal).
    bool offer(TimeSource s, int64_t unixSec, int64_t sysUnix, uint32_t nowMs);
    void noteSet(TimeSource s, uint32_t nowMs);          // la puso otro (el cliente SNTP del sistema)
    TimeSource source(uint32_t nowMs) const;             // quien la puso por ultima vez (None si nunca o hace mas de keepMs)
    uint32_t ageS(uint32_t nowMs) const { return set_ ? (nowMs - setMs_) / 1000u : 0; }
    uint32_t sets() const { return sets_; }
    int64_t lastOffset() const { return offset_; }       // segundos que se ha corregido la ultima vez

   private:
    bool set_ = false;
    TimeSource src_ = TimeSource::None;
    uint32_t setMs_ = 0, sets_ = 0, gpsMs_ = 0;
    int64_t gpsPrev_ = 0, offset_ = 0;
};

}  // namespace plotter
#endif
