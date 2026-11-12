/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "timesync.h"

#include <cstdlib>
#include <cstring>

namespace plotter {

const char* timeSourceName(TimeSource s) {
    switch (s) {
        case TimeSource::Gps: return "GPS";
        case TimeSource::Ntp: return "NTP";
        case TimeSource::Peer: return "peer";
        case TimeSource::Rtc: return "RTC";
        default: return "-";
    }
}

// Dias desde 1970-01-01 (calendario gregoriano, algoritmo de Howard Hinnant)
static int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int64_t utcToUnix(int year, int month, int day, int hour, int minute, int second) {
    static const int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1970 || year > 2199 || month < 1 || month > 12 || day < 1 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
        second < 0 || second > 60)
        return -1;
    const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (day > mdays[month - 1] + (month == 2 && leap ? 1 : 0)) return -1;
    return daysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
}

int64_t n2kUtc(uint16_t days, uint32_t tenthMs) {
    if (days == 0xFFFF || tenthMs >= 864000000u) return -1;
    return (int64_t)days * 86400 + tenthMs / 10000;
}

static int two(const char* p) { return (p[0] - '0') * 10 + (p[1] - '0'); }
static bool digits(const char* p, int n);

bool isoUtc(const char* s, int64_t& unixSec) {
    if (!s || std::strlen(s) < 19 || !digits(s, 4) || s[4] != '-' || !digits(s + 5, 2) || s[7] != '-' || !digits(s + 8, 2) ||
        (s[10] != 'T' && s[10] != ' ') || !digits(s + 11, 2) || s[13] != ':' || !digits(s + 14, 2) || s[16] != ':' || !digits(s + 17, 2))
        return false;
    const char* z = s + 19;
    if (*z == '.') { z++; while (*z >= '0' && *z <= '9') z++; }
    if (*z && std::strcmp(z, "Z") != 0 && std::strcmp(z, "+00:00") != 0) return false;    // solo UTC
    const int64_t t = utcToUnix(std::atoi(s), two(s + 5), two(s + 8), two(s + 11), two(s + 14), two(s + 17));
    if (t < 0) return false;
    unixSec = t;
    return true;
}

static bool digits(const char* p, int n) {
    for (int i = 0; i < n; i++) if (p[i] < '0' || p[i] > '9') return false;
    return true;
}

bool nmeaUtc(const char* line, int64_t& unixSec, int& ms) {
    if (!line) return false;
    if (line[0] == '\\') {                                  // bloque de etiquetas
        const char* e = std::strchr(line + 1, '\\');
        if (!e) return false;
        line = e + 1;
    }
    if (line[0] != '$') return false;
    // Campos sin la suma (y comprobandola si viene)
    char buf[100];
    size_t n = 0;
    const char* p = line + 1;
    unsigned sum = 0;
    while (*p && *p != '*' && *p != '\r' && *p != '\n' && n < sizeof(buf) - 1) { sum ^= (unsigned char)*p; buf[n++] = *p++; }
    buf[n] = 0;
    if (*p == '*') {
        char* end = nullptr;
        const unsigned long want = std::strtoul(p + 1, &end, 16);
        if (end != p + 3 || want != sum) return false;
    }
    const char* f[16];
    int nf = 0;
    f[nf++] = buf;
    for (char* q = buf; *q && nf < 16; q++) if (*q == ',') { *q = 0; f[nf++] = q + 1; }
    if (std::strlen(f[0]) < 5) return false;
    const char* id = f[0] + std::strlen(f[0]) - 3;
    const char *hms, *date = nullptr;
    int day = 0, month = 0, year = 0;
    if (!std::strcmp(id, "RMC")) {
        if (nf < 10 || f[2][0] != 'A') return false;
        hms = f[1];
        date = f[9];
        if (std::strlen(date) != 6 || !digits(date, 6)) return false;
        day = two(date); month = two(date + 2); year = two(date + 4);
        year += year < 80 ? 2000 : 1900;
    } else if (!std::strcmp(id, "ZDA")) {
        if (nf < 5 || !f[2][0] || !f[3][0] || std::strlen(f[4]) != 4 || !digits(f[4], 4)) return false;
        hms = f[1];
        day = std::atoi(f[2]); month = std::atoi(f[3]); year = std::atoi(f[4]);
    } else {
        return false;
    }
    if (std::strlen(hms) < 6 || !digits(hms, 6)) return false;
    ms = 0;
    if (hms[6] == '.') {                                     // fraccion: hasta milisegundos
        int scale = 100;
        for (const char* c = hms + 7; *c >= '0' && *c <= '9' && scale; c++, scale /= 10) ms += (*c - '0') * scale;
    }
    const int64_t t = utcToUnix(year, month, day, two(hms), two(hms + 2), two(hms + 4));
    if (t < 0) return false;
    unixSec = t;
    return true;
}

TimeSource ClockSync::source(uint32_t nowMs) const {
    if (!set_ || nowMs - setMs_ > keepMs) return TimeSource::None;
    return src_;
}

void ClockSync::noteSet(TimeSource s, uint32_t nowMs) {
    const TimeSource cur = source(nowMs);
    if (cur != TimeSource::None && (uint8_t)cur < (uint8_t)s) return;   // manda una mejor: el reloj sigue siendo suyo
    set_ = true; src_ = s; setMs_ = nowMs ? nowMs : 1; sets_++;
}

bool ClockSync::offer(TimeSource s, int64_t t, int64_t sysUnix, uint32_t nowMs) {
    if (t < kMinValid || t > kMaxValid || s == TimeSource::None) return false;
    if (s == TimeSource::Gps) {                              // dos seguidas que cuadran con el tiempo pasado
        const bool fits = gpsMs_ && nowMs - gpsMs_ < 10000 && std::llabs((t - gpsPrev_) * 1000 - (int64_t)(nowMs - gpsMs_)) <= 2000;
        gpsPrev_ = t; gpsMs_ = nowMs ? nowMs : 1;
        if (!fits) return false;
    }
    const TimeSource cur = source(nowMs);
    const int64_t off = sysUnix > 0 ? t - sysUnix : 0;
    bool doSet;
    if (cur == TimeSource::None || (uint8_t)s < (uint8_t)cur) doSet = true;          // la primera, o una fuente mejor
    else if (s == cur) doSet = sysUnix <= 0 || off > 1 || off < -1 || nowMs - setMs_ > 600000u;   // se va, o repaso cada 10 min
    else doSet = false;                                                              // peor que la que manda
    if (!doSet) return false;
    set_ = true; src_ = s; setMs_ = nowMs ? nowMs : 1; sets_++; offset_ = off;
    return true;
}

}  // namespace plotter
