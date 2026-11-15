/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Mapa de teselas (ver maptiles.h).
 */
#ifdef UI_RADIO
#include "maptiles.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#define LODEPNG_NO_COMPILE_CPP                               // lodepng.c se compila como C: solo su parte C
extern "C" {
#include <src/libs/lodepng/lodepng.h>
#include <src/libs/tjpgd/tjpgd.h>
}

namespace mapt {

static const Layer LAYERS[LAYER_COUNT] = {
    {"osm", "OSM", "\xC2\xA9 OpenStreetMap contributors", 19, false},
    {"ign", "IGN TOPO", "CC BY 4.0 ign.es", 17, false},
    {"pnoa", "IGN PHOTO", "PNOA CC BY 4.0 ign.es", 19, false},
    {"osm", "DARK", "\xC2\xA9 OpenStreetMap contributors", 19, false},
    {"esri", "ESRI", "Esri, Maxar, Earthstar Geographics", 19, true},
};
const Layer& layer(int id) { return LAYERS[id < 0 || id >= LAYER_COUNT ? 0 : id]; }
const char* dirOf(int id) { return layer(id).id; }

std::string url(int id, int z, int x, int y, const std::string& key) {
    char b[320];
    switch (id) {
    case IGN:
    case PNOA:
        snprintf(b, sizeof b, "https://www.ign.es/wmts/%s?layer=%s&style=default&tilematrixset=GoogleMapsCompatible&Service=WMTS&Request=GetTile"
                 "&Version=1.0.0&Format=image/jpeg&TileMatrix=%d&TileCol=%d&TileRow=%d",
                 id == IGN ? "mapa-raster" : "pnoa-ma", id == IGN ? "MTN" : "OI.OrthoimageCoverage", z, x, y);
        break;
    case ESRI:
        if (key.empty()) return "";
        snprintf(b, sizeof b, "https://ibasemaps-api.arcgis.com/arcgis/rest/services/World_Imagery/MapServer/tile/%d/%d/%d?token=%s", z, y, x, key.c_str());
        break;
    default:
        snprintf(b, sizeof b, "https://tile.openstreetmap.org/%d/%d/%d.png", z, x, y);
    }
    return b;
}
std::string seaUrl(int z, int x, int y) {
    char b[96];
    snprintf(b, sizeof b, "https://tiles.openseamap.org/seamark/%d/%d/%d.png", z, x, y);
    return b;
}
std::string path(const char* dir, int z, int x, int y) {
    char b[80];
    snprintf(b, sizeof b, "/maps/%s/%d/%d/%d.bin", dir, z, x, y);
    return b;
}

void toWorld(double lat, double lon, int z, double& x, double& y) {
    if (lat > 85.05112878) lat = 85.05112878;
    if (lat < -85.05112878) lat = -85.05112878;
    const double n = TILE * (double)(1u << z), r = lat * M_PI / 180;
    x = (lon + 180.0) / 360.0 * n;
    y = (1.0 - log(tan(r) + 1.0 / cos(r)) / M_PI) / 2.0 * n;
}
void toLatLon(double x, double y, int z, double& lat, double& lon) {
    const double n = TILE * (double)(1u << z);
    lon = x / n * 360.0 - 180.0;
    lat = atan(sinh(M_PI * (1.0 - 2.0 * y / n))) * 180.0 / M_PI;
}
double metersPerPixel(double lat, int z) { return 156543.03392 * cos(lat * M_PI / 180) / (double)(1u << z); }

// ------------------------------------------------------------------ decodificar
static inline uint16_t rgb565(unsigned r, unsigned g, unsigned b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
static bool isPng(const uint8_t* d, size_t n) { return n > 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G'; }
static bool isJpeg(const uint8_t* d, size_t n) { return n > 3 && d[0] == 0xFF && d[1] == 0xD8; }

struct JpegIo { const uint8_t* data; size_t n, pos; uint16_t* out; bool invert; };
static size_t jpegIn(JDEC* jd, uint8_t* buf, size_t len) {
    JpegIo* io = (JpegIo*)jd->device;
    if (len > io->n - io->pos) len = io->n - io->pos;
    if (buf) memcpy(buf, io->data + io->pos, len);
    io->pos += len;
    return len;
}
static int jpegOut(JDEC* jd, void* bitmap, JRECT* r) {
    JpegIo* io = (JpegIo*)jd->device;
    const uint8_t* s = (const uint8_t*)bitmap;                   // LVGL deja tjpgd en RGB888... en orden B, G, R
    for (int y = r->top; y <= r->bottom; y++)
        for (int x = r->left; x <= r->right; x++, s += 3) {
            if (x >= TILE || y >= TILE) continue;
            unsigned b = s[0], g = s[1], rr = s[2];
            if (io->invert) { rr = 255 - rr; g = 255 - g; b = 255 - b; }
            io->out[y * TILE + x] = rgb565(rr, g, b);
        }
    return 1;
}

bool decode(const uint8_t* d, size_t n, uint16_t* out, bool invert) {
    if (isPng(d, n)) {
        unsigned char* px = nullptr;                             // el lodepng de LVGL devuelve un lv_draw_buf_t (R, G, B, A)
        unsigned w = 0, h = 0;
        if (lodepng_decode32(&px, &w, &h, d, n) || !px) { if (px) lv_draw_buf_destroy((lv_draw_buf_t*)px); return false; }
        lv_draw_buf_t* db = (lv_draw_buf_t*)px;
        if (w != TILE || h != TILE) { lv_draw_buf_destroy(db); return false; }
        const uint8_t* s = db->data;
        for (int i = 0; i < TILE * TILE; i++, s += 4) {
            unsigned r = s[0], g = s[1], b = s[2];
            if (invert) {                                         // oscuro: se invierte y se tine hacia el fondo de la app
                r = (255 - r) * 3 / 4 + 5; g = (255 - g) * 3 / 4 + 8; b = (255 - b) * 3 / 4 + 12;
            }
            out[i] = rgb565(r, g, b);
        }
        lv_draw_buf_destroy(db);
        return true;
    }
    if (isJpeg(d, n)) {
        static uint8_t pool[4096];                               // solo decodifica la tarea de las teselas
        JDEC jd;
        JpegIo io = {d, n, 0, out, invert};
        if (jd_prepare(&jd, jpegIn, pool, sizeof pool, &io) != JDR_OK) return false;
        if (jd.width != TILE || jd.height != TILE) return false;
        return jd_decomp(&jd, jpegOut, 0) == JDR_OK;
    }
    return false;
}

bool blend(const uint8_t* d, size_t n, uint16_t* io) {
    if (!isPng(d, n)) return false;
    unsigned char* px = nullptr;
    unsigned w = 0, h = 0;
    if (lodepng_decode32(&px, &w, &h, d, n) || !px) { if (px) lv_draw_buf_destroy((lv_draw_buf_t*)px); return false; }
    lv_draw_buf_t* db = (lv_draw_buf_t*)px;
    if (w == TILE && h == TILE) {
        const uint8_t* s = db->data;
        for (int i = 0; i < TILE * TILE; i++, s += 4) {
            unsigned a = s[3];
            if (!a) continue;
            uint16_t v = io[i];
            unsigned r = (v >> 8) & 0xF8, g = (v >> 3) & 0xFC, b = (v << 3) & 0xF8;
            r = (s[0] * a + r * (255 - a)) / 255; g = (s[1] * a + g * (255 - a)) / 255; b = (s[2] * a + b * (255 - a)) / 255;
            io[i] = rgb565(r, g, b);
        }
    }
    lv_draw_buf_destroy(db);
    return true;
}

// ------------------------------------------------------------------ cache
struct Slot { Key k; bool used = false; uint32_t seen = 0; uint16_t* px = nullptr; lv_image_dsc_t img; };
struct Bad { Key k; uint32_t at; };
static Io io;
static std::vector<Slot> slots;
static std::vector<Key> wanted;                                  // lo ultimo pedido, al final
static std::vector<Bad> bad;                                     // fallos: no se reintentan hasta pasado un rato
static std::string arcgis;
static std::mutex mx;
static volatile uint32_t sequence = 0;
static const uint32_t KEEP_MS = 3000, RETRY_MS = 30000;

void begin(const Io& i, int tiles, const std::string& key) {
    io = i;
    arcgis = key;
    slots.resize(tiles);
    for (Slot& s : slots) {
        s.px = (uint16_t*)io.alloc(TILE * TILE * 2);
        memset(&s.img, 0, sizeof s.img);
        s.img.header.magic = LV_IMAGE_HEADER_MAGIC;
        s.img.header.cf = LV_COLOR_FORMAT_RGB565;
        s.img.header.w = TILE; s.img.header.h = TILE; s.img.header.stride = TILE * 2;
        s.img.data_size = TILE * TILE * 2;
        s.img.data = (const uint8_t*)s.px;
    }
}
void setKey(const std::string& key) { std::lock_guard<std::mutex> l(mx); arcgis = key; }
uint32_t seq() { return sequence; }
int pending() { std::lock_guard<std::mutex> l(mx); return (int)wanted.size(); }
void forget() { std::lock_guard<std::mutex> l(mx); wanted.clear(); bad.clear(); }

const lv_image_dsc_t* get(const Key& k) {
    std::lock_guard<std::mutex> l(mx);
    const uint32_t now = io.ms ? io.ms() : 0;
    for (Slot& s : slots)
        if (s.used && s.k == k) { s.seen = now; return s.px ? &s.img : nullptr; }
    for (size_t i = 0; i < bad.size();) {
        if (now - bad[i].at > RETRY_MS) { bad.erase(bad.begin() + i); continue; }
        if (bad[i].k == k) return nullptr;
        i++;
    }
    for (size_t i = 0; i < wanted.size(); i++)
        if (wanted[i] == k) { wanted.erase(wanted.begin() + i); break; }
    wanted.push_back(k);
    if (wanted.size() > 40) wanted.erase(wanted.begin());
    return nullptr;
}

static void fail(const Key& k) {
    std::lock_guard<std::mutex> l(mx);
    if (bad.size() > 60) bad.erase(bad.begin());
    bad.push_back({k, io.ms ? io.ms() : 0});
    sequence++;
}

// Lee de la tarjeta o baja. empty404: un 404 se guarda como archivo vacio (OpenSeaMap no tiene tesela donde no hay marcas).
static int fetch(const std::string& file, const std::string& link, std::vector<uint8_t>& data, bool empty404) {
    data.clear();
    if (io.load && io.load(file, data)) return data.empty() ? 404 : 200;
    if (link.empty() || !io.online || !io.online()) return 0;
    int code = io.http(link, data);
    if (code == 200 && !data.empty()) { if (io.save) io.save(file, data.data(), data.size()); return 200; }
    data.clear();
    if (code == 404 && empty404 && io.save) io.save(file, nullptr, 0);
    return code;
}

bool work() {
    Key k;
    std::string key;
    {
        std::lock_guard<std::mutex> l(mx);
        if (wanted.empty() || slots.empty()) return false;
        k = wanted.back();
        wanted.pop_back();
        key = arcgis;
    }
    static uint16_t* scratch = nullptr;                          // se decodifica aparte y se copia: la pantalla nunca ve media tesela
    if (!scratch) scratch = (uint16_t*)io.alloc(TILE * TILE * 2);
    if (!scratch) return false;
    std::vector<uint8_t> data;
    int code = fetch(path(dirOf(k.layer), k.z, k.x, k.y), url(k.layer, k.z, k.x, k.y, key), data, false);
    if (code != 200 || !decode(data.data(), data.size(), scratch, k.layer == DARK)) { fail(k); return true; }
    if (k.sea && fetch(path("sea", k.z, k.x, k.y), k.z <= 18 ? seaUrl(k.z, k.x, k.y) : "", data, true) == 200) blend(data.data(), data.size(), scratch);
    std::lock_guard<std::mutex> l(mx);
    const uint32_t now = io.ms ? io.ms() : 0;
    Slot* best = nullptr;
    for (Slot& s : slots) {
        if (!s.px) continue;
        if (!s.used) { best = &s; break; }
        if (now - s.seen < KEEP_MS) continue;                    // a la vista: no se toca
        if (!best || s.seen < best->seen) best = &s;
    }
    if (!best) return true;                                      // todo a la vista: se volvera a pedir
    memcpy(best->px, scratch, TILE * TILE * 2);
    best->k = k; best->used = true; best->seen = now;
    sequence++;
    return true;
}

} // namespace mapt
#endif
