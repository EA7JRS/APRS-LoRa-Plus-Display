/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "tracker_link.h"
#include "protocol.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <deque>
#include <mutex>

namespace trk {

static const NimBLEUUID UUID_CTRL("4c6f5261-2b00-4ea7-a000-000000000001");
static const NimBLEUUID UUID_CMD("4c6f5261-2b00-4ea7-a000-000000000002");
static const NimBLEUUID UUID_EVT("4c6f5261-2b00-4ea7-a000-000000000003");
static const NimBLEUUID UUID_KISS("00000001-ba2a-46c9-ae49-01b0961f68bb");       // los que anuncia el tracker
static const NimBLEUUID UUID_TNC2("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");

static std::mutex mx;
static std::deque<std::string> rxLines, txLines;
static proto::LineBuffer lineBuf;
static volatile State st = State::Idle;
static std::string peer, wantAddr;
static uint32_t pinCode = 123456;
static volatile bool doForget = false, doReconnect = false, gotDevice = false;
static NimBLEAddress foundAddr;
static std::string foundName;
static Preferences prefs;

static void setState(State s) { st = s; }

class ScanCb : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* d) override {
        if (gotDevice) return;
        bool match;
        if (!wantAddr.empty()) match = (d->getAddress().toString() == wantAddr);
        else match = d->isAdvertisingService(UUID_CTRL) || d->isAdvertisingService(UUID_KISS) || d->isAdvertisingService(UUID_TNC2);
        if (!match) return;
        foundAddr = d->getAddress();
        foundName = d->getName();
        gotDevice = true;
        NimBLEDevice::getScan()->stop();
    }
};
static ScanCb scanCb;

class ClientCb : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*, int reason) override { Serial.printf("[ble] desconectado (%d)\n", reason); }
    void onPassKeyEntry(NimBLEConnInfo& info) override { NimBLEDevice::injectPassKey(info, pinCode); }
    void onConfirmPasskey(NimBLEConnInfo& info, uint32_t pass) override { NimBLEDevice::injectConfirmPasskey(info, true); }
};
static ClientCb clientCb;

static void onNotify(NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) {
    std::lock_guard<std::mutex> g(mx);
    lineBuf.feed(d, n, [](const std::string& l) { if (rxLines.size() < 64) rxLines.push_back(l); });
}

static bool writeLine(NimBLERemoteCharacteristic* c, const std::string& s) {
    return c->writeValue((const uint8_t*)s.data(), s.size(), true);       // con respuesta: el cuerpo largo va por escritura larga
}

static void session() {
    NimBLEClient* cli = NimBLEDevice::createClient();
    cli->setClientCallbacks(&clientCb, false);
    cli->setConnectTimeout(8000);
    setState(State::Connecting);
    if (!cli->connect(foundAddr)) { Serial.println("[ble] no se pudo conectar"); NimBLEDevice::deleteClient(cli); setState(State::Failed); return; }
    peer = foundName.empty() ? foundAddr.toString() : foundName;
    if (wantAddr.empty()) { prefs.putString("addr", foundAddr.toString().c_str()); }

    NimBLERemoteService* svc = cli->getService(UUID_CTRL);
    NimBLERemoteCharacteristic *cmd = svc ? svc->getCharacteristic(UUID_CMD) : nullptr, *evt = svc ? svc->getCharacteristic(UUID_EVT) : nullptr;
    if (!cmd || !evt) {
        Serial.println("[ble] el tracker no tiene el servicio de control (firmware antiguo o BLE en modo TNC2 sin control)");
        cli->disconnect(); NimBLEDevice::deleteClient(cli); setState(State::Failed); return;
    }

    // Leer la caracteristica cifrada provoca el emparejamiento por PIN
    setState(State::Pairing);
    if (!cli->secureConnection()) Serial.println("[ble] secureConnection ha fallado (PIN incorrecto o ya emparejado con otro PIN)");
    evt->readValue();
    if (!cli->isConnected()) { NimBLEDevice::deleteClient(cli); setState(State::Failed); return; }
    if (!evt->subscribe(true, onNotify)) { Serial.println("[ble] no se pudo suscribir a los eventos"); cli->disconnect(); NimBLEDevice::deleteClient(cli); setState(State::Failed); return; }

    { std::lock_guard<std::mutex> g(mx); lineBuf.reset(); rxLines.clear(); txLines.clear(); }
    setState(State::Linked);
    Serial.printf("[ble] enlazado con %s\n", peer.c_str());

    uint32_t last = millis();
    while (cli->isConnected() && !doReconnect && !doForget) {
        std::string out;
        { std::lock_guard<std::mutex> g(mx); if (!txLines.empty()) { out = txLines.front(); txLines.pop_front(); } }
        if (!out.empty()) {
            if (!writeLine(cmd, out)) Serial.println("[ble] fallo al escribir una orden");
            last = millis();
            delay(30);                                  // el tracker procesa las ordenes en su bucle: no apilar
        } else {
            delay(10);
        }
        (void)last;
    }
    if (cli->isConnected()) cli->disconnect();
    NimBLEDevice::deleteClient(cli);
    setState(State::Idle);
}

static void task(void*) {
    NimBLEDevice::init("APRS-LoRa+ Display");
    NimBLEDevice::setSecurityAuth(true, true, true);                     // bonding + MITM + conexiones seguras
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);             // el tracker "muestra" el PIN; aqui se teclea (inyecta)
    NimBLEDevice::setMTU(247);
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&scanCb, false);
    scan->setActiveScan(true);
    scan->setInterval(100); scan->setWindow(80);

    for (;;) {
        if (doForget) { NimBLEDevice::deleteAllBonds(); doForget = false; Serial.println("[ble] emparejamientos olvidados"); }
        doReconnect = false;
        gotDevice = false;
        setState(State::Scanning);
        scan->start(8000, false, true);
        while (scan->isScanning()) delay(50);
        if (gotDevice) session();
        else { setState(State::Idle); delay(1500); }
        { std::lock_guard<std::mutex> g(mx); txLines.clear(); }
        delay(1000);
    }
}

void begin() {
    prefs.begin("display", false);
    pinCode = prefs.getUInt("pin", 123456);
    wantAddr = prefs.getString("want", "").c_str();
    xTaskCreatePinnedToCore(task, "ble", 8192, nullptr, 3, nullptr, 0);
}
State state() { return st; }
const char* stateText() {
    switch (st) {
        case State::Idle: return "waiting";
        case State::Scanning: return "scanning";
        case State::Connecting: return "connecting";
        case State::Pairing: return "pairing (PIN)";
        case State::Linked: return "linked";
        default: return "failed";
    }
}
std::string peerName() { return peer; }
void send(const std::string& line) { std::lock_guard<std::mutex> g(mx); if (txLines.size() < 16) txLines.push_back(line); }
bool poll(std::string& line) { std::lock_guard<std::mutex> g(mx); if (rxLines.empty()) return false; line = rxLines.front(); rxLines.pop_front(); return true; }
void setPin(uint32_t p) { pinCode = p % 1000000; prefs.putUInt("pin", pinCode); }
uint32_t pin() { return pinCode; }
void setPeerAddress(const std::string& a) { wantAddr = a; prefs.putString("want", a.c_str()); }
std::string peerAddress() { return wantAddr; }
void forgetPairing() { doForget = true; doReconnect = true; }
void reconnect() { doReconnect = true; }

} // namespace trk
