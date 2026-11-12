/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Red de la 7B v2 (ver net.h). Tres tareas: enlace con la caja del rotor (TCP 4534 o BLE), DX Cluster (telnet) y
 * trabajos de internet (tiempo de OpenWeatherMap, firmware del tracker). La interfaz habla con ellas por colas.
 */
#ifdef UI_RADIO
#include "net.h"
#include "protocol.h"
#include "rotor_proto.h"
#include "settings.h"
#include "tracker_link.h"
#include "maptiles.h"
#include <algorithm>
#include <ctime>
#include "board.h"
#include <SD.h>
#include <SPI.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include <deque>
#include <mutex>
#include <ctime>

namespace net {

static std::mutex mx;
static proto::Model* model = nullptr;
void setModel(proto::Model* m) { model = m; }

// ------------------------------------------------------------------ WiFi
static bool wifiStarted = false, ntpStarted = false;
void wifiApply() {
    const cfg::Settings& s = cfg::get();
    WiFi.disconnect(true);
    ntpStarted = false;
    if (s.wifiSsid.empty()) { WiFi.mode(WIFI_OFF); wifiStarted = false; return; }
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("APRS-LoRa-Display");
    WiFi.setAutoReconnect(true);
    WiFi.begin(s.wifiSsid.c_str(), s.wifiPass.c_str());
    wifiStarted = true;
}
Wifi wifi() {
    Wifi w;
    w.configured = !cfg::get().wifiSsid.empty();
    w.connected = wifiStarted && WiFi.status() == WL_CONNECTED;
    w.ssid = cfg::get().wifiSsid;
    if (w.connected) { w.ip = WiFi.localIP().toString().c_str(); w.rssi = WiFi.RSSI(); }
    return w;
}
bool timeValid() { return time(nullptr) > 1700000000; }
static bool online() { return wifiStarted && WiFi.status() == WL_CONNECTED; }

// ------------------------------------------------------------------ caja del rotor
static volatile Link rotSt = Link::Off;
static std::deque<std::string> rotTx, rotRx;
static volatile bool rotRestart = true;
static std::string rotHost, rotPin, rotBle;
static int rotType = 0;
static proto::LineBuffer rotLb;

Link rotorState() { return rotSt; }
const char* rotorStateText() {
    switch (rotSt) {
        case Link::Off: return rotType ? "waiting" : "off";
        case Link::Connecting: return "connecting";
        case Link::Linked: return rotType == 1 ? "linked (WiFi)" : "linked (BLE)";
        default: return "failed";
    }
}
void rotorApply() {
    std::lock_guard<std::mutex> g(mx);
    const cfg::Settings& s = cfg::get();
    rotType = s.rotorLink; rotHost = s.rotorHost; rotPin = s.rotorPin; rotBle = s.rotorBle;
    rotTx.clear();
    rotRestart = true;
}
void rotorSend(const std::string& l) { std::lock_guard<std::mutex> g(mx); if (rotTx.size() < 24) rotTx.push_back(l); }
bool rotorPoll(std::string& l) { std::lock_guard<std::mutex> g(mx); if (rotRx.empty()) return false; l = rotRx.front(); rotRx.pop_front(); return true; }
static void rotPush(const std::string& l) { std::lock_guard<std::mutex> g(mx); if (rotRx.size() < 48) rotRx.push_back(l); }
static bool rotPop(std::string& l) { std::lock_guard<std::mutex> g(mx); if (rotTx.empty()) return false; l = rotTx.front(); rotTx.pop_front(); return true; }
static void rotHello() {
    std::string pin;
    { std::lock_guard<std::mutex> g(mx); pin = rotPin; }
    std::lock_guard<std::mutex> g(mx);
    rotTx.push_front(rotp::cmd("cat"));
    rotTx.push_front(rotp::cmd("get"));
    rotTx.push_front(rotp::sub(500));
    if (!pin.empty()) rotTx.push_front(rotp::auth(pin));
    rotTx.push_front(rotp::cmd("info"));
}

static void rotorTcp() {
    std::string host;
    { std::lock_guard<std::mutex> g(mx); host = rotHost; }
    if (!online()) { rotSt = Link::Off; delay(1000); return; }
    NetworkClient c;
    rotSt = Link::Connecting;
    if (!c.connect(host.c_str(), 4534, 4000)) { rotSt = Link::Failed; delay(5000); return; }
    c.setNoDelay(true);
    rotSt = Link::Linked;
    rotLb.reset();
    rotHello();
    uint8_t buf[256];
    while (c.connected() && !rotRestart) {
        int n = c.available();
        if (n > 0) { n = c.read(buf, n > (int)sizeof buf ? sizeof buf : n); rotLb.feed(buf, n, rotPush); }
        std::string l;
        if (rotPop(l)) c.write((const uint8_t*)l.data(), l.size());
        else delay(10);
    }
    c.stop();
    rotSt = rotRestart ? Link::Off : Link::Failed;
    if (!rotRestart) delay(3000);
}

// BLE: servicio ...-b000-... (el tracker usa ...-a000-...), emparejamiento "Just Works" al leer los eventos cifrados
static const NimBLEUUID ROT_SVC("4c6f5261-2b00-4ea7-b000-000000000001");
static const NimBLEUUID ROT_CMD("4c6f5261-2b00-4ea7-b000-000000000002");
static const NimBLEUUID ROT_EVT("4c6f5261-2b00-4ea7-b000-000000000003");
static volatile bool rotFound = false;
static NimBLEAddress rotAddr;
class RotScan : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* d) override {
        if (rotFound) return;
        std::string want;
        { std::lock_guard<std::mutex> g(mx); want = rotBle; }
        bool ok = want.empty() ? d->isAdvertisingService(ROT_SVC) : strcasecmp(d->getAddress().toString().c_str(), want.c_str()) == 0;
        if (!ok) return;
        rotAddr = d->getAddress();
        rotFound = true;
        NimBLEDevice::getScan()->stop();
    }
};
static RotScan rotScanCb;
static void rotNotify(NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) { rotLb.feed(d, n, rotPush); }
static void rotorBle() {
    if (!NimBLEDevice::isInitialized()) { delay(1000); return; }
    rotSt = Link::Connecting;
    rotFound = false;
    trk::scanLock();
    NimBLEScan* sc = NimBLEDevice::getScan();
    sc->setScanCallbacks(&rotScanCb, false);
    sc->start(5000, false, true);
    while (sc->isScanning()) delay(50);
    trk::scanUnlock();
    if (!rotFound) { rotSt = Link::Failed; delay(6000); return; }
    NimBLEClient* cli = NimBLEDevice::createClient();
    cli->setConnectTimeout(8000);
    if (!cli->connect(rotAddr)) { NimBLEDevice::deleteClient(cli); rotSt = Link::Failed; delay(5000); return; }
    NimBLERemoteService* svc = cli->getService(ROT_SVC);
    NimBLERemoteCharacteristic *cmd = svc ? svc->getCharacteristic(ROT_CMD) : nullptr, *evt = svc ? svc->getCharacteristic(ROT_EVT) : nullptr;
    if (!cmd || !evt) { cli->disconnect(); NimBLEDevice::deleteClient(cli); rotSt = Link::Failed; delay(5000); return; }
    evt->readValue();                                           // provoca el emparejamiento (lectura cifrada)
    if (!cli->isConnected() || !evt->subscribe(true, rotNotify)) { if (cli->isConnected()) cli->disconnect(); NimBLEDevice::deleteClient(cli); rotSt = Link::Failed; delay(5000); return; }
    rotLb.reset();
    rotSt = Link::Linked;
    rotHello();
    while (cli->isConnected() && !rotRestart) {
        std::string l;
        if (rotPop(l)) { cmd->writeValue((const uint8_t*)l.data(), l.size(), true); delay(20); }
        else delay(10);
    }
    if (cli->isConnected()) cli->disconnect();
    NimBLEDevice::deleteClient(cli);
    rotSt = rotRestart ? Link::Off : Link::Failed;
    if (!rotRestart) delay(3000);
}
static void rotorTask(void*) {
    for (;;) {
        int type;
        { std::lock_guard<std::mutex> g(mx); type = rotType; rotRestart = false; }
        if (type == 1) rotorTcp();
        else if (type == 2) rotorBle();
        else { rotSt = Link::Off; delay(500); }
    }
}

