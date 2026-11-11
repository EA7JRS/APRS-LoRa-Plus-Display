/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz tipo emisora (entornos waveshare_lcd7b_v2 y waveshare_lcd7_v2, -DUI_RADIO): la de la app Android APRS LoRa+
 * (Shell.kt, RadioScreen.kt...) adaptada a 1024 x 600 en horizontal. En la LCD-7 (800 x 480) todo el codigo sigue
 * trabajando en esas medidas (1024 x 614) y se reduce a 25/32 al llegar a LVGL (R7_SCALED, mas abajo). Barra de estado, bandas A (tracker) y B (CAT de la caja del rotor),
 * teclas tactiles [F] + 5, menu [F] de tres paginas y menu lateral. Comun a todas las pantallas.
 */
#pragma once
#ifdef UI_RADIO
#include <lvgl.h>
#include <string>
#include <vector>
#include "protocol.h"
#include "rotor_proto.h"
#include "dxcluster.h"

#if defined(BOARD_LCD7)
// LCD-7, 800 x 480: fuentes y simbolos a 25/32 (tools/make_fonts_7b.sh, tools/make_symbols.py 22)
#define R7_SCALED 1
#define app_text_18 app_text_14
#define app_text_20 app_text_16
#define app_text_24 app_text_19
#define app_semi_20 app_semi_16
#define app_cond_16 app_cond_13
#define app_cond_18 app_cond_14
#define app_bold_18 app_bold_14
#define app_bold_22 app_bold_17
#define app_bold_26 app_bold_20
#define app_bold_34 app_bold_27
#define app_mono_20 app_mono_16
#define app_mono_28 app_mono_22
#define app_mono_40 app_mono_31
#define app_mono_64 app_mono_50
#define R7_ICON_L lv_font_montserrat_20
#define R7_ICON_M lv_font_montserrat_16
#else
#define R7_ICON_L lv_font_montserrat_24
#define R7_ICON_M lv_font_montserrat_20
#endif
LV_FONT_DECLARE(app_text_18)
LV_FONT_DECLARE(app_text_20)
LV_FONT_DECLARE(app_text_24)
LV_FONT_DECLARE(app_semi_20)
LV_FONT_DECLARE(app_cond_16)
LV_FONT_DECLARE(app_cond_18)
LV_FONT_DECLARE(app_bold_18)
LV_FONT_DECLARE(app_bold_22)
LV_FONT_DECLARE(app_bold_26)
LV_FONT_DECLARE(app_bold_34)
LV_FONT_DECLARE(app_mono_20)
LV_FONT_DECLARE(app_mono_28)
LV_FONT_DECLARE(app_mono_40)
LV_FONT_DECLARE(app_mono_64)

