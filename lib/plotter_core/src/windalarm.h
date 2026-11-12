/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Alarmas de viento del instrumento: viento real fuerte, viento aparente fuerte y rolada (cambio de la direccion del
// viento real respecto a una referencia). Cada una salta si la condicion dura unos segundos (una racha suelta no) y se
// apaga con histeresis. "Silenciar" calla zumbador y ventana 10 min; "cerrar" las calla hasta que la condicion pase y
// vuelva (en la rolada, toma la direccion de ahora como nueva referencia).
#ifndef PLOTTER_WINDALARM_H_
#define PLOTTER_WINDALARM_H_

#include <stdint.h>

namespace plotter {

enum class WindAlarmKind : uint8_t { None, Tws, Aws, Shift };

struct WindAlarmConfig {
    double twsMaxKn = 0;        // 0 = apagada
    double awsMaxKn = 0;
    double shiftDeg = 0;
};

class WindAlarm {
   public:
    void configure(const WindAlarmConfig& c);
    // Cada segundo, mas o menos, con lo que haya (NAN = sin dato). Devuelve la alarma activa mas importante
    WindAlarmKind update(uint32_t nowMs, double twsKn, double awsKn, double twdDeg);
    WindAlarmKind active() const { return active_; }
    // Hay que avisar (ventana y zumbador): activa, sin silenciar ni cerrar
    bool alerting(uint32_t nowMs) const;
    void silence(uint32_t nowMs, uint32_t ms = 600000);
    void dismiss(uint32_t nowMs);
    double value() const;       // de la activa: kn o grados de rolada (+ a la derecha)
    double limit() const;
    double shiftDeg() const { return shift_; }
    bool hasReference() const { return refOk_; }

   private:
    struct Level { bool on = false, dismissed = false; uint32_t overMs = 0, underMs = 0; };
    void speed(Level& l, double v, double lim, uint32_t now);
    WindAlarmConfig cfg_;
    Level tws_, aws_, sh_;
    double twsV_ = 0, awsV_ = 0;
    double ref_ = 0, smooth_ = 0, shift_ = 0;
    bool refOk_ = false, smoothOk_ = false;
    uint32_t lastMs_ = 0, snoozeUntil_ = 0, firstMs_ = 0;
    WindAlarmKind active_ = WindAlarmKind::None;
};

}  // namespace plotter
#endif