// ------------------------------------------------------------------ DX Cluster
static volatile Link dxSt = Link::Off;
static volatile bool dxWant = false;
static std::deque<std::string> dxTx, dxRx;
static std::string dxHost, dxCall;
static int dxPort = 7300;
Link dxState() { return dxSt; }
const char* dxStateText() {
    switch (dxSt) { case Link::Off: return "disconnected"; case Link::Connecting: return "connecting"; case Link::Linked: return "connected"; default: return "failed"; }
}
void dxConnect() {
    std::lock_guard<std::mutex> g(mx);
    dxHost = cfg::get().dxHost; dxPort = cfg::get().dxPort; dxCall = cfg::get().dxCall;
    dxTx.clear();
    dxWant = true;
}
void dxDisconnect() { dxWant = false; }
void dxSend(const std::string& l) { std::lock_guard<std::mutex> g(mx); if (dxTx.size() < 16) dxTx.push_back(l); }
bool dxPoll(std::string& l) { std::lock_guard<std::mutex> g(mx); if (dxRx.empty()) return false; l = dxRx.front(); dxRx.pop_front(); return true; }
static void dxPush(const std::string& l) { std::lock_guard<std::mutex> g(mx); if (dxRx.size() < 80) dxRx.push_back(l); }
static void dxTask(void*) {
    for (;;) {
        if (!dxWant || !online()) { dxSt = Link::Off; delay(300); continue; }
        std::string host, call;
        int port;
        { std::lock_guard<std::mutex> g(mx); host = dxHost; call = dxCall; port = dxPort; }
        NetworkClient c;
        dxSt = Link::Connecting;
        if (!c.connect(host.c_str(), port, 6000)) { dxSt = Link::Failed; dxPush("Could not connect to " + host); dxWant = false; continue; }
        dxSt = Link::Linked;
        std::string line;
        bool logged = false;
        uint32_t t0 = millis();
        while (c.connected() && dxWant) {
            while (c.available()) {
                int b = c.read();
                if (b == 0xFF) { c.read(); c.read(); continue; }    // orden telnet (IAC x y): se ignora
                if (b == '\n') { dxPush(line); line.clear(); continue; }
                if (b == '\r' || b == 7 || b < 0) continue;
                if (line.size() < 300) line += (char)b;
                // el nodo pide el indicativo sin salto de linea ("login: ", "Please enter your call: ")
                if (!logged && line.size() > 3 && (line.find("login") != std::string::npos || line.find("call") != std::string::npos) && line.back() == ' ') {
                    dxPush(line); line.clear();
                    std::string l = call + "\r\n";
                    c.write((const uint8_t*)l.data(), l.size());
                    logged = true;
                }
            }
            if (!logged && millis() - t0 > 8000) {                   // algunos nodos no preguntan: se manda igual
                std::string l = call + "\r\n";
                c.write((const uint8_t*)l.data(), l.size());
                logged = true;
            }
            std::string o;
            { std::lock_guard<std::mutex> g(mx); if (!dxTx.empty()) { o = dxTx.front(); dxTx.pop_front(); } }
            if (!o.empty()) { o += "\r\n"; c.write((const uint8_t*)o.data(), o.size()); }
            delay(20);
        }
        c.stop();
        dxSt = dxWant ? Link::Failed : Link::Off;
        if (dxWant) { dxPush("Connection closed by the node"); delay(5000); }
    }
}