namespace r7 {

// ---------- paleta (Theme.kt de la app: DARK y LIGHT) ----------
struct Pal {
    uint32_t bg, scrim, panel, panelBorder, subPanel, subBorder, key, keyBorder, selected, green, greenDim, greenBg, cyan, amber, amberDim, red,
        redSoft, text, muted, faint, subText, subMuted, off, symbolBg, outline, skyBg, bandB;
    bool dark;
};
extern const Pal* P;
inline lv_color_t C(uint32_t v) { return lv_color_hex(v); }

// medidas de la 7B. Con R7_SCALED son medidas de dibujo: px() las pasa a puntos reales y las funciones de LVGL que
// reciben medidas quedan envueltas, asi que las pantallas no cambian. Para puntos reales: (lv_obj_set_size)(...).
#ifdef R7_SCALED
constexpr int W = 1024, H = 614, SYM = 22;
inline int32_t px(int32_t v) {
    if (!LV_COORD_IS_PX(v) || v == 0) return v;
    int32_t r = v > 0 ? (v * 25 + 16) / 32 : -((-v * 25 + 16) / 32);
    return r == 0 ? (v > 0 ? 1 : -1) : r;
}
#else
constexpr int W = 1024, H = 600, SYM = 28;
inline int32_t px(int32_t v) { return v; }
#endif
constexpr int STATUS_H = 54, KEYS_H = 80, PAD = 12, GAP = 10, RADIUS = 14;
constexpr int CONTENT_H = H - STATUS_H - KEYS_H;          // con las teclas
constexpr int CONTENT_FULL = H - STATUS_H;                // sin ellas

// ---------- datos ----------
extern proto::Model* M;
extern rotp::Model RM;
struct DxStore { std::vector<dx::Spot> spots; uint32_t seq = 0; std::vector<std::string> console; };
extern DxStore DX;
uint32_t nowMs();
// hora UTC: de internet (NTP) o la del GNSS del tracker; false si no se sabe
bool utcNow(int& y, int& mo, int& d, int& h, int& mi, int& s);
// posicion propia: GNSS del tracker
bool ownPos(double& lat, double& lon);
// mi indicativo: el del perfil de baliza en uso del tracker (o el del DX Cluster)
std::string myCall();
std::string upper(std::string s);

// ---------- navegacion ----------
enum Scr { RADIO, FMENU, DISP, SLIST, STATION, MAP, WX, MSGS, THREAD, PAGER, DXC, TRACKER, SENSORS, SAT, ROTOR, AIS, REMOTE, CONFIG,
           FIRMWARE, SETTINGS, ABOUT, SCR_COUNT };
struct ScreenDef {
    const char* title;
    bool keys;                                  // con teclas tactiles abajo
    void (*build)(lv_obj_t* root);              // una vez, la primera vez que se abre
    void (*refresh)();                          // con cada cambio, solo si esta a la vista
    void (*enter)();                            // al abrirla (pedir datos)
    void (*leave)();
};
void registerScreen(Scr s, const ScreenDef& d);
void go(Scr s, const std::string& arg = "");
void back();
Scr top();
const std::string& arg();
void openSideMenu();
void requestRefresh();                          // repintar en la siguiente vuelta aunque el modelo no cambie

// ---------- trackers y avisos ----------
void send(const std::string& line);             // orden al tracker (si no hay enlace, avisa)
bool linked();
void notice(const char* t);
void error(const char* t);
void confirm(const char* title, const char* text, const char* ok, void (*cb)(void*), void* ud = nullptr, bool danger = false);

// ---------- piezas ----------
lv_obj_t* box(lv_obj_t* p);                                         // contenedor sin estilo, alto segun el contenido
lv_obj_t* col(lv_obj_t* p, int gap = GAP);
lv_obj_t* row(lv_obj_t* p, int gap = GAP);
lv_obj_t* panel(lv_obj_t* p, int pad = 14, uint32_t bg = 0xFFFFFFFF, uint32_t border = 0xFFFFFFFF);
lv_obj_t* text(lv_obj_t* p, const char* t, const lv_font_t* f = &app_text_20, uint32_t color = 0xFFFFFFFF);
lv_obj_t* wrap(lv_obj_t* p, const char* t, const lv_font_t* f = &app_text_18, uint32_t color = 0xFFFFFFFF);
lv_obj_t* section(lv_obj_t* p, const char* t);                      // SectionLabel: mayusculas espaciadas en gris
void setText(lv_obj_t* l, const char* t);
void setTextf(lv_obj_t* l, const char* fmt, ...) LV_FORMAT_ATTRIBUTE(2, 3);
void setColor(lv_obj_t* l, uint32_t c);
void show(lv_obj_t* o, bool on);
void grow(lv_obj_t* o, int g = 1);
void oneLine(lv_obj_t* l);                                          // una linea con ... al final (alto fijo)

struct Key {                                                        // RadioKey: rotulo en negrita y una linea debajo
    lv_obj_t *obj = nullptr, *lab = nullptr, *sub = nullptr;
    void set(const char* label, const char* sub = nullptr);
    void style(uint32_t color, uint32_t border, uint32_t subColor = 0xFFFFFFFF, uint32_t bg = 0xFFFFFFFF);
};
Key key(lv_obj_t* p, const char* label, const char* sub, int w, int h, lv_event_cb_t cb, void* ud = nullptr, const lv_font_t* f = &app_bold_22);
lv_obj_t* chip(lv_obj_t* p, const char* label, bool sel, lv_event_cb_t cb, void* ud = nullptr, int h = 40, uint32_t color = 0xFFFFFFFF);
void chipSet(lv_obj_t* c, bool sel, uint32_t color = 0xFFFFFFFF);
lv_obj_t* button(lv_obj_t* p, const char* label, lv_event_cb_t cb, void* ud = nullptr, int w = 0, int h = 52, uint32_t color = 0xFFFFFFFF);
void buttonColor(lv_obj_t* b, uint32_t color);
lv_obj_t* lamp(lv_obj_t* p, int size = 10);
void lampSet(lv_obj_t* l, bool on, uint32_t color = 0xFFFFFFFF);

struct Led { lv_obj_t *halo, *dot, *lab; uint32_t color; void set(bool on, bool ring = false); };
Led led(lv_obj_t* p, const char* label, uint32_t color);

struct SMeter {                                                     // SMeterBar: 9 S + 6 de 10 dB, colores IARU, lectura
    lv_obj_t* obj;
    lv_obj_t* seg[15];
    lv_obj_t *reading, *extra;
    void set(const proto::SReading* r, const char* extraText);
};
SMeter smeter(lv_obj_t* p, int w);

struct Tile { lv_obj_t *obj, *val, *unit; void set(const char* v, uint32_t color = 0xFFFFFFFF); };   // DataTile
Tile tile(lv_obj_t* p, const char* label, const char* unit, int w);

struct Rows {                                                       // InfoRow: clave en gris y valor; las filas se reutilizan
    lv_obj_t* box = nullptr;
    std::vector<lv_obj_t*> r;
    size_t n = 0;
    int keyW = 170;
    const lv_font_t* font = &app_mono_20;
    void create(lv_obj_t* p, int keyWidth = 170);
    void begin() { n = 0; }
    void add(const char* k, const char* v, uint32_t color = 0xFFFFFFFF);
    void addf(const char* k, uint32_t color, const char* fmt, ...) LV_FORMAT_ATTRIBUTE(4, 5);
    void end();
};

// cabecera de las pantallas que no son RADIO: < titulo ... (y a la derecha lo que se ponga en *trailing)
lv_obj_t* header(lv_obj_t* root, const char* title, lv_obj_t** trailing = nullptr);
// zona desplazable bajo la cabecera, con el alto que queda
lv_obj_t* scrollArea(lv_obj_t* root);

// campo de texto con teclado
lv_obj_t* field(lv_obj_t* p, const char* placeholder, int w, int maxLen, const char* accepted = nullptr, bool upperCase = false);
void keyboardFor(lv_obj_t* ta);

// simbolo APRS (hojas de aprs.fi, 28 px)
lv_obj_t* symbol(lv_obj_t* p);
void symbolSet(lv_obj_t* s, char table, char code);
const char* symbolName(char table, char code);
// el simbolo dibujado a mano (mapa), centrado en x, y (puntos reales)
void drawSymbol(lv_layer_t* layer, int x, int y, char table, char code);

// lista de filas reutilizables (S.LIST, mensajes, DX...): make crea una fila vacia; el clic llega con el indice en user_data
struct List {
    lv_obj_t* box = nullptr;
    std::vector<lv_obj_t*> rows;
    lv_obj_t* (*make)(lv_obj_t* parent) = nullptr;
    lv_event_cb_t onClick = nullptr;
    size_t n = 0;
    void create(lv_obj_t* p, lv_obj_t* (*mk)(lv_obj_t*), lv_event_cb_t click);
    void begin() { n = 0; }
    lv_obj_t* next();
    void end();
};
int clickIndex(lv_event_t* e);

// brujula (CompassRose de la app): norte o rumbo arriba, objetivo en ambar, rotor en cian, rumbo propio en verde
struct Compass {
    lv_obj_t* obj;
    float up = 0, target = 0, rotor = 0, own = 0;
    bool hasTarget = false, hasRotor = false, hasOwn = false;
    void set(float upDeg, const float* targetDeg, const float* rotorDeg, const float* ownDeg);
};
Compass* compass(lv_obj_t* p, int size);

// formato
std::string fmtAge(int s);
std::string fmtKm(double km);
std::string fmtDeg(double d);
uint32_t tempColor(float c);
// frecuencia grande: "433.775" + "000"
struct Freq { lv_obj_t *obj, *main, *sub; void set(uint32_t hz, uint32_t color = 0xFFFFFFFF); };
Freq freq(lv_obj_t* p, const lv_font_t* big, const lv_font_t* small);

// ---------- pantallas ----------
void initRadio();       // RADIO, FMENU, DISP
void initStations();    // SLIST, STATION, MAP, WX
void initMessages();    // MSGS, THREAD, PAGER
void initTracker();     // TRACKER, SENSORS
void initSat();         // SAT
void initRotor();       // ROTOR
void initTools();       // AIS, REMOTE, CONFIG, FIRMWARE, SETTINGS, ABOUT
void initDx();          // DXC

// teclas del menu [F] y tactiles
struct FKey { const char* id; const char* label; void (*run)(); };
const std::vector<FKey>& fkeys();
const FKey* fkey(const std::string& id);
std::string fkeySub(const std::string& id, int& lit);   // texto de debajo y si se enciende (1 verde, 2 ambar)
void refreshTouchKeys();

// la banda A y B compactas (menu [F])
lv_obj_t* bandSummary(lv_obj_t* p);
void bandSummaryRefresh();

// lo que tiene que hacer la pantalla cada segundo aunque el modelo no cambie (luces, relojes)
void tick1s();
void fastStep();          // luces de la banda A (cada vuelta)
void pumpLinks();         // lo que llega de la caja del rotor y del DX Cluster

// el rotor sigue lo que toca (punteria del tracker o estacion): lo llama update()
void rotorFollowStep();
// el tracker en SAT: Doppler al CAT de la caja
void catDopplerStep();

} // namespace r7

