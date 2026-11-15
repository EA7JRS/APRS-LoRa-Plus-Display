/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ble.h"

// Sin Bluetooth central: fuera del ESP32, con APRS_LINKS_NO_BLE o si NimBLE se compila sin el papel de central (el rotor solo es
// periferico). Entonces las funciones no hacen nada.
#if defined(ESP32) && !defined(APRS_LINKS_NO_BLE) && !defined(CONFIG_BT_NIMBLE_ROLE_CENTRAL_DISABLED)
#define APRS_LINKS_BLE 1
#endif

#ifdef APRS_LINKS_BLE
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>
#endif

namespace ble {

static Status st;
const Status& status() { return st; }

#ifdef APRS_LINKS_BLE

// Servicio de datos NMEA de APRS LoRa+ (docs/BLE_NMEA.md): ...0002 escribe el plotter, ...0003 notifica la pasarela.
static const NimBLEUUID kSvc("4c6f5261-2b00-4ea7-b100-000000000001");
static const NimBLEUUID kChrIn("4c6f5261-2b00-4ea7-b100-000000000003");
static const NimBLEUUID kChrOut("4c6f5261-2b00-4ea7-b100-000000000002");
// Tambien vale un equipo con el servicio UART de Nordic (muchos puentes NMEA por BLE lo usan)
static const NimBLEUUID kNus("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static const NimBLEUUID kNusTx("6e400003-b5a3-f393-e0a9-e50e24dcca9e");
static const NimBLEUUID kNusRx("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
// Tracker APRS LoRa+: anuncia su servicio KISS (o el UART de Nordic en modo TNC2); el NMEA va por el servicio de control,
// que exige enlace cifrado y emparejado con el PIN (el tracker muestra la clave, el plotter la teclea)
static const NimBLEUUID kKiss("00000001-ba2a-46c9-ae49-01b0961f68bb");
static const NimBLEUUID kTrk("4c6f5261-2b00-4ea7-a000-000000000001");
static const NimBLEUUID kTrkNmea("4c6f5261-2b00-4ea7-a000-000000000004");
static const NimBLEUUID kTrkCmd("4c6f5261-2b00-4ea7-a000-000000000002");     // ordenes JSON por lineas (datos del barco: "vessel")
static const NimBLEUUID kTrkEvt("4c6f5261-2b00-4ea7-a000-000000000003");     // eventos JSON por lineas
static volatile uint32_t wantPin = 123456;

static volatile bool wantOn = false;
static char wantAddr[18] = "";
static volatile uint32_t cfgVersion = 0;
static StreamBufferHandle_t sb = nullptr;
static StreamBufferHandle_t out = nullptr;                      // plotter -> equipo (rumbo y velocidad al instrumento)
static NimBLERemoteCharacteristic* chOut = nullptr;
static NimBLERemoteCharacteristic* chCmd = nullptr;             // ordenes al tracker
static StreamBufferHandle_t cmdq = nullptr;                    // plotter -> tracker (JSON)
static StreamBufferHandle_t evq = nullptr;                     // tracker -> plotter (eventos JSON)
static char evLine[512];
static int evLen = 0;
static TaskHandle_t taskHandle = nullptr;

static volatile bool found = false;
static NimBLEAddress foundAddr;
static char foundName[24] = "";
static int foundRssi = 0;
static volatile bool linkDown = false;

static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (!len) return;
    xStreamBufferSend(sb, data, len, 0);                           // si el bucle principal no da abasto, se pierde
    st.bytes += (uint32_t)len;
}

static void onEvent(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    if (len && evq) xStreamBufferSend(evq, data, len, 0);
}

class ScanCb : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (found) return;
        if (!dev->isAdvertisingService(kSvc) && !dev->isAdvertisingService(kNus) && !dev->isAdvertisingService(kKiss)) return;
        std::string a = dev->getAddress().toString();
        if (wantAddr[0] && strcasecmp(a.c_str(), wantAddr) != 0) return;      // hay un equipo ya elegido y no es este
        foundAddr = dev->getAddress();
        snprintf(foundName, sizeof(foundName), "%s", dev->getName().c_str());
        foundRssi = dev->getRSSI();
        found = true;
    }
};

