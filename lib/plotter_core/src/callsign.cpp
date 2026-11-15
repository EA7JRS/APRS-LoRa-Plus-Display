/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "callsign.h"
#include "minijson.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace plotter {
namespace callsign {

// Prefijo (1-2 letras, cifra+letra o letra+cifra), una cifra y un sufijo de 0-3 + 1 caracteres que acaba en letra
static bool shape(const char* b) {
    const size_t n = strlen(b);
    auto L = [&](size_t i) { return i < n && b[i] >= 'A' && b[i] <= 'Z'; };
    auto D = [&](size_t i) { return i < n && b[i] >= '0' && b[i] <= '9'; };
    auto suffix = [&](size_t i) {                               // [0-9A-Z]{0,3}[A-Z]$
        if (i >= n || n - i > 4) return false;
        for (size_t k = i; k < n; k++) if (!L(k) && !D(k)) return false;
        return L(n - 1);
    };
    // las tres formas del prefijo, cada una seguida de la cifra
    if (L(0) && D(1) && suffix(2)) return true;                 // [A-Z] + cifra
    if (L(0) && L(1) && D(2) && suffix(3)) return true;         // [A-Z]{2} + cifra
    if (D(0) && L(1) && D(2) && suffix(3)) return true;         // [0-9][A-Z] + cifra
    if (L(0) && D(1) && D(2) && suffix(3)) return true;         // [A-Z][0-9] + cifra
    return false;
}

bool base(const char* call, char* out, size_t size) {
    if (!call || size < 2) return false;
    while (*call == ' ') call++;
    char b[16];
    size_t k = 0;
    for (; call[k] && call[k] != '-' && call[k] != ' ' && k + 1 < sizeof(b); k++) b[k] = (char)toupper((unsigned char)call[k]);
    b[k] = 0;
    if (!strcmp(b, "N0CALL") || !strcmp(b, "NOCALL") || !shape(b) || strlen(b) + 1 > size) return false;
    memcpy(out, b, strlen(b) + 1);
    return true;
}

std::string url(const char* b) { return std::string("https://database.radioid.net/api/dmr/user/?callsign=") + b; }

bool Result::has(uint32_t id) const {
    for (int i = 0; i < nIds; i++) if (ids[i] == id) return true;
    return false;
}

void Result::idsText(char* out, size_t size) const {
    size_t n = 0;
    out[0] = 0;
    for (int i = 0; i < nIds && n + 12 < size; i++) n += (size_t)snprintf(out + n, size - n, i ? ",%lu" : "%lu", (unsigned long)ids[i]);
}

bool parse(const std::string& json, const char* b, Result& out) {
    JsonValue v;
    if (!jsonParse(json, v) || !v.isObject() || !v["results"].isArray()) return false;
    Result r;
    const JsonValue& list = v["results"];
    for (size_t i = 0; i < list.size(); i++) {
        std::string c = list[i]["callsign"].str();
        while (!c.empty() && c.back() == ' ') c.pop_back();
        while (!c.empty() && c.front() == ' ') c.erase(0, 1);
        bool same = c.size() == strlen(b);
        for (size_t k = 0; same && k < c.size(); k++) same = toupper((unsigned char)c[k]) == b[k];
        if (!same) continue;
        if (!r.found) {
            std::string name = list[i]["fname"].str();
            const std::string sur = list[i]["surname"].str();
            if (!sur.empty()) name += name.empty() ? sur : " " + sur;
            snprintf(r.name, sizeof(r.name), "%s", name.c_str());
            snprintf(r.country, sizeof(r.country), "%s", list[i]["country"].str().c_str());
        }
        r.found = true;
        const double id = list[i]["id"].num(-1);
        if (id > 0 && r.nIds < 8) r.ids[r.nIds++] = (uint32_t)id;
    }
    out = r;
    return true;
}

bool valid(const char* call, const Licence& l) {
    char b[10];
    return l.call[0] && base(call, b, sizeof(b)) && !strcmp(b, l.call);
}

bool needsRecheck(const Licence& l, int64_t nowUnix) {
    if (!l.call[0]) return false;
    if (l.atUnix <= 0) return nowUnix > 0;
    return nowUnix - l.atUnix >= RECHECK_S;
}

const char* check(const char* call, const Result* r, uint32_t expectedId, int64_t nowUnix, Licence& l) {
    char b[10];
    if (!base(call, b, sizeof(b))) return "No es un indicativo de radioaficionado";
    if (!r) return "Respuesta de RadioID inesperada";
    if (!r->found) return "Sin DMR ID / CCS7 en RadioID.net (reg\xC3\xADstralo all\xC3\xAD primero)";
    if (expectedId && !r->has(expectedId)) return "Ese DMR ID / CCS7 no es de este indicativo en RadioID.net";
    Licence n;
    snprintf(n.call, sizeof(n.call), "%s", b);
    r->idsText(n.ids, sizeof(n.ids));
    snprintf(n.name, sizeof(n.name), "%s", r->name);
    snprintf(n.country, sizeof(n.country), "%s", r->country);
    n.atUnix = nowUnix > 0 ? nowUnix : 0;
    l = n;
    return nullptr;
}

}  // namespace callsign
}  // namespace plotter