// ------------------------------------------------------------------ trabajos de internet: tiempo y firmware
static volatile bool wxReq = false, fwCheckReq = false, fwInstallReq = false, fwCancelReq = false;
static std::string wxText = "not set";
static std::string fwBoard;
static Fw fwState;
static uint32_t wxLast = 0;
static double posLat = 0, posLon = 0;
static volatile bool posOk = false;

std::string wxStatus() { std::lock_guard<std::mutex> g(mx); return wxText; }
void wxNow() { wxReq = true; }
Fw fw() { std::lock_guard<std::mutex> g(mx); return fwState; }
void fwCheck(const std::string& b) { { std::lock_guard<std::mutex> g(mx); fwBoard = b; } fwCheckReq = true; }
void fwInstall(const std::string& b) { { std::lock_guard<std::mutex> g(mx); fwBoard = b; } fwCancelReq = false; fwInstallReq = true; }
void fwCancel() { fwCancelReq = true; }
static void wxSet(const std::string& t) { std::lock_guard<std::mutex> g(mx); wxText = t; }
static void fwSet(int phase, const std::string& t, uint32_t done = 0, uint32_t total = 0) {
    std::lock_guard<std::mutex> g(mx);
    fwState.phase = phase; fwState.text = t; fwState.done = done; fwState.total = total;
}

