/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Protocolo JSON de la caja APRS LoRa+ Rotor (docs/CONTROL_PROTOCOL.md del repositorio del rotor): C++ puro, probado en el PC.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rotp {

struct Wind { bool ok = false, on = false, locked = false, local = false; float park = 0, ws = -1, wg = -1, peak = -1; int wd = -1, age = 0, hold = 0; std::string src; };

struct Info {
    bool ok = false, hasEl = false, pin = false, cat = false, wind = false, capCal = false, capMove = false;
    float minAz = 0, maxAz = 360, minEl = 0, maxEl = 90;
    std::string dev, ver, board, name, drv, call, ip;
    int rotctld = 0, rigctld = 0;
    uint32_t up = 0;
};
struct Pos {
    bool ok = false, hasEl = false, moving = false, hasTarget = false;
    float az = 0, el = 0, maz = 0, taz = 0, tel = 0;
    int st = 0;
    std::string fault = "OK", aprs;
    Wind wind;
};
struct Cat {
    bool ok = false, link = false, split = false, ptt = false, pttAllowed = false, trackEn = false, trackOn = false;
    std::string rig, mode, vfo = "A", trackName;
    uint32_t f = 0, tf = 0;
    int sm = -1, dop = 0;
    float tone = 0;
};
struct Raw { bool ok = false, link = false, hasAz = false, hasEl = false; int az = 0, el = 0; std::string unit; };

struct Model {
    Info info;
    Pos pos;
    Cat cat;
    Raw raw;
    bool authOk = false, authFailed = false;
    std::string cfgText;        // trozos de 'cfg' juntados
    int cfgN = 0, cfgGot = 0;
    bool cfgDone = false;
    std::string lastError;
    uint32_t errSeq = 0, okSeq = 0, revision = 0;
};

bool apply(Model& m, const char* line);

std::string cmd(const char* c);                                     // {"c":"info"}
std::string auth(const std::string& pin);
std::string sub(int ms);
std::string goTo(float az, float el, bool withEl);
std::string move(const char* dir);                                  // cw ccw up down
std::string catSet(const char* key, double v);                      // f txf tone
std::string catSetText(const char* key, const std::string& v);      // m vfo
std::string catSetFlag(const char* key, bool v);                    // split ptt
std::string catTrack(bool on, const std::string& name, uint32_t dl, uint32_t ul, double rr);
std::string wx(float ws, float wg, int wd, int age, const std::string& src);
std::string cal(const char* axis, const char* pt, float deg);
std::string here(const char* axis, float deg);
// Configuracion por trozos (<= 180 caracteres sin cortar UTF-8): {"c":"cfgset","i":k,"n":N,"d":"..."}
std::vector<std::string> cfgSet(const std::string& json);

// Modo de Hamlib para un transmisor de satelite (FM, USB, CW, PKTUSB...)
std::string satMode(const std::string& mode);
// S-meter de la caja (0..9, 10 = S9+10...) como texto
std::string sLabel(int sm);

} // namespace rotp
