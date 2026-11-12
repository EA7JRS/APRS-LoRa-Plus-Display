/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Entradas sueltas de la placa: pulsos (corredera de paletas, cuentavueltas), interruptores (sentina, puerta), sondas DS18B20,
// y los niveles de aviso de Signal K con umbrales.
#ifndef PLOTTER_INPUTS_H_
#define PLOTTER_INPUTS_H_

#include <cstdint>

#include "wind.h"

namespace plotter {

// Entrada de pulsos generica. Tiempos en microsegundos.
class PulseInput {
   public:
    explicit PulseInput(double pulsesPerRev = 1.0, uint32_t debounceUs = 5000, uint32_t windowUs = 3000000, uint32_t stopUs = 5000000);
    bool pulse(uint64_t nowUs) { return p_.pulse(nowUs); }      // false si se descarta por rebote
    double hz(uint64_t nowUs) { return p_.knots(nowUs); }       // pulsos por segundo; 0 si esta parado
    double rpm(uint64_t nowUs) { return hz(nowUs) / perRev_ * 60.0; }
    uint32_t pulses() const { return p_.pulseCount() - base_; }
    double revolutions() const { return pulses() / perRev_; }
    void resetCount() { base_ = p_.pulseCount(); }              // parcial a cero

   private:
    PulseAnemometer p_;
    double perRev_;
    uint32_t base_ = 0;
};

// Rueda o helice de radio conocido: velocidad en m/s y distancia en metros (calibracion = factor de correccion)
double wheelSpeedMs(double revPerSec, double radiusM, double calibration = 1.0);
double wheelDistanceM(double revolutions, double radiusM, double calibration = 1.0);
// Corredera de paletas: el fabricante da los pulsos por milla (p. ej. 20000)
double logSpeedKn(double hz, double pulsesPerNm);
double logDistanceNm(uint32_t pulses, double pulsesPerNm);

// Interruptor con filtro de rebotes. Tiempos en milisegundos.
class DigitalInput {
   public:
    explicit DigitalInput(bool activeLow = true, uint32_t debounceMs = 50) : activeLow_(activeLow), debounceMs_(debounceMs) {}
    bool update(bool pinHigh, uint32_t nowMs);      // true cuando cambia el estado ya filtrado
    bool active() const { return active_; }
    uint32_t sinceMs(uint32_t nowMs) const { return nowMs - changedMs_; }

   private:
    bool activeLow_, active_ = false, cand_ = false, init_ = false;
    uint32_t debounceMs_, candMs_ = 0, changedMs_ = 0;
};

// DS18B20 (1-Wire): CRC-8 de Dallas y temperatura a partir de los 9 bytes leidos
uint8_t dallasCrc8(const uint8_t* data, int len);
bool ds18b20Celsius(const uint8_t scratchpad[9], double& celsius);     // false si el CRC falla o la sonda no contesta

// Niveles de aviso, los mismos que usa Signal K (notifications.*.state)
enum class AlertLevel : uint8_t { Nominal = 0, Normal = 1, Alert = 2, Warn = 3, Alarm = 4, Emergency = 5 };
AlertLevel alertLevelFromSignalK(const char* state);       // desconocido -> Normal
const char* alertLevelSignalK(AlertLevel l);
const char* alertLevelNameEs(AlertLevel l);
inline bool alertSounds(AlertLevel l) { return l >= AlertLevel::Warn; }

// Aviso por umbral con histeresis: fuera de [low, high] da "level"; vuelve a Normal al entrar "hysteresis" dentro del margen.
class ThresholdAlert {
   public:
    ThresholdAlert(double low, double high, double hysteresis, AlertLevel level) : lo_(low), hi_(high), hys_(hysteresis), level_(level) {}
    AlertLevel update(double value);
    AlertLevel state() const { return on_ ? level_ : AlertLevel::Normal; }

   private:
    double lo_, hi_, hys_;
    AlertLevel level_;
    bool on_ = false;
};

}  // namespace plotter
#endif