static bool httpGet(const std::string& url, std::string& out, size_t maxBytes) {
    NetworkClientSecure tls;
    tls.setInsecure();                                          // sin comprobar el certificado; el firmware se comprueba con su SHA-256
    HTTPClient h;
    h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    h.setTimeout(10000);
    if (!h.begin(tls, url.c_str())) return false;
    int code = h.GET();
    if (code != 200) { h.end(); return false; }
    out = h.getString().c_str();
    h.end();
    return out.size() <= maxBytes;
}

static void doWeather() {
    std::string key = cfg::get().owmKey;
    if (key.empty()) { wxSet("no OpenWeatherMap key (Settings)"); return; }
    if (!online()) { wxSet("no WiFi"); return; }
    if (!posOk) { wxSet("no position (tracker GNSS)"); return; }
    char url[256];
    snprintf(url, sizeof url, "https://api.openweathermap.org/data/2.5/weather?lat=%.4f&lon=%.4f&units=metric&appid=%s", posLat, posLon, key.c_str());
    std::string body;
    if (!httpGet(url, body, 8192)) { wxSet("download failed (key or network)"); return; }
    JsonDocument d;
    if (deserializeJson(d, body)) { wxSet("bad answer from OpenWeatherMap"); return; }
    proto::Cmd c("wxnet");
    c.text("src", "OWM");
    std::string place = d["name"] | "";
    if (!place.empty()) c.text("pl", place);
    float t = d["main"]["temp"] | -999.0f;
    if (t > -900) c.num("t", (double)t);
    if (!d["main"]["humidity"].isNull()) c.num("h", d["main"]["humidity"] | 0);
    float p = d["main"]["sea_level"] | (d["main"]["pressure"] | 0.0f);
    if (p > 0) c.num("p", (double)p);
    float ws = d["wind"]["speed"] | -1.0f, wg = d["wind"]["gust"] | -1.0f;
    int wd = d["wind"]["deg"] | -1;
    if (ws >= 0) c.num("ws", (double)ws);
    if (wg >= 0) c.num("wg", (double)wg);
    if (wd >= 0) c.num("wd", wd);
    if (!d["clouds"]["all"].isNull()) c.num("cl", d["clouds"]["all"] | 0);
    if (!d["rain"]["1h"].isNull()) c.num("r", (double)(d["rain"]["1h"] | 0.0f));
    std::string ds = d["weather"][0]["description"] | "";
    if (!ds.empty()) c.text("ds", ds);
    uint32_t dt = d["dt"] | 0u;
    int age = timeValid() && dt ? (int)(time(nullptr) - dt) : 0;
    if (age < 0) age = 0;
    c.num("age", age);
    if (cfg::get().wxToTracker && model && model->info.capWxnet) trk::send(c.str());
    if (cfg::get().wxToRotor && ws >= 0 && rotSt == Link::Linked) rotorSend(rotp::wx(ws, wg, wd, age, "OWM"));
    char b[96];
    snprintf(b, sizeof b, "OWM %s %.1f C, wind %.1f m/s", place.c_str(), t, ws);
    wxSet(b);
}

