/*
 * Copyright (C) 2026 Manuel - EA7JRS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Interfaz tipo emisora de la 7B v2 (entorno waveshare_lcd7b_v2, -DUI_RADIO): la de la app Android APRS LoRa+ (Shell.kt,
 * RadioScreen.kt...) adaptada a 1024 x 600 en horizontal. Barra de estado, bandas A (tracker) y B (CAT de la caja del rotor),
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

// medidas de la 7B
constexpr int W = 1024, H = 600;
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
#endif