class ClientCb : public NimBLEClientCallbacks {
    void onDisconnect(NimBLEClient*, int) override { linkDown = true; }
    void onPassKeyEntry(NimBLEConnInfo& info) override { NimBLEDevice::injectPassKey(info, wantPin); }
};

static ScanCb scanCb;
static ClientCb clientCb;

static bool subscribe(NimBLEClient* c) {
    NimBLERemoteService* s = c->getService(kTrk);
    st.tracker = s != nullptr;
    chOut = nullptr;
    chCmd = nullptr;
    st.canSend = false;
    st.canCommand = false;
    st.pinFailed = false;
    if (cmdq) xStreamBufferReset(cmdq);
    if (evq) xStreamBufferReset(evq);
    evLen = 0;
    if (s) {                                                       // tracker: primero emparejar; sin eso no manda NMEA
        if (!c->secureConnection() || !c->getConnInfo().isEncrypted() || !c->getConnInfo().isAuthenticated()) { st.pinFailed = true; return false; }
        NimBLERemoteCharacteristic* n = s->getCharacteristic(kTrkNmea);
        NimBLERemoteCharacteristic* ev = s->getCharacteristic(kTrkEvt);
        NimBLERemoteCharacteristic* cm = s->getCharacteristic(kTrkCmd);
        if (ev && ev->canNotify() && ev->subscribe(true, onEvent) && cm && (cm->canWrite() || cm->canWriteNoResponse())) {
            chCmd = cm;
            st.canCommand = true;
        }
        return n && n->canNotify() && n->subscribe(true, onNotify);
    }
    s = c->getService(kSvc);
    NimBLERemoteCharacteristic* ch = s ? s->getCharacteristic(kChrIn) : nullptr;
    NimBLERemoteCharacteristic* w = s ? s->getCharacteristic(kChrOut) : nullptr;
    chOut = w && (w->canWriteNoResponse() || w->canWrite()) ? w : nullptr;
    st.canSend = chOut != nullptr;
    if (!ch) {                                                     // UART de Nordic: la que notifique de las dos
        s = c->getService(kNus);
        ch = s ? s->getCharacteristic(kNusTx) : nullptr;
        if (!ch || !ch->canNotify()) ch = s ? s->getCharacteristic(kNusRx) : nullptr;
    }
    return ch && ch->canNotify() && ch->subscribe(true, onNotify);
}

