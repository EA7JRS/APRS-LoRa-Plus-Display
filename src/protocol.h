/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
// Protocolo de control BLE del tracker APRS LoRa+ (docs/BLE_CONTROL.md del firmware): C++ puro, sin Arduino, para probarlo en el PC.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "alerts.h"      // lib/plotter_core: modelo de los avisos oficiales

namespace proto {

constexpr float NA = -999.0f;              // sin dato

struct Profile { std::string call, label; };
struct LoraProfile { uint32_t f = 0; int sf = 0, cr = 0; bool fsk = false; };
struct Sa868 {                              // radio FM de la LilyGO T-TWR Plus
    bool present = false, ok = false, spkEsp = false, txul = false, txulav = false;
    int rev = 0;
    std::string fw, band, modem, mac;
    uint32_t rx = 0, tx = 0, m17 = 0, dapnet = 0;
};

struct Info {
    bool ok = false;
    int v = 0;
    std::string fw, board;
    int mode = 0;
    bool capOta = false, capRotor = false, capNmea = false, capCw = false, capDapnet = false, capRkey = false, capGpsaid = false;
    bool capM17 = false, capFm = false, capSatFav = false, capSatFm = false, capWxnet = false, capAis = false, capNetreq = false;
    std::string radio = "lora", bcast = "none";
    int satMax = 64000;
    std::vector<std::string> modeNames;     // nombre por posicion
    std::vector<int> modeIds;
    int profile = 0, lora = 0;
    std::vector<Profile> profiles;
    std::vector<LoraProfile> loras;
    std::vector<std::string> cwMsgs;
    Sa868 sa868;
    std::string rst, rstAt, rstTask, pmuOff;
};

struct Status {
    bool ok = false;
    int mode = 0, profile = 0, lora = 0;
    bool digi = false, smart = false, sos = false, bleKiss = true;
    float batt = -1;
    uint32_t up = 0;
    uint32_t f = 0;                         // Hz de la radio en el modo en curso (0 = sin dato)
    std::string mod;                        // LoRa, FSK, AFSK, CW, POCSAG, M17, FM, WSPR, FT8, SAT, AIS...
};

// Tiempo de internet que la app manda al tracker (OpenWeatherMap / IQAir); llega dentro del evento sensors ("net")
struct NetWx {
    bool ok = false;
    std::string src, place, desc, pollutant, forecast;     // forecast: pronostico del tracker con estos datos ("fc")
    float t = NA, h = -1, p = -1, ws = -1, wg = -1, rain = -1, pm25 = -1, pm10 = -1;
    int wd = -1, clouds = -1, aqiOwm = 0, aqiUs = -1;
    float d3 = NA;                          // cambio de presion en 3 h (hPa)
    uint32_t age = 0;
};
struct Sensors {
    bool ok = false;
    float batt = -1, temp = NA, hum = -1, press = -1, qnh = -1, baroAlt = NA, heading = -1;
    float imuTemp = NA, boardTemp = NA, gas = -1, iaq = -1, vbus = -1, pmuTemp = NA, battMa = NA;
    int battPct = -1, moving = -1, charging = -1, flat = -1, magCal = -1;
    uint32_t heap = 0;
    bool hasAccel = false, hasGyro = false;
    float accel[3] = {0, 0, 0}, gyro[3] = {0, 0, 0};
    std::string qnhSrc, iaqLabel, wxSensor, chg;
    NetWx net;
};

struct Gnss { bool ok = false, fix = false; double lat = 0, lon = 0; float alt = 0, spd = 0, crs = 0, hdop = 0; int sats = 0; uint32_t age = 0; std::string utc; };

struct SkySat { char sys = 'N'; int prn = 0, el = -1, az = -1, snr = -1; bool used = false; };
struct Sky { bool ok = false; float pd = -1, hd = -1, vd = -1, err = -1; int used = -1; std::vector<SkySat> sats; };

struct HeardWx { bool ok = false; float t = NA, h = NA, p = NA, wd = NA, ws = NA, wg = NA, r = NA; };
struct Heard {
    std::string call, path, cmt, kind, dev, sts;
    char sym[3] = {0, 0, 0};                // tal cual llega: codigo y tabla ("&L", ">/")
    bool hasPos = false;
    double lat = 0, lon = 0;
    int alt = 0, spd = 0, crs = 0;
    int rssi = 0, age = 0, n = 0, pa = -1;  // age / pa: segundos desde el ultimo paquete / la ultima posicion (al llegar)
    float snr = 0;
    HeardWx wx;
    char symTable() const { return sym[1] ? sym[1] : '/'; }
    char symCode() const { return sym[0] ? sym[0] : '/'; }
};

struct Msg { int i = 0; char dir = 'R'; uint32_t ep = 0; int st = 0, ack = 0; std::string call, text; };

struct Sat {
    bool ok = false, geo = false, fav = false;
    std::string name, pos, stamp;
    int id = 0, i = -1, tx = 0, cat = 0, tot = 0, nfav = 0;
    float az = 0, el = 0, rng = 0, rr = 0, tle = -1;
    uint32_t aos = 0, los = 0;
    int maxEl = 0;
};
struct SatEntry { int i = 0, id = 0, t = 0, b = 0; std::string name, c; bool fav = false, in = true; };
struct SatTx { int i = 0, b = 0; std::string m, d, rx, tx; uint32_t up = 0, dn = 0, rxd = 0, txd = 0; float tone = 0; bool aprs = false; };
struct SatRx { bool on = false; std::string k; uint32_t f = 0; int dop = 0, fine = 0, rssi = 0, n = 0, tx = 0; std::vector<std::string> lines; };

struct Rot {
    bool ok = false, trk = false, mob = false, hasAct = false;
    float az = 0, el = 0, km = 0, aaz = 0, ael = 0, abs = 0, hdg = 0;
    int src = 0;
    std::string name;
};
struct Target { int i = 0; std::string n; double lat = 0, lon = 0; int alt = 0; uint32_t f = 0; };
struct Targets { bool ok = false; int sel = -1, src = 0, fm = 0, pm = 0; bool pr = false; std::string ff; std::vector<Target> items; };

struct Screen { bool ok = false, on = true; int p = 0, cur = 0; std::string title, right; std::vector<std::string> rows; };

struct Page { uint32_t ric = 0; int func = 0, rssi = 0; uint32_t age = 0; bool own = false, bad = false; std::string text; };

// Luces de la banda A y alertas (eventos rf, led, eai)
struct Leds { bool ok = false, msg = false, emg = false, flash = false, em = false, eo = false; int unread = 0, eq = 0; std::string ec, et, ek; };
struct Rf { uint32_t txSinceMs = 0, rxAtMs = 0; int rssi = 0; float snr = 0; };   // 0 = nunca
struct Eai { bool ok = false, fm = false, md = false; int sos = 0, sent = 0, left = 0, fmLeft = 0, fmMode = 0, mdLeft = 0, mdMin = 0; std::string by, fmBy; };
struct CwRx { bool on = false; std::string text; float dbm = NA; int snr = 0, wpm = 0; uint32_t f = 0; };

// Modo FM de la T-TWR (evento fm) y sus memorias
struct FmSat { bool ok = false, aprs = false, txok = false, xup = false; std::string n, why; uint32_t f = 0, up = 0; int dop = 0, tone = 0, sent = 0, heard = 0; float az = 0, el = 0; };
struct Fm {
    bool ok = false, inv = false, w = true, hp = false, m = false, busy = false, ptt = false, aprs = false, qsyb = false, tok = true, spk = false, sqa = false;
    uint32_t f = 0, tx = 0;
    int sh = 0, ofs = 0, tm = 0, t = 0, d = 0, n = -1, i = 0, step = 0, sq = 0, sqm = 0, nr = 0, edit = 0, txs = 0, rssi = 0, scan = 0, arts = 0, eai = 0;
    std::string note;
    FmSat sat;
};
struct FmMem { int i = 0, sh = 0, ofs = 0, tm = 0, t = 0, d = 0; uint32_t f = 0; bool inv = false, w = true, hp = false; std::string n; };

// Radio comercial por I2C (RDA5807M / Si4703 / Si4735)
struct Bcast {
    bool ok = false, on = false, mute = false, mono = false, st = false, seek = false;
    std::string chip, ps, rt;
    int band = 0, vol = 0, rssi = 0, snr = 0;
    uint32_t f = 0;
    std::vector<int> bands;
    std::vector<uint32_t> presets;
};

// Barcos AIS (modo 12)
struct Vessel {
    uint32_t mmsi = 0;
    std::string cl, name, call, dest, eta, txt;
    int type = 0, st = 0, hdg = -1, pa = -1, age = 0, brg = -1, len = 0, beam = 0, imo = 0, alt = -1;
    bool hasPos = false, danger = false;
    double lat = 0, lon = 0;
    float sog = -1, cog = -1, nm = -1, cpa = -1, tcpa = -1, draught = -1;
};

struct Action { std::string n, l; bool ok = true; };
struct ActionPage { int p = 0; std::string title; std::vector<Action> items; };

struct Model {
    uint32_t now = 0;           // reloj del llamador en ms (millis()): lo pone antes de applyEvent para fechar lo que llega
    bool linked = false;
    Info info;
    Status status;
    Sensors sensors;
    Gnss gnss;
    Sky sky;
    std::vector<Heard> heard;   // todas las estaciones (las paginas de 'heard' se juntan por 'from'); 0 = la mas reciente
    int heardTot = 0;
    uint32_t heardPkts = 0, heardAtMs = 0;
    std::vector<Msg> msgs;      // registro de mensajes del tracker (paginas juntadas por 'from'); 0 = el mas reciente
    int msgTot = 0, msgUnread = 0;
    uint32_t msgSeq = 0;        // sube con cada evento 'msg' (cambio en el registro: hay que volver a pedirlo)
    Sat sat;
    std::vector<SatEntry> satList;
    int satListTot = 0, satListSel = -1;
    std::vector<SatTx> satTxs;
    SatRx satRx;
    Rot rot;
    Targets targets;
    Screen screen;
    std::vector<Page> pages;
    Leds leds;
    Rf rf;
    Eai eai;
    CwRx cwrx;
    Fm fm;
    std::vector<FmMem> fmMems;
    int fmMemMax = 32;
    Bcast bcast;
    std::vector<Vessel> ais;    // por distancia (aislist) y actualizados por 'ais'
    int aisTot = 0, aisMsgs = 0;
    std::string aisSrc, aisCh;
    std::vector<ActionPage> actions;
    bool actionsDone = false;
    // configuracion (cfgget): se junta por desplazamiento
    std::string cfgText;
    uint32_t cfgTotal = 0;
    bool cfgDone = false;
    // actualizacion por BLE
    uint32_t otaOff = 0;
    bool otaDone = false;
    std::string netReq;         // ultimo 'netreq' ("wx" o "sat"); lo vacia quien lo atiende
    std::string lastOk, lastError, lastErrorCmd;
    uint32_t okSeq = 0, errSeq = 0;
    uint32_t revision = 0;      // sube con cada cambio: la interfaz solo repinta si cambia
    // Avisos oficiales del tracker (eventos "alertsv", "alerts", "spacewx"; los descarga la app con su internet o los decodifica la T-TWR)
    std::vector<plotter::Alert> alerts;
    uint32_t alertsVer = 0;        // version de la lista que se tiene
    uint32_t alertsVerTracker = 0; // la ultima que anuncio el tracker (si es otra, hay que pedir "alerts")
    int alertsTot = 0;
    bool alertsPending = false;    // faltan paginas
    plotter::SpaceWx space;
    bool spaceOk = false;
};

// Aplica una linea JSON recibida del tracker. Devuelve false si no es JSON valido o no es un evento conocido.
bool applyEvent(Model& m, const char* line);

// Orden con campos: proto::Cmd("fmset").num("f", 145500000).flag("w", true).str()
class Cmd {
  public:
    explicit Cmd(const char* c);
    ~Cmd();
    Cmd(const Cmd&) = delete;
    Cmd& operator=(const Cmd&) = delete;
    Cmd& num(const char* k, double v);
    Cmd& num(const char* k, int v);
    Cmd& num(const char* k, uint32_t v);
    Cmd& flag(const char* k, bool v);
    Cmd& text(const char* k, const std::string& v);
    std::string str() const;                // la linea terminada en \n
    operator std::string() const { return str(); }
  private:
    void* doc_;
};

// Ordenes de siempre
std::string cmdSimple(const char* c);
std::string cmdSub(int sensorsSec, int gnssSec, int skySec = 0);
std::string cmdHeard(int from, int cnt);
std::string cmdMsgs(int from, int cnt);
std::string cmdMsgSend(const std::string& to, const std::string& text, bool ack);
std::string cmdMode(int mode);
std::string cmdProfile(int i);
std::string cmdLora(int i);
std::string cmdScreen(int every, int page = -1);
std::string cmdSatSel(int i);
std::string cmdAct(const std::string& name);
// Texto largo por trozos (cfgset, satdata): begin, trozos de <= 180 caracteres sin cortar UTF-8, end. Se manda uno y se espera el 'ok'.
std::vector<std::string> chunked(const char* name, const std::string& text, bool reboot = true);

// Utilidades de la interfaz
const char* msgStateName(int st);
std::string modeName(const Model& m, int id);
// Distancia (km) y rumbo (grados) entre dos puntos
double distanceKm(double la1, double lo1, double la2, double lo2);
double bearingDeg(double la1, double lo1, double la2, double lo2);
const char* cardinal16(double deg);
// S-meter IARU: S9 = -73 dBm por debajo de 30 MHz, -93 dBm desde 30 MHz, 6 dB por unidad. segments 0..15 (9 S + 6 de 10 dB)
struct SReading { int s = 0, overDb = 0, segments = 0; std::string label; };
SReading sMeter(double dbm, uint32_t freqHz);
// "433.775" y "000" de una frecuencia en Hz
std::string mhz(uint32_t hz);
std::string subKhz(uint32_t hz);
// Locator Maidenhead de 6 caracteres
std::string locator(double lat, double lon);
std::string cmdAlerts(int from, int cnt);

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
