#!/usr/bin/env python3
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Compara los pases que calcula el reloj (propagador de Kepler + J2 de src/hamclock/hc_model.cpp) con SGP4 (pip install sgp4).
#   python3 tools/hamclock_sim/check_passes.py /tmp/test_hamclock
import math, subprocess, sys
from sgp4.api import Satrec, WGS72, jday

exe = sys.argv[1]
LAT, LON, T0 = 37.39, -5.98, 1794355200            # Sevilla, 2026-11-11 00:00 UTC
EPOCH_ISO, EPOCH_UNIX = "2026-11-09T06:00:00.000", 1794355200 - 2 * 86400 + 6 * 3600   # elementos de casi dos dias
SATS = {  # rev/dia, excentricidad, inclinacion, nodo, perigeo, anomalia media, bstar
    "ISS": (15.50, 0.0005, 51.64, 100.0, 50.0, 200.0, 2.0e-4),
    "SO-50": (14.76, 0.0060, 64.55, 10.0, 20.0, 30.0, 8.0e-5),
    "RS-44": (12.80, 0.0218, 82.52, 250.0, 300.0, 120.0, 1.0e-5),
    "AO-7": (12.54, 0.0012, 101.99, 330.0, 80.0, 280.0, 1.0e-5),
}
D = math.pi / 180

def gmst(jd):
    return math.radians((280.46061837 + 360.98564736629 * (jd - 2451545.0)) % 360)

def elev(sat, t):
    jd = t / 86400 + 2440587.5
    e, r, _ = sat.sgp4(math.floor(jd - 0.5) + 0.5, (jd - 0.5) % 1)
    th = gmst(jd) + LON * D
    # observador sobre el elipsoide WGS84 (km)
    f, a = 1 / 298.257223563, 6378.137
    C = 1 / math.sqrt(1 + f * (f - 2) * math.sin(LAT * D) ** 2); S = (1 - f) ** 2 * C
    o = (a * C * math.cos(LAT * D) * math.cos(th), a * C * math.cos(LAT * D) * math.sin(th), a * S * math.sin(LAT * D))
    up = (math.cos(LAT * D) * math.cos(th), math.cos(LAT * D) * math.sin(th), math.sin(LAT * D))
    d = [r[i] - o[i] for i in range(3)]
    return math.degrees(math.asin(sum(d[i] * up[i] for i in range(3)) / math.sqrt(sum(x * x for x in d))))

worst_t = worst_el = 0
for name, (mm, ecc, inc, raan, argp, ma, bstar) in SATS.items():
    sat = Satrec()
    sat.sgp4init(WGS72, "i", 1, EPOCH_UNIX / 86400 + 2440587.5 - 2433281.5, bstar, 0.0, 0.0, ecc, argp * D, inc * D, ma * D,
                 mm * 2 * math.pi / 1440, raan * D)
    ref, up, best, aos = [], False, 0, 0
    for k in range(24 * 3600 // 5 + 1):
        t = T0 + k * 5
        el = elev(sat, t)
        if el > 0:
            if not up: up, best, aos = True, el, t
            best = max(best, el)
        elif up:
            up = False
            if best >= 10: ref.append((aos, best))
    out = subprocess.run([exe, "pass", EPOCH_ISO, str(mm), str(ecc), str(inc), str(raan), str(argp), str(ma), str(T0), str(LAT), str(LON)],
                         capture_output=True, text=True).stdout.split("\n")
    mine = [(int(l.split()[0]), float(l.split()[1])) for l in out if l.strip()]
    print("%-6s SGP4: %d pases en 24 h; el reloj da los %d primeros" % (name, len(ref), len(mine)))
    for (a, e), (ra, re_) in zip(mine, ref):
        print("   AOS %+4d s   elevacion maxima %5.1f / %5.1f" % (a - ra, e, re_))
        worst_t = max(worst_t, abs(a - ra)); worst_el = max(worst_el, abs(e - re_))
    if len(mine) != min(len(ref), 6): print("   OJO: numero de pases distinto"); worst_t = 9999
print("peor diferencia: %d s en el AOS, %.1f grados en la elevacion maxima" % (worst_t, worst_el))
sys.exit(0 if worst_t <= 90 and worst_el <= 3 else 1)
