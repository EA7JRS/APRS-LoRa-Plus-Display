/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "tsync.h"

#include <sys/time.h>
#include <time.h>

#include <cstdio>

#ifdef ESP32
#include <esp_sntp.h>
#endif

namespace tsync {

using plotter::ClockSync;
using plotter::TimeSource;

static ClockSync cs;
static volatile bool ntpDone = false;                  // lo marca el cliente SNTP (otra tarea); se apunta en el bucle principal
static bool ntpOn = false;
static char server[40] = "";

static void drain() {
    if (!ntpDone) return;
    ntpDone = false;
    cs.noteSet(TimeSource::Ntp, millis());
}

int64_t now() {
    const time_t t = time(nullptr);
    return t >= ClockSync::kMinValid ? (int64_t)t : 0;
}

#ifdef ESP32
static void onSntp(struct timeval*) { ntpDone = true; }
#endif

void startNtp(const char* s) {
    snprintf(server, sizeof server, "%s", s && s[0] ? s : plotter::kNtpDefault);
#ifdef ESP32
    // Tres servidores: el elegido, el segundo del ROA y uno mundial (si el elegido no es del ROA, el segundo sigue siendolo)
    sntp_set_time_sync_notification_cb(onSntp);
    configTime(0, 0, server, plotter::kNtpSecond, plotter::kNtpFallback);
#endif
    ntpOn = true;
}

bool offer(TimeSource s, int64_t t, int ms) {
    drain();
    if (!cs.offer(s, t, now(), millis())) return false;
#ifdef ESP32
    struct timeval tv;
    tv.tv_sec = (time_t)t;
    tv.tv_usec = (suseconds_t)(ms > 0 && ms < 1000 ? ms * 1000 : 0);
    settimeofday(&tv, nullptr);
#else
    (void)ms;
#endif
    return true;
}

bool offerNmea(const char* line, TimeSource s) {
    int64_t t;
    int ms;
    if (!plotter::nmeaUtc(line, t, ms)) return false;
    return offer(s, t, ms);
}

Status status() {
    drain();
    Status st;
    const uint32_t n = millis();
    st.source = cs.source(n);
    st.ageS = cs.ageS(n);
    st.sets = cs.sets();
    st.ntp = ntpOn;
    snprintf(st.ntpServer, sizeof st.ntpServer, "%s", server);
    return st;
}
}
