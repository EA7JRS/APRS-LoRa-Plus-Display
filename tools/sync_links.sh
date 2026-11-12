#!/usr/bin/env bash
# Copyright (C) 2026 Manuel - EA7JRS
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copia en lib/ las dos librerias comunes de APRS LoRa+ que nacen en el plotter (EA7JRS/APRS-LoRa-Plus-Plotter):
#   lib/plotter_core  nucleo en C++ puro (NMEA 0183, AIS, NMEA 2000, Signal K, tabla de datos con prioridad de fuentes...)
#   lib/aprs_links    WiFi, cliente TCP (NMEA / gpsd / Signal K), escucha UDP, BLE NMEA y lector JSON de Signal K
# No se editan aqui: se cambian en el plotter (con sus 1500 comprobaciones) y se vuelven a copiar con este guion.
# Uso: tools/sync_links.sh <checkout del plotter>
set -euo pipefail
SRC=${1:?uso: tools/sync_links.sh <checkout de APRS-LoRa-Plus-Plotter>}
DST=$(cd "$(dirname "$0")/.." && pwd)/lib
for l in plotter_core aprs_links; do
    [ -f "$SRC/lib/$l/library.json" ] || { echo "no encuentro $SRC/lib/$l"; exit 1; }
    rm -rf "$DST/$l"
    mkdir -p "$DST/$l"
    cp -r "$SRC/lib/$l/." "$DST/$l/"
done
rev=$(git -C "$SRC" rev-parse --short HEAD 2>/dev/null || echo "?")
printf 'Copiado de EA7JRS/APRS-LoRa-Plus-Plotter, commit %s, con tools/sync_links.sh. No editar aqui.\n' "$rev" > "$DST/ORIGEN.txt"
echo "lib/plotter_core y lib/aprs_links copiados del plotter ($rev)"
