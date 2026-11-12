/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "signalk.h"

#include <cstdio>
#include <cstring>

#include "geo.h"

namespace plotter {

namespace {

const double kRad = 57.29577951308232;
const double kMsToKn = 3600.0 / 1852.0;

enum class Conv : uint8_t { None, MsToKn, RadTo360, RadTo180, KToC, PaToHpa, HzToRpm };

struct Map { const char* path; Key key; Conv conv; };

// La primera ruta de cada dato es la que se usa al publicar
const Map kMap[] = {
    {"navigation.position.latitude", Key::Lat, Conv::None},
    {"navigation.position.longitude", Key::Lon, Conv::None},
    {"navigation.speedOverGround", Key::Sog, Conv::MsToKn},
    {"navigation.courseOverGroundTrue", Key::Cog, Conv::RadTo360},
    {"navigation.headingTrue", Key::Heading, Conv::RadTo360},
    {"navigation.headingMagnetic", Key::HeadingMag, Conv::RadTo360},
    {"navigation.speedThroughWater", Key::Stw, Conv::MsToKn},
    {"navigation.attitude.pitch", Key::Pitch, Conv::RadTo180},
    {"navigation.attitude.roll", Key::Roll, Conv::RadTo180},
    {"steering.rudderAngle", Key::RudderAngle, Conv::RadTo180},
    {"environment.depth.belowSurface", Key::Depth, Conv::None},
    {"environment.depth.belowTransducer", Key::Depth, Conv::None},
    {"environment.water.temperature", Key::WaterTemp, Conv::KToC},
    {"environment.wind.angleApparent", Key::Awa, Conv::RadTo180},
    {"environment.wind.speedApparent", Key::Aws, Conv::MsToKn},
    {"environment.wind.directionTrue", Key::Twd, Conv::RadTo360},
    {"environment.wind.speedTrue", Key::Tws, Conv::MsToKn},
    {"environment.outside.pressure", Key::Pressure, Conv::PaToHpa},
    {"environment.outside.temperature", Key::AirTemp, Conv::KToC},
};

double fromSi(Conv c, double v) {
    switch (c) {
        case Conv::MsToKn: return v * kMsToKn;
        case Conv::RadTo360: return norm360(v * kRad);
        case Conv::RadTo180: return norm180(v * kRad);
        case Conv::KToC: return v - 273.15;
        case Conv::PaToHpa: return v / 100.0;
        case Conv::HzToRpm: return v * 60.0;
        default: return v;
    }
}

double toSi(Conv c, double v) {
    switch (c) {
        case Conv::MsToKn: return v / kMsToKn;
        case Conv::RadTo360: case Conv::RadTo180: return v / kRad;
        case Conv::KToC: return v + 273.15;
        case Conv::PaToHpa: return v * 100.0;
        case Conv::HzToRpm: return v / 60.0;
        default: return v;
    }
}

// "propulsion.<motor>.revolutions", "electrical.batteries.<n>.voltage": cualquier identificador en medio
bool wild(const char* path, const char* prefix, const char* suffix) {
    size_t lp = std::strlen(prefix), ls = std::strlen(suffix), l = std::strlen(path);
    if (l <= lp + ls || std::strncmp(path, prefix, lp) != 0 || std::strcmp(path + l - ls, suffix) != 0) return false;
    for (const char* p = path + lp; p < path + l - ls; p++)
        if (*p == '.') return false;
    return true;
}

}  // namespace

bool skApply(const char* path, double v, Source src, uint32_t now, DataStore& st) {
    if (!path || v != v) return false;                          // NaN = sin dato
    for (const Map& m : kMap)
        if (!std::strcmp(path, m.path)) { st.set(m.key, fromSi(m.conv, v), src, now); return true; }
    if (wild(path, "propulsion.", ".revolutions")) { st.set(Key::Rpm, v * 60.0, src, now); return true; }
    if (wild(path, "electrical.batteries.", ".voltage")) { st.set(Key::BatteryV, v, src, now); return true; }
    return false;
}

int skSubscribe(char* out, int size, uint32_t periodMs) {
    int n = std::snprintf(out, size_t(size), "{\"context\":\"vessels.self\",\"subscribe\":[{\"path\":\"*\",\"period\":%lu}]}\r\n",
                          static_cast<unsigned long>(periodMs));
    return (n > 0 && n < size) ? n : 0;
}

const char* skNotificationKey(const char* path) {
    static const char kPre[] = "notifications.";
    if (!path || std::strncmp(path, kPre, sizeof(kPre) - 1) != 0 || !path[sizeof(kPre) - 1]) return nullptr;
    return path + sizeof(kPre) - 1;
}

const char* skNotificationTitleEs(const char* key) {
    static const struct { const char* key; const char* es; } k[] = {
        {"mob", "Hombre al agua"}, {"fire", "Fuego"}, {"sinking", "Via de agua"}, {"flooding", "Inundacion"},
        {"collision", "Colision"}, {"grounding", "Varada"}, {"listing", "Escora"}, {"adrift", "A la deriva"},
        {"piracy", "Pirateria"}, {"abandon", "Abandono"}, {"navigation.anchor", "Garreo"},
        {"navigation.closestApproach", "Riesgo de abordaje"}, {"environment.depth.belowTransducer", "Poca profundidad"},
        {"environment.depth.belowSurface", "Poca profundidad"},
    };
    if (!key) return nullptr;
    for (const auto& e : k)
        if (!std::strcmp(key, e.key)) return e.es;
    if (wild(key, "electrical.batteries.", ".voltage")) return "Bateria";
    return nullptr;
}

const char* skPathFor(Key k) {
    for (const Map& m : kMap)
        if (m.key == k) return m.path;
    return nullptr;
}

double skToSi(Key k, double v) {
    for (const Map& m : kMap)
        if (m.key == k) return toSi(m.conv, v);
    return v;
}

int skDelta(char* out, int size, const char* path, double v) {
    if (!path) return 0;
    int n = std::snprintf(out, size_t(size), "{\"updates\":[{\"values\":[{\"path\":\"%s\",\"value\":%.7g}]}]}\r\n", path, v);
    return (n > 0 && n < size) ? n : 0;
}

}  // namespace plotter
