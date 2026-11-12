/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "tiles.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace plotter {

void lonLatToWorldPx(LatLon p, int zoom, double& px, double& py) {
    double lat = p.lat;
    if (lat > kMercatorMaxLat) lat = kMercatorMaxLat;
    if (lat < -kMercatorMaxLat) lat = -kMercatorMaxLat;
    double n = double(kTileSize) * double(1u << zoom);
    px = (p.lon + 180.0) / 360.0 * n;
    double s = std::sin(deg2rad(lat));
    py = (0.5 - std::log((1 + s) / (1 - s)) / (4 * kPi)) * n;
}

LatLon worldPxToLonLat(double px, double py, int zoom) {
    double n = double(kTileSize) * double(1u << zoom);
    double lon = px / n * 360.0 - 180.0;
    double y = 0.5 - py / n;
    double lat = 90.0 - 360.0 * std::atan(std::exp(-y * 2 * kPi)) / kPi;
    return {lat, lon};
}

void lonLatToTile(LatLon p, int zoom, int& tx, int& ty, int* inX, int* inY) {
    double px, py;
    lonLatToWorldPx(p, zoom, px, py);
    int max = (1 << zoom) - 1;
    tx = int(std::floor(px / kTileSize));
    ty = int(std::floor(py / kTileSize));
    if (tx < 0) tx = 0;
    if (tx > max) tx = max;
    if (ty < 0) ty = 0;
    if (ty > max) ty = max;
    if (inX) *inX = int(px) - tx * kTileSize;
    if (inY) *inY = int(py) - ty * kTileSize;
}

double metersPerPixel(double lat, int zoom) {
    return 2 * kPi * 6378137.0 * std::cos(deg2rad(lat)) / (double(kTileSize) * double(1u << zoom));
}

int zoomForWidth(double lat, double widthNm, int widthPx, int maxZoom) {
    for (int z = maxZoom; z > 0; z--)
        if (metersPerPixel(lat, z) * widthPx >= widthNm * kMetersPerNm) return z;
    return 0;
}

int tileUrl(char* out, int size, const char* t, int zoom, int tx, int ty) {
    if (!t || !std::strstr(t, "{x}")) return 0;
    int n = 0;
    for (const char* p = t; *p;) {
        char num[16];
        const char* rep = nullptr;
        int skip = 0;
        if (!std::strncmp(p, "{z}", 3)) { std::snprintf(num, sizeof(num), "%d", zoom); rep = num; skip = 3; }
        else if (!std::strncmp(p, "{x}", 3)) { std::snprintf(num, sizeof(num), "%d", tx); rep = num; skip = 3; }
        else if (!std::strncmp(p, "{y}", 3)) { std::snprintf(num, sizeof(num), "%d", ty); rep = num; skip = 3; }
        else if (!std::strncmp(p, "{-y}", 4)) { std::snprintf(num, sizeof(num), "%d", tmsY(ty, zoom)); rep = num; skip = 4; }
        if (rep) {
            int l = int(std::strlen(rep));
            if (n + l >= size) return 0;
            std::memcpy(out + n, rep, size_t(l));
            n += l;
            p += skip;
        } else {
            if (n + 1 >= size) return 0;
            out[n++] = *p++;
        }
    }
    out[n] = 0;
    return n;
}

void tileBounds3857(int zoom, int tx, int ty, double& minX, double& minY, double& maxX, double& maxY) {
    const double half = kPi * 6378137.0;                      // 20037508,34 m
    double span = 2 * half / double(1u << zoom);
    minX = -half + tx * span;
    maxX = minX + span;
    maxY = half - ty * span;
    minY = maxY - span;
}

int wmsTileUrl(char* out, int size, const char* base, const char* layers, int zoom, int tx, int ty, bool transparent, const char* format) {
    if (!base || !layers || !format) return 0;
    double x0, y0, x1, y1;
    tileBounds3857(zoom, tx, ty, x0, y0, x1, y1);
    char fmt[32];
    int k = 0;
    for (const char* p = format; *p && k < int(sizeof(fmt)) - 4; p++) {      // "image/png" -> "image%2Fpng"
        if (*p == '/') { fmt[k++] = '%'; fmt[k++] = '2'; fmt[k++] = 'F'; }
        else fmt[k++] = *p;
    }
    fmt[k] = 0;
    int n = std::snprintf(out, size_t(size),
                          "%s%cSERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&LAYERS=%s&STYLES=&CRS=EPSG:3857&BBOX=%.2f,%.2f,%.2f,%.2f"
                          "&WIDTH=%d&HEIGHT=%d&FORMAT=%s&TRANSPARENT=%s",
                          base, std::strchr(base, '?') ? '&' : '?', layers, x0, y0, x1, y1, kTileSize, kTileSize, fmt,
                          transparent ? "TRUE" : "FALSE");
    return (n > 0 && n < size) ? n : 0;
}

int wmsFeatureInfoUrl(char* out, int size, const char* base, const char* layers, LatLon p, int zoom, const char* infoFormat, int count) {
    if (!base || !layers || !infoFormat) return 0;
    int tx, ty, i, j;
    lonLatToTile(p, zoom, tx, ty, &i, &j);
    double x0, y0, x1, y1;
    tileBounds3857(zoom, tx, ty, x0, y0, x1, y1);
    char fmt[40];
    int k = 0;
    for (const char* q = infoFormat; *q && k < int(sizeof(fmt)) - 4; q++) {
        if (*q == '/') { fmt[k++] = '%'; fmt[k++] = '2'; fmt[k++] = 'F'; }
        else if (*q == '+') { fmt[k++] = '%'; fmt[k++] = '2'; fmt[k++] = 'B'; }
        else fmt[k++] = *q;
    }
    fmt[k] = 0;
    int n = std::snprintf(out, size_t(size),
                          "%s%cSERVICE=WMS&VERSION=1.3.0&REQUEST=GetFeatureInfo&LAYERS=%s&QUERY_LAYERS=%s&STYLES=&CRS=EPSG:3857"
                          "&BBOX=%.2f,%.2f,%.2f,%.2f&WIDTH=%d&HEIGHT=%d&I=%d&J=%d&INFO_FORMAT=%s&FEATURE_COUNT=%d",
                          base, std::strchr(base, '?') ? '&' : '?', layers, layers, x0, y0, x1, y1, kTileSize, kTileSize, i, j, fmt, count);
    return (n > 0 && n < size) ? n : 0;
}

int tilePath(char* out, int size, const char* folder, int zoom, int tx, int ty, const char* ext) {
    int n = std::snprintf(out, size_t(size), "%s/%d/%d/%d.%s", folder ? folder : "", zoom, tx, ty, ext ? ext : "png");
    return (n > 0 && n < size) ? n : 0;
}

}  // namespace plotter
