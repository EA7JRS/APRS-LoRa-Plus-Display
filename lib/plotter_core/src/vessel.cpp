/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "vessel.h"
#include "minijson.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace plotter {
namespace vessel {

namespace {
    struct Type { const char* name; char cls; };
    // Mismo orden y clase que AIS_Vessel del tracker (ais_vessel.cpp)
    const Type kTypes[] = {
        {"Velero de recreo", 'C'}, {"Lancha de recreo", 'S'}, {"Mercante", 'A'}, {"Petrolero", 'A'},
        {"Pasaje / ferry", 'A'}, {"Pesquero", 'A'}, {"Remolcador", 'A'}, {"Pr\xC3\xA1" "ctico", 'A'},
        {"Alta velocidad", 'A'}, {"Vigilancia", 'A'}, {"Salvamento", 'A'}, {"Sanitario", 'A'},
        {"Militar", 'A'}, {"Buceo", 'A'}, {"Velero grande", 'A'}, {"WIG / aerodeslizador", 'A'}, {"Otro buque", 'A'},
    };
    const int kTypeN = (int)(sizeof(kTypes) / sizeof(kTypes[0]));

    void baseCall(const char* in, char* out, size_t n) {
        size_t k = 0;
        for (; in[k] && in[k] != '-' && k + 1 < n; k++) out[k] = (char)toupper((unsigned char)in[k]);
        out[k] = 0;
    }
    bool sameText(const char* a, const char* b) {
        for (; *a && *b; a++, b++) if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return false;
        return *a == *b;
    }
    void put(std::string& s, const char* key, const char* v) {
        s += ",\""; s += key; s += "\":\"";
        for (; *v; v++) { if (*v == '"' || *v == '\\') s += '\\'; if ((unsigned char)*v >= 0x20) s += *v; }
        s += '"';
    }
    void put(std::string& s, const char* key, long v) {
        char b[24]; snprintf(b, sizeof(b), ",\"%s\":%ld", key, v); s += b;
    }
}

int typeCount() { return kTypeN; }
const char* typeName(int t) { return kTypes[(t >= 0 && t < kTypeN) ? t : 0].name; }
char typeClass(int t) { return kTypes[(t >= 0 && t < kTypeN) ? t : 0].cls; }
bool classB(int t) { return typeClass(t) != 'A'; }
bool validMmsi(uint32_t m) { return m >= 200000000UL && m <= 799999999UL; }
bool configured(const Data& d) { return d.name[0] || d.mmsi; }

bool same(const Data& a, const Data& b) {
    char ca[10], cb[10];
    baseCall(a.call, ca, sizeof(ca));
    baseCall(b.call, cb, sizeof(cb));
    return a.mmsi == b.mmsi && sameText(a.name, b.name) && strcmp(ca, cb) == 0 && sameText(a.mcall, b.mcall) && (a.mcall[0] || b.mcall[0] ? a.mcok == b.mcok : true) && a.type == b.type
        && a.lengthM == b.lengthM && a.beamM == b.beamM && a.draftDm == b.draftDm;
}

const char* queryCommand() { return "{\"c\":\"vessel\"}"; }

size_t setCommand(const Data& d, bool classBOnly, char* out, size_t size) {
    std::string s = "{\"c\":\"vessel\"";
    put(s, "mmsi", (long)(validMmsi(d.mmsi) ? d.mmsi : 0));
    put(s, "n", d.name);
    char c[10];
    baseCall(d.call, c, sizeof(c));
    if (c[0]) put(s, "call", c);
    put(s, "mc", d.mcall);
    if (d.mcall[0]) put(s, "mcok", (long)d.mcok);
    if (!classBOnly || classB(d.type)) put(s, "t", (long)d.type);
    put(s, "len", (long)d.lengthM);
    put(s, "beam", (long)d.beamM);
    char b[32];
    snprintf(b, sizeof(b), ",\"dr\":%d.%d}", d.draftDm / 10, d.draftDm % 10);
    s += b;
    if (s.size() + 1 > size) return 0;
    memcpy(out, s.c_str(), s.size() + 1);
    return s.size();
}

bool parseEvent(const std::string& line, Data& out, bool& classBOnly) {
    JsonValue v;
    if (!jsonParse(line, v) || !v.isObject() || v["e"].str() != "vessel") return false;
    classBOnly = v["b"].type == JsonValue::Bool && v["b"].boolean;
    if (v["ok"].type == JsonValue::Bool && !v["ok"].boolean) return false;
    Data d;
    snprintf(d.name, sizeof(d.name), "%s", v["n"].str().c_str());
    snprintf(d.call, sizeof(d.call), "%s", v["call"].str().c_str());
    snprintf(d.mcall, sizeof(d.mcall), "%s", v["mc"].str().c_str());
    d.mcok    = (int)v["mcok"].num(0);
    d.mmsi    = (uint32_t)v["mmsi"].num(0);
    d.type    = (int)v["t"].num(0);
    d.lengthM = (int)v["len"].num(0);
    d.beamM   = (int)v["beam"].num(0);
    d.draftDm = (int)lround(v["dr"].num(0) * 10.0);
    out = d;
    return true;
}

Action Sync::link(bool up) {
    if (up == up_) return NONE;
    up_ = up;
    pushed_ = false;
    known_ = false;
    state_ = up ? WAITING : NO_LINK;
    return up ? QUERY : NONE;
}

Action Sync::noProfile() {
    pushed_ = false;
    state_ = up_ ? NO_PROFILE : NO_LINK;
    return NONE;
}

Action Sync::remote(const Data& r, bool classBOnly, const Data& local) {
    last_ = r;
    known_ = true;
    classB_ = classBOnly;
    if (pushed_) {                                      // respuesta a lo enviado: lo que quede en el transpondedor es lo que vale
        pushed_ = false;
        if (same(r, local)) { state_ = SYNCED; return NONE; }
        state_ = ADJUSTED;
        return ADOPT;
    }
    if (!configured(local)) { state_ = SYNCED; return configured(r) ? ADOPT : NONE; }
    if (same(r, local)) { state_ = SYNCED; return NONE; }
    pushed_ = true;
    state_ = SENT;
    return PUSH;
}

void Sync::blocked() {
    pushed_ = false;
    state_ = up_ ? NOT_LICENSED : NO_LINK;
}

Action Sync::localChanged(const Data& local) {
    if (!up_) return NONE;
    if (known_ && same(last_, local)) { state_ = SYNCED; return NONE; }
    pushed_ = true;
    state_ = SENT;
    return PUSH;
}

}  // namespace vessel
}  // namespace plotter
