/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Teselas de mapa (Web Mercator, 256 px): paso de posicion a tesela y pixel, y direcciones de descarga a partir de una
// plantilla con {z}, {x}, {y} (numeracion XYZ, origen arriba) o {-y} (numeracion TMS, origen abajo, como las del IGN).
#ifndef PLOTTER_TILES_H_
#define PLOTTER_TILES_H_

#include <cstdint>

#include "geo.h"

namespace plotter {

constexpr int kTileSize = 256;
constexpr double kMercatorMaxLat = 85.0511287798;

// Posicion -> coordenada de pixel en el mundo a ese zoom (0 .. 256 * 2^z)
void lonLatToWorldPx(LatLon p, int zoom, double& px, double& py);
LatLon worldPxToLonLat(double px, double py, int zoom);
// Tesela que contiene la posicion (XYZ) y el pixel dentro de ella
void lonLatToTile(LatLon p, int zoom, int& tx, int& ty, int* inX = nullptr, int* inY = nullptr);
inline int tmsY(int ty, int zoom) { return (1 << zoom) - 1 - ty; }
// Metros por pixel a esa latitud y zoom
double metersPerPixel(double latDeg, int zoom);
// Zoom mas alto en el que "widthNm" millas caben en "widthPx" pixeles a esa latitud
int zoomForWidth(double latDeg, double widthNm, int widthPx, int maxZoom = 18);

// Sustituye {z}, {x}, {y} y {-y} en la plantilla. Devuelve la longitud, 0 si no cabe o la plantilla no lleva {x}.
int tileUrl(char* out, int size, const char* tmpl, int zoom, int tx, int ty);
// Ruta en la microSD: "<carpeta>/<z>/<x>/<y>.<ext>"
int tilePath(char* out, int size, const char* folder, int zoom, int tx, int ty, const char* ext);

// Servicios WMS (IHM, Portus...): no dan teselas, pero se les puede pedir la imagen de cada tesela por su recuadro en
// Web Mercator (EPSG:3857). "base" es la direccion del servicio sin parametros; "layers", los nombres de capa separados por comas.
void tileBounds3857(int zoom, int tx, int ty, double& minX, double& minY, double& maxX, double& maxY);   // metros
int wmsTileUrl(char* out, int size, const char* base, const char* layers, int zoom, int tx, int ty, bool transparent = true,
               const char* format = "image/png");
// Consulta de lo que hay en un punto (GetFeatureInfo): la tesela que lo contiene y el pixel dentro de ella. Sirve para
// pedir la ficha de una luz o de una estacion de mareas al tocarla en la carta.
int wmsFeatureInfoUrl(char* out, int size, const char* base, const char* layers, LatLon p, int zoom,
                      const char* infoFormat = "text/plain", int featureCount = 5);
// Servicios del Instituto Hidrografico de la Marina (los nombres de capa se leen del GetCapabilities de cada uno)
constexpr const char* kWmsIhmEnc = "https://ideihm.covam.es/wms/enc";
constexpr const char* kWmsIhmPaperCharts = "https://ideihm.covam.es/wms/CartaOF";
constexpr const char* kWmsIhmLights = "https://ideihm.covam.es/wms/lucesIHM";
constexpr const char* kWmsIhmTides = "https://ideihm.covam.es/wms/mareas";
constexpr const char* kWmsIhmMaritimeLimits = "https://ideihm.covam.es/ihm-inspire/wms-unidadesmaritimas";

// Fuentes comprobadas en el codigo de la API-CNIG del IGN (teselas JPEG / PNG ya hechas, numeracion TMS)
constexpr const char* kTilesIgnBase = "https://tms-ign-base.idee.es/1.0.0/IGNBaseTodo/{z}/{x}/{-y}.jpeg";
constexpr const char* kTilesPnoa = "https://tms-pnoa-ma.idee.es/1.0.0/pnoa-ma/{z}/{x}/{-y}.jpeg";

}  // namespace plotter
#endif