static const char* SITE = "https://ea7jrs.github.io/APRS-LoRa-Plus/";
struct Release { std::string version, url, sha; uint32_t size = 0; bool ota = false; };
static bool fetchRelease(const std::string& board, Release& r) {
    std::string body;
    if (!httpGet(std::string(SITE) + "ota/" + board + ".json", body, 4096)) return false;
    JsonDocument d;
    if (deserializeJson(d, body)) return false;
    r.version = d["version"] | ""; r.size = d["size"] | 0u; r.sha = d["sha256"] | ""; r.ota = d["ota"] | false;
    std::string u = d["url"] | "";
    r.url = u.rfind("http", 0) == 0 ? u : std::string(SITE) + u;
    return !r.version.empty() && r.size && r.sha.size() == 64;
}
static void doFwCheck() {
    std::string board;
    { std::lock_guard<std::mutex> g(mx); board = fwBoard; }
    fwSet(1, "Looking for the published version...");
    Release r;
    if (!online() || !fetchRelease(board, r)) { fwSet(6, "Could not read the published version (WiFi?)"); return; }
    { std::lock_guard<std::mutex> g(mx); fwState.latest = r.version; }
    char b[96];
    snprintf(b, sizeof b, "%s published (%.2f MB)%s", r.version.c_str(), r.size / 1048576.0, r.ota ? "" : ", no second slot");
    fwSet(2, b);
}
static void doFwInstall() {
    std::string board;
    { std::lock_guard<std::mutex> g(mx); board = fwBoard; }
    Release r;
    if (!online() || !fetchRelease(board, r)) { fwSet(6, "Could not read the published version"); return; }
    uint8_t* img = (uint8_t*)heap_caps_malloc(r.size, MALLOC_CAP_SPIRAM);
    if (!img) { fwSet(6, "Not enough memory for the image"); return; }
    // descarga
    NetworkClientSecure tls;
    tls.setInsecure();
    HTTPClient h;
    h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    h.setTimeout(15000);
    bool ok = h.begin(tls, r.url.c_str()) && h.GET() == 200;
    uint32_t got = 0;
    if (ok) {
        NetworkClient* s = h.getStreamPtr();
        uint32_t t = millis();
        while (got < r.size && millis() - t < 20000 && !fwCancelReq) {
            int n = s->available();
            if (n <= 0) { delay(5); continue; }
            n = s->read(img + got, std::min<uint32_t>(n, r.size - got));
            if (n > 0) { got += n; t = millis(); if ((got & 0xFFFF) < (uint32_t)n) fwSet(3, "Downloading " + r.version + "...", got, r.size); }
        }
    }
    h.end();
    if (fwCancelReq || got != r.size) { heap_caps_free(img); fwSet(6, fwCancelReq ? "Cancelled" : "Download incomplete"); return; }
    uint8_t sum[32];
    mbedtls_sha256(img, r.size, sum, 0);
    char hex[65];
    for (int i = 0; i < 32; i++) snprintf(hex + 2 * i, 3, "%02x", sum[i]);
    if (r.sha != hex) { heap_caps_free(img); fwSet(6, "The download does not match its SHA-256"); return; }
    // envio por BLE: orden ota, ventana de 8 KB sobre lo que confirma el tracker ({"e":"ota","off":n})
    uint32_t ok0 = model->okSeq, err0 = model->errSeq;
    model->otaOff = 0; model->otaDone = false;
    trk::send(proto::Cmd("ota").num("size", r.size).text("sha", r.sha).str());
    uint32_t t = millis();
    while (model->okSeq == ok0 && model->errSeq == err0 && millis() - t < 15000) delay(20);
    if (model->errSeq != err0 || model->okSeq == ok0) { heap_caps_free(img); fwSet(6, model->errSeq != err0 ? model->lastError : "The tracker did not start the update"); return; }
    uint32_t sent = 0, lastOff = 0, lastAt = millis();
    while (!model->otaDone) {
        if (fwCancelReq) { trk::fwClear(); trk::send(proto::cmdSimple("ota_abort")); heap_caps_free(img); fwSet(6, "Cancelled: the tracker keeps its firmware"); return; }
        if (model->errSeq != err0) { trk::fwClear(); heap_caps_free(img); fwSet(6, model->lastError); return; }
        if (trk::state() != trk::State::Linked) { heap_caps_free(img); fwSet(6, "Link lost: the tracker keeps its firmware"); return; }
        uint32_t off = model->otaOff;
        if (off != lastOff) { lastOff = off; lastAt = millis(); fwSet(4, "Sending to the tracker...", off, r.size); }
        if (millis() - lastAt > 25000) { trk::fwClear(); heap_caps_free(img); fwSet(6, "The tracker stopped answering"); return; }
        if (sent < r.size && sent < off + 8192) {
            uint32_t n = std::min<uint32_t>(240, r.size - sent);
            if (trk::fwPush(img + sent, n)) sent += n; else delay(5);
        } else delay(10);
    }
    heap_caps_free(img);
    fwSet(5, "Installed " + r.version + ": the tracker restarts and links again", r.size, r.size);
}

