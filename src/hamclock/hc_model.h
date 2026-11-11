/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Reloj de radioaficionado para la Waveshare ESP32-S3-Touch-LCD-7B con los datos de un servidor OpenHamClock
 * (https://github.com/accius/openhamclock, MIT; openhamclock.com o uno propio). Este archivo es C++ puro
 * (modelo, lectura del JSON de la API, sol, rumbos y pases de satelites) y se prueba en el PC: test/test_hamclock.cpp.
 */
#pragma once
#include <ArduinoJson.h>
#include <cstdint>
#include <ctime>

namespace hc {

// ---------- ajustes (Preferences "hamclock") ----------
struct Config {
    char call[12] = "";
    char grid[8] = "";                                   // locator Maidenhead de 4 o 6 caracteres
    char ssid[33] = "";
    char pass[65] = "";
    char server[96] = "https://openhamclock.com";
    char tz[48] = "CET-1CEST,M3.5.0,M10.5.0/3";          // zona horaria POSIX de la hora local
    char rig[64] = "";                                   // CAT: rigctld de Hamlib, "host" o "host:puerto"; vacio = sin radio
};

// ---------- bandas ----------
enum Band : uint8_t { B160, B80, B60, B40, B30, B20, B17, B15, B12, B10, B6, B2, B_OTHER, B_COUNT };
Band bandOf(double mhz);
const char* bandName(Band b);
uint32_t bandColor(Band b);                              // 0xRRGGBB

// ---------- datos ----------
struct Spot {
    char freq[10];                                       // MHz con tres decimales
    char dx[14], de[14], mode[6], time[6];               // time: HHMM (UTC)
    float lat, lon;
    bool hasPos;
    Band band;
};
struct Activation {                                      // POTA o SOTA
    char call[14], ref[14], freq[10], mode[6];
    bool sota;
};
struct BandCond { char name[10], day[6], night[6]; };
struct SatElements {                                     // elementos medios (OMM / TLE)
    char name[12];
    double epochJd, n, ecc, inc, raan, argp, ma;         // n en rad/min; angulos en radianes
};
struct Pass { char name[12]; time_t aos; float maxEl; };

constexpr int MAX_SPOTS = 40, MAX_ACTS = 12, MAX_SATS = 64, MAX_PASSES = 9, KP_HIST = 16;

struct Model {
    // meteorologia espacial (NAN o -1 = sin dato)
    float sfi = -1, kp = -1, bz = 0;
    int ssn = -1, aIndex = -1, wind = -1;
    char xray[8] = "";
    float kpHist[KP_HIST] = {};
    int kpN = 0;
    BandCond bands[4] = {};
    int nBands = 0;
    // DX cluster
    Spot spots[MAX_SPOTS] = {};
    int nSpots = 0;
    // POTA / SOTA
    Activation pota[MAX_ACTS] = {}, sota[MAX_ACTS] = {};
    int nPota = 0, nSota = 0, totalPota = 0, totalSota = 0;
    // satelites
    SatElements sats[MAX_SATS] = {};
    int nSats = 0;
    Pass passes[MAX_PASSES] = {};
    int nPasses = 0;
    // noticias DX (una linea)
    char news[700] = "";
    // enlace
    bool wifi = false;
    int rssi = 0;
    char status[40] = "";                                // ultimo error de red, vacio si todo va bien
    // CAT (rigctld)
    bool rigLink = false;
    double rigHz = 0;
    char rigMode[8] = "";
    // cada parte sube su contador al cambiar: la interfaz solo repinta lo que cambia
    uint32_t revSolar = 0, revSpots = 0, revActs = 0, revPasses = 0, revNews = 0, revLink = 0, revRig = 0;
};

// ---------- API de OpenHamClock: ruta, filtro de ArduinoJson (para no guardar lo que no se usa) y lectura ----------
// /api/solar-indices
void filterSolar(JsonDocument& f);
bool applySolar(Model& m, JsonVariantConst v);
// /api/n0nbh (indice A, rayos X, viento solar y condiciones por banda)
void filterN0nbh(JsonDocument& f);
bool applyN0nbh(Model& m, JsonVariantConst v);
// /api/dxcluster/paths (spots con la posicion del DX)
void filterSpots(JsonDocument& f);
bool applySpots(Model& m, JsonVariantConst v);
// /api/pota/spots y /api/sota/spots
void filterPota(JsonDocument& f);
bool applyPota(Model& m, JsonVariantConst v);
void filterSota(JsonDocument& f);
bool applySota(Model& m, JsonVariantConst v);
// /api/satellites/data
void filterSats(JsonDocument& f);
bool applySats(Model& m, JsonVariantConst v);
// /api/dxnews
void filterNews(JsonDocument& f);
bool applyNews(Model& m, JsonVariantConst v);

// ---------- geografia ----------
bool gridToLatLon(const char* grid, double& lat, double& lon);      // centro del cuadro
void latLonToGrid(double lat, double lon, char out[5]);            // 4 caracteres
double distanceKm(double lat1, double lon1, double lat2, double lon2);
double bearingDeg(double lat1, double lon1, double lat2, double lon2);
// punto intermedio de la ortodromica (f de 0 a 1)
void greatCircle(double lat1, double lon1, double lat2, double lon2, double f, double& lat, double& lon);

// ---------- sol ----------
double julianDate(time_t t);
double gmstRad(double jd);
void sunPosition(time_t t, double& decDeg, double& lonDeg);        // declinacion y longitud del punto subsolar
// orto y ocaso (UTC) del dia de t; false si no sale o no se pone
bool sunRiseSet(time_t t, double lat, double lon, time_t& rise, time_t& set);

// ---------- satelites ----------
bool parseIsoUtc(const char* s, double& jd);                       // "2026-11-10T12:34:56.789"
// elevacion (grados) del satelite visto desde lat / lon
double satElevation(const SatElements& s, double jd, double lat, double lon);
// proximos pases (de todos los satelites) con elevacion maxima >= minEl en las proximas 'hours' horas, por orden
int findPasses(const SatElements* sats, int n, time_t now, double lat, double lon, Pass* out, int max,
               double minEl = 10, int hours = 12);

}
