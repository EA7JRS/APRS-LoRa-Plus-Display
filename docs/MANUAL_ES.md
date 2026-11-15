# APRS LoRa+ Display — manual de usuario

Copyright (C) 2026 Manuel - EA7JRS. GPL-3.0-or-later. Versión **5.4.6**.

APRS LoRa+ Display es una pantalla táctil para el tracker **APRS LoRa+**. No lleva radio LoRa ni GPS: se une al tracker por
Bluetooth LE, enseña lo que oye y lo maneja entero, como la app Android. Con WiFi añade la red del barco, el tiempo de
internet, los avisos oficiales, el DX Cluster, el mapa y la caja del rotor.

Guía rápida: [QUICKSTART_ES.md](QUICKSTART_ES.md). Pines y lo que falta por comprobar de cada placa: [HARDWARE.md](HARDWARE.md).
Reloj OpenHamClock: [HAMCLOCK.md](HAMCLOCK.md).

## 1. Placas y firmware

| Firmware (entorno) | Placa Waveshare | Interfaz |
|---|---|---|
| `waveshare_lcd7b` | ESP32-S3-Touch-LCD-7B (1024×600) | Pestañas |
| `waveshare_lcd7b_v2` | ESP32-S3-Touch-LCD-7B | Tipo emisora (la de la app) |
| `waveshare_lcd7b_hamclock` | ESP32-S3-Touch-LCD-7B | Reloj OpenHamClock ([HAMCLOCK.md](HAMCLOCK.md)) |
| `waveshare_lcd7` | ESP32-S3-Touch-LCD-7, sin B, rev 1.2 (800×480) | Pestañas |
| `waveshare_lcd7_v2` | ESP32-S3-Touch-LCD-7, sin B | Tipo emisora, a escala |
| `waveshare_lcd35` | ESP32-S3-Touch-LCD-3.5 y 3.5-C (480×320) | Pestañas |
| `waveshare_lcd28b` | ESP32-S3-Touch-LCD-2.8B (480×640) | Pestañas |
| `waveshare_lcd169` | ESP32-S3-Touch-LCD-1.69 (240×280) | Pestañas (iconos) |

**La 7 y la 7B no son la misma placa**: la 7 (800×480, expansor CH422G) no arranca con el firmware de la 7B (la pantalla se
queda negra) y al revés. Mira la serigrafía: la 7B pone «7B».

Estado: la LCD-7 con pestañas está probada en la placa (pantalla, imagen y táctil). El resto compila, pasa sus pruebas en el
PC y está **sin probar en la placa**.

## 2. Instalar

1. Abre **https://ea7jrs.github.io/APRS-LoRa-Plus/install/#display** en Chrome o Edge (ordenador, o Android con cable OTG).
2. Conecta la placa por USB, pulsa *Detectar placa* o elígela a mano, y elige el firmware de la tabla anterior.
3. *Instalar*. La configuración guardada en la placa se conserva.

En la 7B y la 7 el puerto serie es el USB-C de la UART (CH343): usa ese conector. Por USB la placa contesta a `APRS-INFO?`
con `device=display`, que es como la reconoce el instalador.

También se puede compilar con PlatformIO: `pio run -e <entorno> -t upload`.

## 3. Unirla al tracker

1. En el tracker (WebUI > Bluetooth): Bluetooth encendido y, mejor, un *Control PIN* propio (de serie `123456`).
2. En la pantalla: *Settings* (pestañas) o *Settings > TRACKER LINK* (v2): el mismo PIN. La dirección puede quedar vacía:
   se une al primer tracker que anuncie el servicio KISS, TNC2 o de control. Con varios trackers cerca, escribe su
   dirección BLE.
3. La primera vez se empareja con el PIN; después vuelve a unirse sola al arrancar o si se corta.
4. Si cambias el PIN del tracker: *Forget pairing* y volver a unir.

El tracker admite un solo equipo de control a la vez por BLE: si la app del móvil está unida, la pantalla espera.

## 4. Interfaz con pestañas (7B, LCD-7, 3.5, 2.8B, 1.69)

En las pantallas grandes las pestañas llevan texto a la izquierda; en las pequeñas son iconos y las tablas tienen menos columnas.