#ifdef R7_SCALED
#define lv_obj_set_size(o, w, h) (lv_obj_set_size)(o, r7::px(w), r7::px(h))
#define lv_obj_set_width(o, w) (lv_obj_set_width)(o, r7::px(w))
#define lv_obj_set_height(o, h) (lv_obj_set_height)(o, r7::px(h))
#define lv_obj_set_pos(o, x, y) (lv_obj_set_pos)(o, r7::px(x), r7::px(y))
#define lv_obj_align(o, a, x, y) (lv_obj_align)(o, a, r7::px(x), r7::px(y))
#define lv_obj_set_ext_click_area(o, v) (lv_obj_set_ext_click_area)(o, r7::px(v))
#define lv_obj_scroll_to_y(o, y, a) (lv_obj_scroll_to_y)(o, r7::px(y), a)
#define lv_obj_set_style_radius(o, v, s) (lv_obj_set_style_radius)(o, r7::px(v), s)
#define lv_obj_set_style_border_width(o, v, s) (lv_obj_set_style_border_width)(o, r7::px(v), s)
#define lv_obj_set_style_pad_all(o, v, s) (lv_obj_set_style_pad_all)(o, r7::px(v), s)
#define lv_obj_set_style_pad_hor(o, v, s) (lv_obj_set_style_pad_hor)(o, r7::px(v), s)
#define lv_obj_set_style_pad_ver(o, v, s) (lv_obj_set_style_pad_ver)(o, r7::px(v), s)
#define lv_obj_set_style_pad_left(o, v, s) (lv_obj_set_style_pad_left)(o, r7::px(v), s)
#define lv_obj_set_style_pad_right(o, v, s) (lv_obj_set_style_pad_right)(o, r7::px(v), s)
#define lv_obj_set_style_pad_top(o, v, s) (lv_obj_set_style_pad_top)(o, r7::px(v), s)
#define lv_obj_set_style_pad_bottom(o, v, s) (lv_obj_set_style_pad_bottom)(o, r7::px(v), s)
#define lv_obj_set_style_pad_row(o, v, s) (lv_obj_set_style_pad_row)(o, r7::px(v), s)
#define lv_obj_set_style_pad_column(o, v, s) (lv_obj_set_style_pad_column)(o, r7::px(v), s)
#define lv_obj_set_style_margin_top(o, v, s) (lv_obj_set_style_margin_top)(o, r7::px(v), s)
#define lv_obj_set_style_margin_hor(o, v, s) (lv_obj_set_style_margin_hor)(o, r7::px(v), s)
#define lv_obj_set_style_min_width(o, v, s) (lv_obj_set_style_min_width)(o, r7::px(v), s)
#define lv_obj_set_style_max_width(o, v, s) (lv_obj_set_style_max_width)(o, r7::px(v), s)
#define lv_obj_set_style_text_letter_space(o, v, s) (lv_obj_set_style_text_letter_space)(o, r7::px(v), s)
#endif
#endif
