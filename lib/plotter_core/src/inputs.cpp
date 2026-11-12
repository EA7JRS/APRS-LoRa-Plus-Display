/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "inputs.h"

#include <cstring>

namespace plotter {

static const double kPi = 3.14159265358979323846;

PulseInput::PulseInput(double perRev, uint32_t deb, uint32_t win, uint32_t stop)
    : p_(1.0, deb, win, stop), perRev_(perRev > 0 ? perRev : 1.0) {}

double wheelSpeedMs(double rps, double r, double cal) { return rps * 2 * kPi * r * cal; }
double wheelDistanceM(double rev, double r, double cal) { return rev * 2 * kPi * r * cal; }
double logSpeedKn(double hz, double ppnm) { return ppnm > 0 ? hz * 3600.0 / ppnm : 0; }
double logDistanceNm(uint32_t pulses, double ppnm) { return ppnm > 0 ? pulses / ppnm : 0; }

bool DigitalInput::update(bool pinHigh, uint32_t now) {
    bool a = activeLow_ ? !pinHigh : pinHigh;
    if (!init_) { init_ = true; active_ = cand_ = a; candMs_ = changedMs_ = now; return false; }
    if (a != cand_) { cand_ = a; candMs_ = now; }
    if (cand_ != active_ && now - candMs_ >= debounceMs_) {
        active_ = cand_;
        changedMs_ = now;
        return true;
    }
    return false;
}

uint8_t dallasCrc8(const uint8_t* d, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t b = d[i];
        for (int k = 0; k < 8; k++) {
            uint8_t mix = (crc ^ b) & 1;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            b >>= 1;
        }
    }
    return crc;
}

bool ds18b20Celsius(const uint8_t sp[9], double& c) {
    bool allZero = true, allOnes = true;
    for (int i = 0; i < 9; i++) { if (sp[i] != 0) allZero = false; if (sp[i] != 0xFF) allOnes = false; }
    if (allZero || allOnes || dallasCrc8(sp, 8) != sp[8]) return false;       // bus en corto, sin sonda o lectura danada
    int16_t raw = int16_t(uint16_t(sp[1]) << 8 | sp[0]);
    switch ((sp[4] >> 5) & 3) {                                                // resolucion: los bits que sobran no valen
        case 0: raw &= ~7; break;          // 9 bits
        case 1: raw &= ~3; break;          // 10
        case 2: raw &= ~1; break;          // 11
        default: break;                    // 12
    }
    c = raw / 16.0;
    return true;
}

static const char* const kSk[] = {"nominal", "normal", "alert", "warn", "alarm", "emergency"};
static const char* const kEs[] = {"Nominal", "Normal", "Alerta", "Aviso", "Alarma", "Emergencia"};

AlertLevel alertLevelFromSignalK(const char* s) {
    if (s)
        for (int i = 0; i < 6; i++)
            if (!std::strcmp(s, kSk[i])) return AlertLevel(i);
    return AlertLevel::Normal;
}
const char* alertLevelSignalK(AlertLevel l) { return kSk[int(l) <= 5 ? int(l) : 1]; }
const char* alertLevelNameEs(AlertLevel l) { return kEs[int(l) <= 5 ? int(l) : 1]; }

AlertLevel ThresholdAlert::update(double v) {
    if (!on_) { if (v < lo_ || v > hi_) on_ = true; }
    else if (v >= lo_ + hys_ && v <= hi_ - hys_) on_ = false;
    return state();
}

}  // namespace plotter