// ------------------------------------------------------------------ avisos meteorologicos
static net::Warnings warnState;
static volatile bool warnReq = false;
static uint32_t warnLast = 0;
static bool warnFailed = false;
Warnings warnings() { std::lock_guard<std::mutex> g(mx); return warnState; }
void warnFetch() { warnReq = true; }
static void warnStatus(const std::string& t) { std::lock_guard<std::mutex> g(mx); warnState.status = t; }

// Recibe el cuerpo segun llega, corta los avisos y se queda con los que tocan
struct WarnSink : public Stream {
    wxw::Scanner sc;
    std::vector<wxw::Warning> keep;
    bool aemet = false;
    int64_t nowS = 0;
    size_t bytes = 0;
    void take(const std::string& blk) {
        const cfg::Settings& st = cfg::get();
        wxw::Warning w;
        if (!wxw::parse(blk, w, posOk, posLat, posLon) || wxw::expired(w, nowS)) return;
        const bool mine = w.hasArea ? (w.inside || w.distKm <= st.warnKm) : (!aemet || !st.maArea.empty()) && wxw::areaMatches(w.area, st.maArea);
        if (!mine) return;
        for (const wxw::Warning& k : keep) if (k.event == w.event && k.area == w.area && k.onset == w.onset) return;
        if (keep.size() < 40) keep.push_back(w);
    }
    size_t write(const uint8_t* b, size_t n) override {
        bytes += n;
        std::string blk;
        bool r = sc.feed(b, n, blk);
        while (r) { take(blk); r = sc.feed(nullptr, 0, blk); }
        return n;
    }
    size_t write(uint8_t c) override { return write(&c, 1); }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
static int streamGet(const std::string& url, Stream& sink) {
    NetworkClientSecure tls;
    tls.setInsecure();
    HTTPClient h;
    h.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    h.setTimeout(15000);
    h.setUserAgent("APRS-LoRa-Plus-Display/" DISPLAY_VERSION " (EA7JRS)");
    if (!h.begin(tls, url.c_str())) return 0;
    int code = h.GET();
    if (code == 200 && h.writeToStream(&sink) < 0) code = -1;
    h.end();
    return code;
}
static void doWarnings() {
    const cfg::Settings st = cfg::get();
    warnFailed = true;
    if (!online()) { warnStatus("no WiFi"); return; }
    std::string url;
    WarnSink sink;
    sink.aemet = st.warnSrc == 1;
    sink.nowS = timeValid() ? (int64_t)time(nullptr) : 0;
    if (st.warnSrc == 1) {
        if (st.aemetKey.empty()) { warnStatus("no AEMET OpenData key (Setup)"); return; }
        if (!posOk && st.maArea.empty()) { warnStatus("no position yet (tracker GNSS) and no area text"); return; }
        // AEMET contesta con la direccion de los datos: un archivo con todos los avisos CAP en vigor de Espana
        std::string body;
        if (!httpGet("https://opendata.aemet.es/opendata/api/avisos_cap/ultimoelaborado/area/esp?api_key=" + st.aemetKey, body, 4096)) { warnStatus("AEMET: no answer (key or network)"); return; }
        JsonDocument d;
        if (deserializeJson(d, body) || !d["datos"].is<const char*>()) { warnStatus("AEMET: unexpected answer"); return; }
        url = d["datos"].as<const char*>();
    } else {
        if (st.maCountry.empty()) { warnStatus("no country (Setup)"); return; }
        url = "https://feeds.meteoalarm.org/feeds/meteoalarm-legacy-atom-" + st.maCountry;
    }
    warnStatus("downloading...");
    const int code = streamGet(url, sink);
    if (code != 200) { char b[48]; snprintf(b, sizeof b, "download failed (%d)", code); warnStatus(b); return; }
    if (sink.sc.gzip()) { warnStatus("the file came compressed: not supported yet"); return; }
    std::sort(sink.keep.begin(), sink.keep.end(), wxw::before);
    char b[96];
    snprintf(b, sizeof b, "%s: %u in your area of %u in force (%u kB)", st.warnSrc == 1 ? "AEMET" : "Meteoalarm", (unsigned)sink.keep.size(), (unsigned)sink.sc.blocks(),
             (unsigned)(sink.bytes / 1024));
    std::lock_guard<std::mutex> g(mx);
    warnState.list = sink.keep;
    warnState.status = b;
    warnState.seq++;
    warnFailed = false;
}

static void jobsTask(void*) {
    for (;;) {
        // avisos: al pedirlos, al arrancar con WiFi y cada 15 min (a los 3 si la ultima vez fallo)
        if (cfg::get().warnSrc && (warnReq || (online() && (!warnLast || millis() - warnLast > (warnFailed ? 180000u : 900000u))))) {
            warnReq = false; warnLast = millis() | 1; doWarnings();
        } else if (warnReq) { warnReq = false; warnStatus("off (choose a source in Setup)"); }
        if (fwInstallReq) { fwInstallReq = false; doFwInstall(); }
        if (fwCheckReq) { fwCheckReq = false; doFwCheck(); }
        if (wxReq) { wxReq = false; doWeather(); wxLast = millis(); }
        delay(200);
    }
}

// ------------------------------------------------------------------ mapa de teselas
// La microSD de las placas de 7" va por SPI con el CS en el expansor: se deja seleccionada siempre (es lo unico que hay en
// ese bus) y a la libreria SD se le da un pin que no se usa. Las teselas se guardan tal como llegan en /maps/<capa>/z/x/y.bin.
#ifndef SD_SCK
#define SD_SCK 12
#define SD_MISO 13
#define SD_MOSI 11
#endif
#ifndef SD_DUMMY_CS
#define SD_DUMMY_CS 6                 // el pin del conector de sensor: SD lo mueve como si fuera el CS
#endif
static bool sdReady = false, mapStarted = false;
bool sdOk() { return sdReady; }

struct VecStream : public Stream {
    std::vector<uint8_t>& v;
    size_t cap;
    explicit VecStream(std::vector<uint8_t>& d, size_t c) : v(d), cap(c) {}
    size_t write(uint8_t c) override { if (v.size() >= cap) return 0; v.push_back(c); return 1; }
    size_t write(const uint8_t* b, size_t n) override { if (v.size() + n > cap) return 0; v.insert(v.end(), b, b + n); return n; }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
static int tileHttp(const std::string& url, std::vector<uint8_t>& out) {
    // una conexion por servidor que se queda abierta: el saludo TLS de cada tesela costaria mas que la tesela
    static NetworkClientSecure tls[2];
    static HTTPClient http[2];
    static bool init = false;
    if (!init) { for (int i = 0; i < 2; i++) { tls[i].setInsecure(); http[i].setReuse(true); http[i].setTimeout(8000); http[i].setUserAgent("APRS-LoRa-Plus-Display/" DISPLAY_VERSION " (EA7JRS)"); } init = true; }
    const int i = url.find("openseamap") != std::string::npos ? 1 : 0;
    out.clear();
    if (!http[i].begin(tls[i], url.c_str())) return 0;
    int code = http[i].GET();
    if (code == 200) {
        VecStream vs(out, 400000);
        if (http[i].writeToStream(&vs) < 0) code = 0;
    }
    http[i].end();
    return code < 0 ? 0 : code;
}
static bool tileLoad(const std::string& path, std::vector<uint8_t>& out) {
    if (!sdReady) return false;
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) return false;
    size_t n = f.size();
    out.resize(n);
    bool ok = n == 0 || f.read(out.data(), n) == n;
    f.close();
    return ok;
}
static void tileSave(const std::string& path, const uint8_t* data, size_t n) {
    if (!sdReady) return;
    for (size_t i = 1; i < path.size(); i++)                    // las carpetas, una a una
        if (path[i] == '/') { std::string d = path.substr(0, i); if (!SD.exists(d.c_str())) SD.mkdir(d.c_str()); }
    File f = SD.open(path.c_str(), FILE_WRITE);
    if (!f) return;
    if (n) f.write(data, n);
    f.close();
}
static void tilesTask(void*) {
    for (;;) delay(mapt::work() ? 2 : 120);
}
void mapBegin() {
    if (mapStarted) return;
    mapStarted = true;
    board::sdSelect(true);
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
    sdReady = SD.begin(SD_DUMMY_CS, SPI, 20000000) && SD.cardType() != CARD_NONE;
    Serial.printf("[map] microSD: %s\n", sdReady ? "lista, teselas en /maps" : "no hay (teselas solo en memoria)");
    mapt::Io io;
    io.alloc = [](size_t n) -> void* { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM); };
    io.online = []() -> bool { return online(); };
    io.http = tileHttp;
    io.load = tileLoad;
    io.save = tileSave;
    io.ms = []() -> uint32_t { return millis(); };
    mapt::begin(io, 24, cfg::get().arcgisKey);
    xTaskCreatePinnedToCore(tilesTask, "tiles", 16384, nullptr, 1, nullptr, 0);
}

