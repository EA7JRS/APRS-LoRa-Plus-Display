// APRS LoRa+ Display - Copyright (C) 2026 Manuel - EA7JRS
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Memoria de LVGL (LV_USE_STDLIB_MALLOC = LV_STDLIB_CUSTOM en lv_defs.h). Los objetos pequenos van a la RAM interna mientras
// quede margen para WiFi, Bluetooth y TLS (LV_INTERNAL_RESERVE libres despues de reservar); el resto, a la PSRAM. Todo en la
// PSRAM era estable pero lento, y en el panel RGB (que lee su imagen de la PSRAM) salian rayas en el borde izquierdo: el
// dibujo le quitaba ancho de banda. Todo en la RAM interna (el malloc de la libreria C) la agotaba al arrancar la v2.
// Los buffers de dibujo y del panel no pasan por aqui (board::allocDrawBuffer y el controlador del panel).
#include <lvgl.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM
#include <stdlib.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>

#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void* mem, size_t bytes) { LV_UNUSED(mem); LV_UNUSED(bytes); return NULL; }
void lv_mem_remove_pool(lv_mem_pool_t pool) { LV_UNUSED(pool); }

#ifndef LV_INTERNAL_RESERVE
#define LV_INTERNAL_RESERVE (80 * 1024)   // RAM interna que se deja libre para el sistema
#endif
#ifndef LV_INTERNAL_MAX
#define LV_INTERNAL_MAX 2048              // reservas mayores, a la PSRAM (imagenes, capas, buffers)
#endif
#define INTERNAL_CAPS (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)

static int internalFits(size_t size) {
    return size <= LV_INTERNAL_MAX && heap_caps_get_free_size(INTERNAL_CAPS) > LV_INTERNAL_RESERVE + size;
}

void* lv_malloc_core(size_t size) {
    void* p = internalFits(size) ? heap_caps_malloc(size, INTERNAL_CAPS) : NULL;
    if (!p) p = heap_caps_malloc(size, PSRAM_CAPS);
    return p ? p : heap_caps_malloc(size, INTERNAL_CAPS);
}

void* lv_realloc_core(void* p, size_t new_size) {
    if (!p) return lv_malloc_core(new_size);
    void* q = NULL;
    if (esp_ptr_internal(p) && internalFits(new_size)) q = heap_caps_realloc(p, new_size, INTERNAL_CAPS);
    if (!q) q = heap_caps_realloc(p, new_size, PSRAM_CAPS);
    return q ? q : heap_caps_realloc(p, new_size, INTERNAL_CAPS);
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