static void task(void*) {
    bool inited = false;
    uint32_t seen = 0;
    NimBLEClient* client = nullptr;
    for (;;) {
        bool changed = seen != cfgVersion;
        seen = cfgVersion;
        if (!wantOn || changed) {                                  // apagado o configuracion nueva: se suelta todo
            if (inited) {
                NimBLEDevice::getScan()->stop();
                if (client && client->isConnected()) client->disconnect();
            }
            found = false;
            linkDown = false;
            st.state = OFF;
            if (!wantOn) { vTaskDelay(pdMS_TO_TICKS(300)); continue; }
        }
        if (!inited) {
            NimBLEDevice::init("APRS LoRa+ Plotter");
            NimBLEDevice::setMTU(247);
            NimBLEDevice::setSecurityAuth(true, true, true);       // bonding, MITM y conexiones seguras, como pide el tracker
            NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);
            NimBLEScan* scan = NimBLEDevice::getScan();
            scan->setScanCallbacks(&scanCb, false);
            scan->setActiveScan(true);                             // para leer el nombre
            scan->setInterval(160);
            scan->setWindow(80);
            inited = true;
        }
        if (client && client->isConnected() && !linkDown) {
            st.state = CONNECTED;
            // lo que el plotter manda al equipo, en trozos que quepan en el MTU, sin esperar respuesta
            uint8_t buf[244];
            int room = (int)client->getMTU() - 3;
            if (room > (int)sizeof(buf)) room = sizeof(buf);
            if (room < 20) room = 20;
            size_t n;
            while (chOut && out && (n = xStreamBufferReceive(out, buf, (size_t)room, 0)) > 0) {
                if (chOut->writeValue(buf, n, !chOut->canWriteNoResponse())) st.sent += (uint32_t)n;
                else break;
            }
            while (chCmd && cmdq && (n = xStreamBufferReceive(cmdq, buf, (size_t)room, 0)) > 0) {
                if (chCmd->writeValue(buf, n, true)) st.sent += (uint32_t)n;    // con respuesta: el tracker no pierde trozos
                else break;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        st.canSend = false;
        linkDown = false;
        if (found) {
            NimBLEDevice::getScan()->stop();
            st.state = CONNECTING;
            if (!client) {
                client = NimBLEDevice::createClient();
                client->setClientCallbacks(&clientCb, false);
                client->setConnectTimeout(8000);
            }
            bool ok = client && client->connect(foundAddr) && subscribe(client);
            if (ok) {
                snprintf(st.name, sizeof(st.name), "%s", foundName[0] ? foundName : "pasarela");
                snprintf(st.addr, sizeof(st.addr), "%s", foundAddr.toString().c_str());
                st.rssi = foundRssi;
                st.connects++;
                st.state = CONNECTED;
            } else if (client && client->isConnected()) client->disconnect();   // conectado pero sin el servicio esperado
            found = false;
            if (!ok) vTaskDelay(pdMS_TO_TICKS(st.pinFailed ? 15000 : 2000));    // con el PIN mal, sin insistir tanto
            continue;
        }
        st.state = SCANNING;
        if (!NimBLEDevice::getScan()->isScanning()) NimBLEDevice::getScan()->start(0, false, true);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void set(bool on, const char* addr, uint32_t pin) {
    wantPin = pin;
    if (!sb) sb = xStreamBufferCreate(4096, 1);
    if (!out) out = xStreamBufferCreate(1024, 1);
    if (!cmdq) cmdq = xStreamBufferCreate(1024, 1);
    if (!evq) evq = xStreamBufferCreate(4096, 1);
    snprintf(wantAddr, sizeof(wantAddr), "%s", addr ? addr : "");
    wantOn = on;
    cfgVersion = cfgVersion + 1;
    if (on && !taskHandle) xTaskCreatePinnedToCore(task, "ble", 8192, nullptr, 1, &taskHandle, 0);
}

int read(uint8_t* b, int max) { return sb ? (int)xStreamBufferReceive(sb, b, (size_t)max, 0) : 0; }

bool write(const char* text, int len) {
    if (!out || !st.canSend || st.state != CONNECTED || len <= 0) return false;
    if (xStreamBufferSpacesAvailable(out) < (size_t)len) return false;       // la tanda entera o nada: sin lineas partidas
    return xStreamBufferSend(out, text, (size_t)len, 0) == (size_t)len;
}

bool command(const char* json) {
    if (!cmdq || !st.canCommand || st.state != CONNECTED || !json) return false;
    const size_t len = strlen(json);
    if (!len || xStreamBufferSpacesAvailable(cmdq) < len + 1) return false;
    xStreamBufferSend(cmdq, json, len, 0);
    xStreamBufferSend(cmdq, "\n", 1, 0);
    return true;
}

int readEvent(char* line, int max) {
    if (!evq || max < 2) return 0;
    char c;
    while (xStreamBufferReceive(evq, &c, 1, 0) == 1) {
        if (c == '\r') continue;
        if (c == '\n') {
            if (!evLen) continue;
            int n = evLen < max - 1 ? evLen : max - 1;
            memcpy(line, evLine, (size_t)n);
            line[n] = 0;
            evLen = 0;
            return n;
        }
        if (evLen < (int)sizeof(evLine)) evLine[evLen++] = c;      // una linea demasiado larga se corta (no la usamos)
    }
    return 0;
}

#else   // en el PC no hay Bluetooth (ni en los equipos sin central)

void set(bool, const char*, uint32_t) {}
int read(uint8_t*, int) { return 0; }
bool write(const char*, int) { return false; }
bool command(const char*) { return false; }
int readEvent(char*, int) { return 0; }

#endif

}
