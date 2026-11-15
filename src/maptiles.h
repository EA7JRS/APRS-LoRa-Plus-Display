/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Mapa de teselas de la interfaz tipo emisora (pantalla MAP): capas como las de la app Android (MapScreen.kt), cuentas de
// Web Mercator, decodificacion PNG / JPEG a RGB565 con las librerias que trae LVGL (lodepng, tjpgd) y cache en memoria.
// No sabe de red ni de tarjetas: el que baja y guarda las teselas se lo pasa en Io (net.cpp en la placa).
#pragma once
#ifdef UI_RADIO
#include <lvgl.h>
#include <cstdint>
#include <string>
#include <vector>

namespace mapt {

constexpr int TILE = 256;

// Capas base. Cada servidor tiene sus condiciones: OpenStreetMap (User-Agent propio, sin descargas masivas, atribucion),
// IGN (CC BY 4.0, atribucion), Esri World Imagery (solo con la clave ArcGIS del usuario).
enum LayerId { OSM, IGN, PNOA, DARK, ESRI, LAYER_COUNT };
struct Layer { const char* id; const char* name; const char* attribution; int maxZoom; bool needsKey; };
const Layer& layer(int id);
std::string url(int layerId, int z, int x, int y, const std::string& arcgisKey);
std::string seaUrl(int z, int x, int y);                         // OpenSeaMap: marcas nauticas, transparente, va encima
std::string path(const char* layerDir, int z, int x, int y);     // en la tarjeta: /maps/<capa>/<z>/<x>/<y>.bin
const char* dirOf(int layerId);                                  // DARK comparte las teselas de OSM

// Web Mercator: puntos del mundo a un zoom (0..256*2^z)
void toWorld(double lat, double lon, int z, double& x, double& y);
void toLatLon(double x, double y, int z, double& lat, double& lon);
double metersPerPixel(double lat, int z);

// Imagen PNG o JPEG (se distingue por la cabecera) de 256 x 256 a RGB565. invert = mapa oscuro.
bool decode(const uint8_t* data, size_t n, uint16_t* out, bool invert);
// PNG con transparencia mezclado encima de una tesela ya decodificada
bool blend(const uint8_t* png, size_t n, uint16_t* io);

// ---- cache y peticiones (lo usan la pantalla y la tarea que baja; con cerrojo) ----
struct Key {
    uint8_t layer = 0, z = 0;
    bool sea = false;
    uint32_t x = 0, y = 0;
    bool operator==(const Key& o) const { return layer == o.layer && z == o.z && sea == o.sea && x == o.x && y == o.y; }
};
struct Io {
    void* (*alloc)(size_t);                                                       // memoria de las teselas (PSRAM)
    bool (*online)();                                                             // hay WiFi
    int (*http)(const std::string& url, std::vector<uint8_t>& out);               // codigo HTTP (200 = bien), 0 = sin conexion
    bool (*load)(const std::string& path, std::vector<uint8_t>& out);             // de la tarjeta
    void (*save)(const std::string& path, const uint8_t* data, size_t n);         // a la tarjeta
    uint32_t (*ms)();
};
void begin(const Io& io, int tiles, const std::string& arcgisKey);
void setKey(const std::string& arcgisKey);
// La tesela, si esta en memoria (vale hasta el siguiente dibujo); si no, queda pedida y devuelve nullptr.
const lv_image_dsc_t* get(const Key& k);
bool work();                      // baja o lee una tesela pendiente; false si no hay nada que hacer
uint32_t seq();                   // cambia cuando llega una tesela (hay que repintar)
int pending();
void forget();                    // olvida lo pedido y los fallos (al cambiar de capa)

} // namespace mapt
#endif