// ------------------------------------------------------------------ arranque y bucle
void begin() {
    wifiApply();
    rotorApply();
    xTaskCreatePinnedToCore(rotorTask, "rotor", 6144, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(dxTask, "dx", 6144, nullptr, 1, nullptr, 0);
    xTaskCreatePinnedToCore(jobsTask, "netjobs", 12288, nullptr, 1, nullptr, 0);
}

void loop() {
    if (online() && !ntpStarted) { configTime(0, 0, "pool.ntp.org", "time.google.com"); ntpStarted = true; }
    if (model) {
        if (model->gnss.ok && model->gnss.fix) { posLat = model->gnss.lat; posLon = model->gnss.lon; posOk = true; }
        if (model->netReq == "wx") { model->netReq.clear(); wxReq = true; }
    }
    // tiempo de internet cada wxMinutes (si hay clave y a quien mandarlo)
    const cfg::Settings& s = cfg::get();
    const bool wanted = !s.owmKey.empty() && ((s.wxToTracker && model && model->info.capWxnet && trk::state() == trk::State::Linked) || (s.wxToRotor && rotSt == Link::Linked));
    if (wanted && online() && posOk && (wxLast == 0 || millis() - wxLast > (uint32_t)(s.wxMinutes < 10 ? 10 : s.wxMinutes) * 60000u)) { wxLast = millis(); wxReq = true; }
}

} // namespace net
#endif