| Pestaña | Qué hace |
|---|---|
| **Status** | Posición GNSS, modo de radio, perfil, batería. Baliza ahora, digipeater y cambio de modo (el tracker se reinicia) |
| **Heard** | Estaciones oídas por el tracker: distancia desde su GNSS, RSSI, SNR, antigüedad y comentario |
| **Messages** | Mensajes APRS del tracker y envío con el teclado táctil |
| **Sensors** | Batería, temperatura, humedad, presión, QNH, altitud barométrica, rumbo; viento con Beaufort; tarjetas *Official alerts* y *Space weather* |
| **SAT / Rotor** | Satélite en curso (azimut, elevación, distancia) y objetivo del rotor del tracker |
| **Mirror** | Espejo de la pantalla del tracker |
| **DAPNET** | Avisos recibidos por el tracker en modo DAPNET |
| **Network** | Red local del barco, internet y avisos (apartado 6) |
| **Settings** | PIN y dirección del tracker, olvidar emparejamiento, luz de fondo, paleta (*Theme*: la de la app o la del rotor, se aplica al reiniciar) y *Link > Clock* (hora UTC y de dónde sale) |

## 5. Interfaz tipo emisora (7B v2 y LCD-7 v2)

La misma forma de manejo que la app Android: pantalla de radio de dos bandas, teclas táctiles abajo y la tecla **[F]**.

- **Pantalla RADIO**: banda A = el tracker (modo, frecuencia, perfil, S-meter de la última estación oída, LED TX / RX / MSG /
  EMG); banda B = el CAT de la caja del rotor. Debajo, BEACON, DIGI, ROTOR y las últimas estaciones oídas.
- **[F]**: tres páginas de funciones. *APRS*: S.LIST, MSG, BEACON, BCN (baliza ahora), GPS, WX, MAP, DISP, DAPNET, DIGI,
  WARNINGS. *RADIO*: DX, SAT, ROTOR, AIS, CW RX, MODE. *SYSTEM*: REMOTE, CONFIG, FIRMWARE, SENSORS, SETTINGS, TRACKER,
  REBOOT, ABOUT. Cada tecla enseña su estado debajo (estaciones oídas, satélites, barcos…).
- **Teclas táctiles**: las cinco de abajo se eligen manteniendo pulsada una tecla del menú [F] (*Settings > DEFAULT TOUCH
  KEYS* las devuelve a las de serie).
- **Menú lateral** (botón ☰ de la cabecera): todas las pantallas.

| Pantalla | Qué hace |
|---|---|
| **Stations** / **Station** | Lista de estaciones oídas y ficha con brújula (norte arriba o rumbo arriba), distancia, rumbo, señal, tiempo APRS y ROTOR |
| **Messages** / **Conversation** | Mensajes por conversación, escribir y responder |
| **Map** | Mapa con estaciones, barcos AIS, tu posición y el haz del rotor (apartado 7) |
| **Weather** | Estaciones meteorológicas APRS con viento, Beaufort, rocío, sensación y tendencia de la presión |
| **Weather warnings** | Avisos oficiales (apartado 8) |
| **Display** | Brújula, cielo GPS, altitud o reloj (sol y luna) a pantalla completa |
| **DX cluster** | Spots de un nodo telnet (de serie EA4URE), con filtros de origen, banda y modo, y TUNE al CAT de la caja |
| **Satellites** | Paso en curso, lista y elección de satélite, con la esfera del rotor |
| **Rotor box** | Caja APRS LoRa+ Rotor por WiFi (TCP 4534) o BLE: esfera, ir a, mover, parar, aparcar, seguir al tracker o a una estación, CAT con Doppler |
| **AIS** | Barcos que manda el tracker (modo AIS de la T-TWR) con CPA |
| **Tracker**, **Sensors**, **Remote**, **Config**, **Firmware** | Lo mismo que en la app: modo, perfiles, sensores, todas las opciones del menú del tracker, su configuración entera y la actualización de su firmware por BLE |
| **DAPNET** | Avisos DAPNET |
| **Settings** | Tracker, WiFi, DX Cluster, OpenWeatherMap, pantalla, mapa, unidades (métrico, náutico, imperial) |

**Esfera del rotor** (ROTOR y SAT, igual que en la app, la caja del rotor y sat-aprs): aguja azul = posición, ámbar =
objetivo, verde = rumbo del tracker cuando va a más de 3 km/h, rojo = topes mecánicos.

**Firmware del tracker** (pantalla Firmware): con WiFi, la pantalla baja la versión publicada de la placa del tracker,
comprueba su SHA-256 y la envía por Bluetooth; el tracker la vuelve a comprobar antes de activarla.

