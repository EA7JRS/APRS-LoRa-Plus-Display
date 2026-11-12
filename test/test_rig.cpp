/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Pruebas del CAT del reloj. Sin argumentos, lo que no necesita radio. Con "host puerto", contra un rigctld de verdad
 * (por ejemplo la radio de pega de Hamlib: rigctld -m 1 -t 4532 &):
 *   g++ -std=c++17 -I src/hamclock test/test_rig.cpp src/hamclock/hc_rig.cpp -o /tmp/test_rig && /tmp/test_rig 127.0.0.1 4532
 */
#include "hc_rig.h"
#include <arpa/inet.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALLA linea %d: %s\n", __LINE__, #c); } } while (0)

static bool sSend(void* ctx, const char* t) { return write(*(int*)ctx, t, strlen(t)) == (ssize_t)strlen(t); }
static bool sLine(void* ctx, char* buf, size_t n, int timeoutMs) {
    size_t len = 0;
    for (;;) {
        pollfd p{*(int*)ctx, POLLIN, 0};
        if (poll(&p, 1, timeoutMs) <= 0) return false;
        char c;
        if (read(*(int*)ctx, &c, 1) != 1) return false;
        if (c == '\n') { buf[len] = 0; return true; }
        if (c != '\r' && len < n - 1) buf[len++] = c;
    }
}

int main(int argc, char** argv) {
    using namespace hcrig;
    char host[32];
    int port;
    CHECK(parseAddress("192.168.1.50", host, sizeof(host), port) && port == 4532 && !strcmp(host, "192.168.1.50"));
    CHECK(parseAddress(" rig.local:4533", host, sizeof(host), port) && port == 4533 && !strcmp(host, "rig.local"));
    CHECK(!parseAddress("", host, sizeof(host), port)); CHECK(!parseAddress("host:0", host, sizeof(host), port));
    CHECK(!strcmp(modeFor("SSB", 7.155), "LSB")); CHECK(!strcmp(modeFor("SSB", 14.205), "USB")); CHECK(!strcmp(modeFor("SSB", 5.357), "USB"));
    CHECK(!strcmp(modeFor("FT8", 14.074), "PKTUSB")); CHECK(!strcmp(modeFor("CW", 7.012), "CW")); CHECK(!strcmp(modeFor("FM", 145.5), "FM"));
    CHECK(!strcmp(modeFor("", 14.2), "")); CHECK(!strcmp(modeFor("XYZ", 14.2), ""));

    if (argc == 3) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{};
        a.sin_family = AF_INET; a.sin_port = htons((uint16_t)atoi(argv[2])); inet_pton(AF_INET, argv[1], &a.sin_addr);
        if (connect(fd, (sockaddr*)&a, sizeof(a))) { printf("no hay rigctld en %s:%s\n", argv[1], argv[2]); return 1; }
        Io io{sSend, sLine, &fd};
        double hz = 0;
        char mode[8] = "";
        CHECK(getFreq(io, hz) && hz > 0); CHECK(getMode(io, mode) && mode[0]);
        printf("radio: %.0f Hz %s\n", hz, mode);
        CHECK(tune(io, 7.155, "SSB")); CHECK(getFreq(io, hz) && fabs(hz - 7155000) < 1); CHECK(getMode(io, mode) && !strcmp(mode, "LSB"));
        CHECK(tune(io, 14.074, "FT8")); CHECK(getFreq(io, hz) && fabs(hz - 14074000) < 1); CHECK(getMode(io, mode) && !strcmp(mode, "PKTUSB"));
        CHECK(tune(io, 21.025, "CW")); CHECK(getFreq(io, hz) && fabs(hz - 21025000) < 1); CHECK(getMode(io, mode) && !strcmp(mode, "CW"));
        CHECK(tune(io, 28.495, "")); CHECK(getFreq(io, hz) && fabs(hz - 28495000) < 1); CHECK(getMode(io, mode) && !strcmp(mode, "CW"));   // sin modo: no lo toca
        close(fd);
    }
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
