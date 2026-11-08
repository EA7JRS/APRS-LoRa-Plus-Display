/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Protocolo de control BLE del tracker APRS LoRa+ (docs/BLE_CONTROL.md del firmware): C++ puro, sin Arduino, para probarlo en el PC.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace proto {

struct Info {
    bool ok = false;
    std::string fw, board;
    int mode = 0;
    bool capOta = false, capRotor = false, capNmea = false;
    std::vector<std::string> modeNames;     // nombre por posicion
    std::vector<int> modeIds;
};

struct Status { bool ok = false; int mode = 0, profile = 0, lora = 0; bool digi = false, smart = false, sos = false; float batt = -1; uint32_t up = 0; };

struct Sensors {
    bool ok = false;
    float batt = -1, temp = -999, hum = -1, press = -1, qnh = -1, baroAlt = -999, heading = -1;
    int battPct = -1;
};

struct Gnss { bool ok = false, fix = false; double lat = 0, lon = 0; float alt = 0, spd = 0, crs = 0, hdop = 0; int sats = 0; std::string utc; };

struct Heard {
    std::string call, path, cmt;
    char sym[3] = {0, 0, 0};
    bool hasPos = false;
    double lat = 0, lon = 0;
    int rssi = 0, age = 0, n = 0;
    float snr = 0;
};

struct Msg { int i = 0; char dir = 'R'; uint32_t ep = 0; int st = 0; std::string call, text; };

struct Sat {
    bool ok = false;
    std::string name;
    float az = 0, el = 0, rng = 0;
    uint32_t aos = 0, los = 0;
    int maxEl = 0;
};

struct Rot { bool ok = false, trk = false; float az = 0, el = 0, km = 0; int src = 0; std::string name; };

struct Screen { bool ok = false; std::string title, right; std::vector<std::string> rows; };

struct Page { uint32_t ric = 0; bool own = false; int rssi = 0; std::string text; };

struct Model {
    bool linked = false;
    Info info;
    Status status;
    Sensors sensors;
    Gnss gnss;
    std::vector<Heard> heard;
    int heardTot = 0;
    std::vector<Msg> msgs;
    int msgTot = 0, msgUnread = 0;
    Sat sat;
    Rot rot;
    Screen screen;
    std::vector<Page> pages;
    std::string lastError;
    uint32_t revision = 0;     // sube con cada cambio: la interfaz solo repinta si cambia
};

// Aplica una linea JSON recibida del tracker. Devuelve false si no es JSON valido o no es un evento conocido.
bool applyEvent(Model& m, const char* line);

// Ordenes (una linea JSON terminada en \n)
std::string cmdSimple(const char* c);
std::string cmdSub(int sensorsSec, int gnssSec);
std::string cmdHeard(int from, int cnt);
std::string cmdMsgs(int from, int cnt);
std::string cmdMsgSend(const std::string& to, const std::string& text, bool ack);
std::string cmdMode(int mode);
std::string cmdProfile(int i);
std::string cmdLora(int i);
std::string cmdScreen(int every);
std::string cmdSatSel(int i);
std::string cmdAct(const std::string& name);

// Recompone lineas a partir de trozos de notificaciones (el \n marca el fin).
class LineBuffer {
  public:
    // Anade bytes; cada linea completa se entrega a 'out'.
    template <class F> void feed(const uint8_t* d, size_t n, F out) {
        for (size_t i = 0; i < n; i++) {
            if (d[i] == '\n') { if (!buf_.empty()) out(buf_); buf_.clear(); }
            else if (d[i] != '\r') { if (buf_.size() < 4096) buf_.push_back((char)d[i]); else buf_.clear(); }
        }
    }
    void reset() { buf_.clear(); }
  private:
    std::string buf_;
};

} // namespace proto
