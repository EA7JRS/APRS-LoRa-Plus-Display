/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ Plotter.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "stream.h"

namespace plotter {

bool LineBuffer::push(char c) {
    if (done_) { len_ = 0; done_ = false; }
    if (c == '\n' || c == '\r') {
        if (overflow_) { overflow_ = false; len_ = 0; dropped_++; return false; }
        if (len_ == 0) return false;
        buf_[len_] = 0;
        done_ = true;
        return true;
    }
    if (overflow_) return false;
    if (len_ >= kMax - 1) { overflow_ = true; return false; }
    buf_[len_++] = c;
    return false;
}

bool DupFilter::seen(const char* line, uint32_t now) {
    uint32_t h = 2166136261u;                           // FNV-1a
    for (const char* p = line; *p; p++) { h ^= uint8_t(*p); h *= 16777619u; }
    for (int i = 0; i < kSlots; i++)
        if (used_[i] && hash_[i] == h && uint32_t(now - time_[i]) <= windowMs_) return true;
    hash_[next_] = h; time_[next_] = now; used_[next_] = true;
    next_ = (next_ + 1) % kSlots;
    return false;
}

}  // namespace plotter
