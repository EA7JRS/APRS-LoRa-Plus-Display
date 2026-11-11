/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Mapa del reloj: mundo equirrectangular de 512x256 en RGB565 con la noche, el sol, los spots por banda y la ortodromica
 * hasta el DX elegido. C++ puro (lo pinta sobre un buffer; la interfaz lo ensena como imagen).
 */
#pragma once
#include "hc_model.h"

namespace hcmap {
constexpr int MAP_W = 512, MAP_H = 256;
extern const uint8_t LAND[MAP_W / 8 * MAP_H];            // 1 = tierra (hc_land.cpp, tools/make_hamclock_map.py)

struct Marker { float lat, lon; uint32_t color; };
struct Scene {
    time_t now = 0;
    bool hasDe = false, hasDx = false;
    double deLat = 0, deLon = 0, dxLat = 0, dxLon = 0;
    const Marker* markers = nullptr;
    int nMarkers = 0;
};
void toXY(double lat, double lon, int& x, int& y);
void render(uint16_t* buf, const Scene& s);              // buf: MAP_W * MAP_H pixeles RGB565
}
