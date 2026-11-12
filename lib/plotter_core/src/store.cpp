/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "store.h"

namespace plotter {

uint32_t DataStore::timeoutOf(Source s) const { return uint8_t(s) < 6 ? to_[uint8_t(s)] : 0; }
void DataStore::setTimeout(Source s, uint32_t ms) { if (uint8_t(s) < 6) to_[uint8_t(s)] = ms; }

bool DataStore::fresh(const Entry& e, uint32_t now) const {
    return e.s != Source::None && uint32_t(now - e.t) <= timeoutOf(e.s);       // la resta sin signo aguanta el desbordamiento de millis()
}

bool DataStore::set(Key k, double v, Source s, uint32_t now, Quality q) {
    Entry& e = e_[int(k)];
    if (fresh(e, now) && uint8_t(s) > uint8_t(e.s)) return false;
    e.v = v; e.t = now; e.s = s; e.q = q;
    return true;
}

bool DataStore::get(Key k, uint32_t now, double& v) const {
    const Entry& e = e_[int(k)];
    if (!fresh(e, now)) return false;
    v = e.v;
    return true;
}

Source DataStore::source(Key k, uint32_t now) const { return fresh(e_[int(k)], now) ? e_[int(k)].s : Source::None; }
uint32_t DataStore::ageMs(Key k, uint32_t now) const { return uint32_t(now - e_[int(k)].t); }

}  // namespace plotter
