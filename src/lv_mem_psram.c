// APRS LoRa+ Display - Copyright (C) 2026 Manuel - EA7JRS
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Memoria de LVGL en la PSRAM (LV_USE_STDLIB_MALLOC = LV_STDLIB_CUSTOM en lv_defs.h). Si la PSRAM falta o se llena, la RAM
// interna. Los buffers de dibujo y del panel no pasan por aqui (board::allocDrawBuffer y el controlador del panel).
#include <lvgl.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM
#include <stdlib.h>
#include <esp_heap_caps.h>

#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void* mem, size_t bytes) { LV_UNUSED(mem); LV_UNUSED(bytes); return NULL; }
void lv_mem_remove_pool(lv_mem_pool_t pool) { LV_UNUSED(pool); }

void* lv_malloc_core(size_t size) {
    void* p = heap_caps_malloc(size, PSRAM_CAPS);
    return p ? p : malloc(size);
}

void* lv_realloc_core(void* p, size_t new_size) {
    void* q = heap_caps_realloc(p, new_size, PSRAM_CAPS);
    return q ? q : realloc(p, new_size);
}

void lv_free_core(void* p) { heap_caps_free(p); }

void lv_mem_monitor_core(lv_mem_monitor_t* mon) {
    mon->total_size = heap_caps_get_total_size(PSRAM_CAPS);
    mon->free_size = heap_caps_get_free_size(PSRAM_CAPS);
    mon->free_biggest_size = heap_caps_get_largest_free_block(PSRAM_CAPS);
    mon->max_used = mon->total_size - heap_caps_get_minimum_free_size(PSRAM_CAPS);
    mon->used_pct = mon->total_size ? (uint8_t)(100 - mon->free_size * 100 / mon->total_size) : 0;
}

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
#endif
