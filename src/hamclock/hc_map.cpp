/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "hc_map.h"
#include <cmath>
#include <cstdlib>

namespace hcmap {

static const double D2R = 3.14159265358979323846 / 180;

static inline uint16_t rgb(uint32_t c) { return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F)); }
static uint32_t mix(uint32_t a, uint32_t b, int t) {     // t de 0 (a) a 16 (b)
    uint32_t r = (((a >> 16) & 255) * (16 - t) + ((b >> 16) & 255) * t) >> 4;
    uint32_t g = (((a >> 8) & 255) * (16 - t) + ((b >> 8) & 255) * t) >> 4;
    uint32_t bl = ((a & 255) * (16 - t) + (b & 255) * t) >> 4;
    return (r << 16) | (g << 8) | bl;
}
static inline bool land(int x, int y) {
    if (y < 0 || y >= MAP_H) return false;
    x = (x + MAP_W) % MAP_W;
    return LAND[y * (MAP_W / 8) + (x >> 3)] & (0x80 >> (x & 7));
}
void toXY(double lat, double lon, int& x, int& y) {
    x = (int)lround((lon + 180) / 360 * MAP_W - 0.5);
    y = (int)lround((90 - lat) / 180 * MAP_H - 0.5);
    x = ((x % MAP_W) + MAP_W) % MAP_W;
    if (y < 0) y = 0;
    if (y >= MAP_H) y = MAP_H - 1;
}
static inline void put(uint16_t* buf, int x, int y, uint16_t c) {
    if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) buf[y * MAP_W + x] = c;
}
static void disc(uint16_t* buf, int cx, int cy, int r, uint32_t fill, uint32_t edge) {
    uint16_t f = rgb(fill), e = rgb(edge);
    for (int dy = -r - 1; dy <= r + 1; dy++)
        for (int dx = -r - 1; dx <= r + 1; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 <= r * r) put(buf, cx + dx, cy + dy, f);
            else if (d2 <= (r + 1) * (r + 1)) put(buf, cx + dx, cy + dy, e);
        }
}
static void line(uint16_t* buf, int x0, int y0, int x1, int y1, uint16_t c) {   // Bresenham de 2 pixeles de grueso
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        put(buf, x0, y0, c); put(buf, x0 + 1, y0, c); put(buf, x0, y0 + 1, c); put(buf, x0 + 1, y0 + 1, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void render(uint16_t* buf, const Scene& s) {
    const uint32_t OCEAN = 0x123048, LAND_C = 0x3A6078, COAST = 0x5C88A8, NIGHT = 0x02060A, GRID = 0x1B3A54;
    // tabla de colores: [tierra / costa / mar / reticula][noche 0..16]
    static uint16_t lut[4][17];
    static bool init = false;
    if (!init) {
        const uint32_t base[4] = {LAND_C, COAST, OCEAN, GRID};
        for (int k = 0; k < 4; k++)
            for (int t = 0; t <= 16; t++) lut[k][t] = rgb(mix(base[k], NIGHT, t * 3 / 4));   // la noche tapa hasta un 75 %
        init = true;
    }
    double dec, slon;
    hc::sunPosition(s.now, dec, slon);
    double sd = sin(dec * D2R), cd = cos(dec * D2R);
    static float cosHa[MAP_W];
    for (int x = 0; x < MAP_W; x++) cosHa[x] = (float)cos(((x + 0.5) * 360.0 / MAP_W - 180 - slon) * D2R);
    for (int y = 0; y < MAP_H; y++) {
        double lat = (90 - (y + 0.5) * 180.0 / MAP_H) * D2R;
        float a = (float)(sin(lat) * sd), b = (float)(cos(lat) * cd);
        bool gridRow = (y % (MAP_H / 6)) == 0 && y;
        for (int x = 0; x < MAP_W; x++) {
            // seno de la altura del sol; el crepusculo va de +1 a -11 grados (0.017 a -0.19)
            float sinEl = a + b * cosHa[x];
            int t = (int)((0.017f - sinEl) * (16.0f / 0.207f));
            t = t < 0 ? 0 : t > 16 ? 16 : t;
            int k;
            if (land(x, y)) k = (land(x - 1, y) && land(x + 1, y) && land(x, y - 1) && land(x, y + 1)) ? 0 : 1;
            else k = (gridRow || ((x % (MAP_W / 12)) == 0 && x)) ? 3 : 2;
            buf[y * MAP_W + x] = lut[k][t];
        }
    }
    int x, y;
    toXY(dec, slon, x, y);
    disc(buf, x, y, 6, 0xFFD23F, 0xFFF3B0);
    // ortodromica DE - DX (se corta donde da la vuelta al mapa)
    if (s.hasDe && s.hasDx) {
        int px = 0, py = 0;
        for (int i = 0; i <= 64; i++) {
            double la, lo;
            hc::greatCircle(s.deLat, s.deLon, s.dxLat, s.dxLon, i / 64.0, la, lo);
            toXY(la, lo, x, y);
            if (i && abs(x - px) < MAP_W / 2) line(buf, px, py, x, y, rgb(0x3CE68A));
            px = x; py = y;
        }
    }
    for (int i = 0; i < s.nMarkers; i++) {
        toXY(s.markers[i].lat, s.markers[i].lon, x, y);
        disc(buf, x, y, 3, s.markers[i].color, 0x0B1016);
    }
    if (s.hasDx) { toXY(s.dxLat, s.dxLon, x, y); disc(buf, x, y, 4, 0x3CE68A, 0x0B1016); }
    if (s.hasDe) { toXY(s.deLat, s.deLon, x, y); disc(buf, x, y, 5, 0xFFB347, 0x0B1016); }
}

}
