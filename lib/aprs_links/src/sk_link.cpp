/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "sk_link.h"

#ifdef ESP32
#include <ArduinoJson.h>

#include "signalk.h"
#endif

namespace sk {

#ifdef ESP32

static char selfCtx[96] = "";       // "vessels.urn:mrn:..." del saludo del servidor

int feedLine(const char* json, uint32_t now, plotter::DataStore& store, Notice& notice) {
    using namespace plotter;
    notice = Notice();
    JsonDocument doc;
    if (deserializeJson(doc, json)) return 0;
    if (doc["self"].is<const char*>()) {                            // saludo: quien es el barco propio
        snprintf(selfCtx, sizeof(selfCtx), "%s", doc["self"].as<const char*>());
        return 0;
    }
    const char* ctx = doc["context"] | "";
    if (ctx[0] && strcmp(ctx, "vessels.self") != 0 && (!selfCtx[0] || strcmp(ctx, selfCtx) != 0)) return 0;   // otro barco (AIS): mas adelante
    int applied = 0;
    for (JsonObject up : doc["updates"].as<JsonArray>()) {
        for (JsonObject v : up["values"].as<JsonArray>()) {
            const char* path = v["path"] | "";
            if (!path[0]) continue;
            JsonVariant val = v["value"];
            if (const char* key = skNotificationKey(path)) {        // aviso
                if (val.is<JsonObject>()) {
                    notice.present = true;
                    snprintf(notice.key, sizeof(notice.key), "%s", key);
                    snprintf(notice.state, sizeof(notice.state), "%s", (const char*)(val["state"] | ""));
                    snprintf(notice.message, sizeof(notice.message), "%s", (const char*)(val["message"] | ""));
                }
                continue;
            }
            if (val.is<JsonObject>()) {                             // posicion: {latitude, longitude}
                if (!strcmp(path, "navigation.position") && val["latitude"].is<double>() && val["longitude"].is<double>()) {
                    if (skApply("navigation.position.latitude", val["latitude"].as<double>(), Source::Tcp, now, store)) applied++;
                    if (skApply("navigation.position.longitude", val["longitude"].as<double>(), Source::Tcp, now, store)) applied++;
                }
                continue;
            }
            if (val.is<double>() && skApply(path, val.as<double>(), Source::Tcp, now, store)) applied++;
        }
    }
    return applied;
}

#else   // en el PC no se compila ArduinoJson

int feedLine(const char*, uint32_t, plotter::DataStore&, Notice& notice) { notice = Notice(); return 0; }

#endif

}