## 6. Red local e internet (pestaña Network de las placas con pestañas)

La misma conectividad que el plotter: WiFi cliente, **un servidor TCP** y escucha **UDP**, en una tarea aparte (si la red se
cae, la pantalla sigue).

| Servidor | Puerto | Qué llega |
|---|---|---|
| NMEA 0183 | 10110 | Sentencias NMEA y AIS de cualquier equipo de la red |
| gpsd | 2947 | NMEA del receptor de gpsd |
| Signal K | 8375 | Datos del barco propio; avisos en estado *alarm* / *emergency* |
| Dirección vacía | 10110 | El móvil que da la WiFi con la app APRS LoRa+ y su servidor NMEA encendido |

- Por **UDP** entra el NMEA difundido en la red, como el del instrumento de viento de 4″.
- **OpenWeatherMap** (clave gratuita): viento, presión y temperatura de internet en la posición, en ámbar (estimado).
- Cada dato sale de la mejor fuente al día: red (LAN), tracker (TRK, posición, rumbo y velocidad de su GNSS) e internet
  (WEB). A los 10 s sin la mejor entra la siguiente.
- Blancos AIS de la red con distancia, demora y CPA; en rojo con CPA ≤ 0,5 mn y TCPA ≤ 20 min.
- **Cálculos** (los mismos en todo APRS LoRa+): Beaufort, punto de rocío, sensación térmica, sol y luna del día, y rumbo
  verdadero con la declinación del WMM2025 si solo llega HDG / HDM.
- **Hora**: la del GNSS del tracker o del GPS de la red; sin GPS, NTP (`ntp.roa.es`, con `hora.roa.es` y `pool.ntp.org`). La
  del GPS manda durante 6 h.

En la v2 la WiFi se configura en *Settings > WIFI* y la usan el DX Cluster, el tiempo de internet (que también se manda al
tracker y, con viento, a la caja del rotor), el mapa, los avisos y el firmware.

## 7. Mapa (v2)

Capas: OpenStreetMap, IGN topográfico, ortofoto PNOA, oscuro, Esri (con tu clave de ArcGIS) y radar; OpenSeaMap encima
como opción. Necesita WiFi. Con una microSD las teselas vistas se guardan en `/maps` y funcionan sin conexión.
Datos: © OpenStreetMap contributors; IGN España (CC BY 4.0); OpenSeaMap; Esri.

## 8. Avisos oficiales

- **Placas con pestañas** (con WiFi): AEMET en España, MeteoAlarm en el resto de Europa (país en *Country (ISO)*), NWS en
  EE. UU., tsunamis, terremotos (USGS, EMSC) y tiempo espacial (SWPC), cada 15 min. Los de AEMET, con posición (del tracker o
  de la red), son los que tienen dentro esa posición según el polígono de cada aviso; sin posición, por las palabras de
  *Alert zones* («Huelva, Cádiz»). Ventana emergente del color del nivel y resumen en Sensors.
- **v2**: pantalla *Weather warnings* (tecla WARN): AEMET (con tu clave gratuita de OpenData, con polígono y distancia) o
  Meteoalarm (país y zona); *POP-UP WINDOW FROM* elige desde qué nivel salta la ventana (amarillo, naranja, rojo o nunca).
- **En todas**: los avisos que manda el tracker (los baja la app: AEMET, MeteoAlarm, NWS, terremotos, tsunamis, SAME), con
  el tiempo espacial.

## 9. Alertas de emergencia

Ventana centrada sobre cualquier pantalla, con el diseño del plotter y de la app: baliza o mensaje APRS de emergencia,
peligro AIS (DISTRESS, riesgo de abordaje) y la alarma del tracker (EAI, hombre al agua). SILENCE ALARM, MAP, STATION y
CLOSE. La misma estación no vuelve a abrirla en 10 min. Las placas no tienen zumbador.

## 10. Problemas

| Qué pasa | Qué mirar |
|---|---|
| Pantalla negra | ¿Firmware de la placa correcta? (7 frente a 7B). Consola serie a 115200: el arranque dice qué falla |
| No se une al tracker | Bluetooth del tracker encendido, el mismo PIN, la app del móvil desconectada; *Forget pairing* tras cambiar el PIN |
| Sin datos de red | WiFi y servidor en Network; el estado sale arriba de la pestaña |
| Avisos o mapa vacíos | WiFi con salida a internet y hora puesta (Settings > Link > Clock) |
