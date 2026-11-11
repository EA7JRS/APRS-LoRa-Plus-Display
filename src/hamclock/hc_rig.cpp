/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "hc_rig.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace hcrig {

static const int TIMEOUT_MS = 2000;

bool parseAddress(const char* text, char* host, size_t n, int& port) {
    port = 4532;
    if (!text) return false;
    while (*text == ' ') text++;
    const char* colon = strrchr(text, ':');
    size_t len = colon ? (size_t)(colon - text) : strlen(text);
    while (len && text[len - 1] == ' ') len--;
    if (!len || len >= n) return false;
    memcpy(host, text, len);
    host[len] = 0;
    if (colon) {
        port = atoi(colon + 1);
        if (port < 1 || port > 65535) return false;
    }
    return true;
}

// respuesta de error de rigctld: "RPRT -11"
static bool isRprt(const char* line, int& code) {
    if (strncmp(line, "RPRT", 4)) return false;
    code = atoi(line + 4);
    return true;
}
bool getFreq(Io& io, double& hz) {
    char b[48];
    int code;
    if (!io.send(io.ctx, "f\n") || !io.line(io.ctx, b, sizeof(b), TIMEOUT_MS) || isRprt(b, code)) return false;
    char* end = nullptr;
    double v = strtod(b, &end);
    if (end == b || v <= 0) return false;
    hz = v;
    return true;
}
bool getMode(Io& io, char mode[8]) {
    char b[48], pb[48];
    int code;
    if (!io.send(io.ctx, "m\n") || !io.line(io.ctx, b, sizeof(b), TIMEOUT_MS) || isRprt(b, code)) return false;
    if (!io.line(io.ctx, pb, sizeof(pb), TIMEOUT_MS)) return false;      // segunda linea: el ancho de banda
    strncpy(mode, b, 7);
    mode[7] = 0;
    return true;
}
static int command(Io& io, const char* cmd) {
    char b[48];
    int code;
    if (!io.send(io.ctx, cmd) || !io.line(io.ctx, b, sizeof(b), TIMEOUT_MS)) return -1000;
    return isRprt(b, code) ? code : -1000;
}
int setFreq(Io& io, double hz) {
    char c[40];
    snprintf(c, sizeof(c), "F %.0f\n", hz);
    return command(io, c);
}
int setMode(Io& io, const char* mode) {
    char c[40];
    snprintf(c, sizeof(c), "M %s 0\n", mode);        // 0 = el ancho de banda normal del modo
    return command(io, c);
}

const char* modeFor(const char* m, double mhz) {
    if (!m || !*m) return "";
    if (!strcmp(m, "CW")) return "CW";
    if (!strcmp(m, "SSB")) return (mhz < 10 && !(mhz > 5.2 && mhz < 5.5)) ? "LSB" : "USB";
    if (!strcmp(m, "USB") || !strcmp(m, "LSB") || !strcmp(m, "FM") || !strcmp(m, "AM") || !strcmp(m, "RTTY")) return m;
    for (const char* d : {"FT8", "FT4", "JS8", "PSK", "DIGI", "DATA", "SSTV", "PSK31", "MFSK", "OLIVIA"})
        if (!strcmp(m, d)) return "PKTUSB";
    return "";
}
bool tune(Io& io, double mhz, const char* spotMode) {
    if (setFreq(io, round(mhz * 1e6)) != 0) return false;
    const char* m = modeFor(spotMode, mhz);
    if (*m && setMode(io, m) != 0 && !strcmp(m, "PKTUSB")) setMode(io, "USB");
    return true;
}

}
