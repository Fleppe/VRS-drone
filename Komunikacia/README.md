# Komunikácia - Matúš Ondrejka

Bezdrôtové ovládanie dronu a príjem telemetrie medzi aplikáciou (PC, telefón) a STM32F303K8 cez Seeed Studio XIAO ESP32-S3 Sense (Wi-Fi most + kamera).

## Štruktúra
| Priečinok | Obsah |
|---|---|
| [`stm32/`](stm32/) | komunikačný modul pre STM32 (UART, parser, CRC8, telemetria, failsafe) |
| [`esp32-bridge/`](esp32-bridge/) | firmvér XIAO ESP32-S3 Sense – Wi-Fi AP + WebSocket ↔ UART most + stream kamery |
| [`app/`](app/) | aplikácia Electron (PC) + Capacitor (telefón) |

## Architektúra
```mermaid
flowchart LR
    APP["Aplikácia<br/>(PC / telefón)"] <-->|"Wi-Fi AP, WebSocket"| ESP["XIAO ESP32-S3 Sense<br/>+ kamera OV2640"]
    ESP <-->|"UART 115200 8N1"| STM["STM32F303K8"]
```
- ESP32-S3 vytvára vlastnú Wi-Fi sieť (SoftAP), aplikácia sa pripája cez WebSocket.
- ESP32-S3 je transparentný most – rámce iba preposiela, všetka logika beží na STM32.
- ESP32-S3 navyše posiela obraz z kamery ako MJPEG stream (`http://192.168.4.1:81/stream`).

## Rádiový modul
**Seeed Studio XIAO ESP32-S3 Sense** – [AliExpress (~13 €)](https://www.aliexpress.com/item/1005008416980328.html), pri objednávke vybrať variant **ESP32S3 Sense** (nie samotný ESP32S3 – ten nemá kameru)

| Parameter | Hodnota |
|---|---|
| MCU | ESP32-S3, 2.4 GHz Wi-Fi + BLE 5.0 |
| Pamäť | 8 MB PSRAM, 8 MB flash |
| Kamera | OV2640 (súčasť dosky Sense) |
| UART k STM32 | D6 (GPIO43) TX, D7 (GPIO44) RX |
| Napájanie | 5 V |
| Anténa | externá 2,4 GHz s konektorom U.FL (IPEX1) – originál Seeed ju má v balení; ponuka na AliExpress to neuvádza, overiť u predajcu, inak dokúpiť (~1–3 €). Bez antény dosah len pár metrov |
| Dosah | odhad ~30–50 m priama viditeľnosť, ~10–20 m v budove (overiť meraním) |

- Kamera ide cez tú istú Wi-Fi ako ovládanie, ovládanie má prednosť.
- Rozlíšenie kamery napevno najnižšie 4:3 – `FRAMESIZE_QQVGA` (160 × 120), JPEG. (`96 × 96` je síce menšie, ale štvorcové a na lietanie nepoužiteľné.)
- Nahrádza pôvodne plánovaný ESP32-C3 SuperMini, ktorý nemá rozhranie na kameru ani PSRAM.

## Zapojenie
| STM32F303K8 | XIAO ESP32-S3 Sense | Poznámka |
|---|---|---|
| PA9 – USART1_TX | D7 (GPIO44) – RX | dáta STM → ESP |
| PA10 – USART1_RX | D6 (GPIO43) – TX | dáta ESP → STM |
| GND | GND | spoločná zem povinná |
| – | 5V | napájanie zo step-down 5 V |

- USART2 (PA2 / PA15) ostáva voľný pre ladiaci výpis cez ST-LINK.
- Logické úrovne 3,3 V na oboch stranách.

## Protokol
Binárny rámec:
```
[0xAA][TYPE][LEN][PAYLOAD (LEN B)][CRC8]
```
| TYPE | Smer | Payload | Frekvencia |
|---|---|---|---|
| `0x01` ovládanie | App → STM | throttle, roll, pitch, yaw (`int16` každý), flags (`uint8`, bit0 = arm) | 50 Hz |
| `0x10` telemetria | STM → App | roll, pitch, yaw (°×100, `int16`), výška (mm, `uint16`), napätie batérie (mV, `uint16`), stav (`uint8`) | 20–50 Hz |
| `0x20` príkaz | App → STM | ID príkazu + parametre (arm/disarm, kalibrácia gyra, PID) | na požiadanie |
| `0x21` ACK | STM → App | ID príkazu + výsledok | odpoveď na `0x20` |

- Little-endian, CRC-8 (polynóm `0x07`, init `0x00`) počítané cez `TYPE`, `LEN` a `PAYLOAD`.
- Osi ovládania v rozsahu `-1000..1000`: throttle `+` = stúpanie, pitch `+` = dopredu, roll `+` = doprava, yaw `+` = doprava; `0` = drž (výšku / polohu).
- Arm sa vykoná iba pri nábežnej hrane bitu arm (0 → 1) a nulovom plyne.
- Rámec s chybným CRC alebo neznámym `TYPE` sa zahodí.
- Ovládanie sa posiela ako **stav** v pravidelnom intervale, nie ako udalosti stlačenia klávesu.
- ESP32-S3 rámce nemení – čo príde cez WebSocket, ide 1:1 na UART a naopak.

## Príkazy – čo sa posiela a čo sa prijíma

### Odosielané (App → ESP32 → STM32)
**`0x01` ovládanie** – 13 B, posiela sa stále 50 Hz.
| Bajt | Pole | Typ | Popis |
|---|---|---|---|
| 0 | štart | `uint8` | `0xAA` |
| 1 | TYPE | `uint8` | `0x01` |
| 2 | LEN | `uint8` | `9` |
| 3–4 | throttle | `int16` | `-1000..1000`, `+` stúpanie, `0` drž výšku |
| 5–6 | roll | `int16` | `-1000..1000`, `+` doprava |
| 7–8 | pitch | `int16` | `-1000..1000`, `+` dopredu |
| 9–10 | yaw | `int16` | `-1000..1000`, `+` doprava |
| 11 | flags | `uint8` | bit0 = arm |
| 12 | CRC8 | `uint8` | cez bajty 1–11 |

Príklady:
```
aa 01 09 00 00 00 00 00 00 00 00 01 39   pokoj, arm=1
aa 01 09 f4 01 00 00 d4 fe 00 00 01 4a   throttle=500, pitch=-300, arm=1
```

**`0x20` príkaz** – na požiadanie (zatiaľ nie je v UI): ID príkazu + parametre (kalibrácia gyra, PID).

### Prijímané (STM32 → ESP32 → App)
**`0x10` telemetria** – 15 B, 20–50 Hz.
| Bajt | Pole | Typ | Popis |
|---|---|---|---|
| 0 | štart | `uint8` | `0xAA` |
| 1 | TYPE | `uint8` | `0x10` |
| 2 | LEN | `uint8` | `11` |
| 3–4 | roll | `int16` | ° × 100 |
| 5–6 | pitch | `int16` | ° × 100 |
| 7–8 | yaw | `int16` | ° × 100 |
| 9–10 | výška | `uint16` | mm |
| 11–12 | batéria | `uint16` | mV |
| 13 | stav | `uint8` | `0` DISARMED, `1` ARMED, `2` FLYING, `3` LANDING |
| 14 | CRC8 | `uint8` | cez bajty 1–13 |

Príklad:
```
aa 10 0b 96 00 b0 ff 28 23 d2 04 18 2e 02 7e   roll=1.5° pitch=-0.8° yaw=90° výška=1234 mm bat=11800 mV FLYING
```

**`0x21` ACK** – odpoveď na `0x20` (zatiaľ nie je v UI): ID príkazu + výsledok.

Na PC UI zobrazuje odoslané a prijaté rámce (hex + dekódované hodnoty) v dvoch konzolách RX / TX.

## Failsafe
- Ak STM32 nedostane platný ovládací rámec **250 ms**, prechádza do stavu `LANDING`: náklon sa vynuluje a plyn pomaly klesá.
- Po dosadnutí sa dron odzbrojí (`DISARMED`).
- Ak sa spojenie obnoví počas klesania, ovládanie preberá opäť aplikácia.
- Arm je povolený len pri nulovom plyne.

## Ovládanie
**PC / notebook:**
| Kláves | Akcia |
|---|---|
| `W` / `S` alebo `↑` / `↓` | pitch – dopredu / dozadu |
| `A` / `D` alebo `←` / `→` | roll – doľava / doprava |
| `Q` / `E` | yaw – otočenie doľava / doprava |
| `Space` | stúpanie |
| `Shift` | klesanie |

- `Shift` namiesto `Ctrl`, lebo `Ctrl+W` v prehliadači zavrie kartu a nedá sa zablokovať.

**Steam Deck (1280 × 800) a ovládač k PC (Xbox, PlayStation, …):** reálne páky cez Gamepad API (klávesy fungujú tiež).
| Ovládač | Akcia |
|---|---|
| `R2` / `L2` | stúpanie / klesanie (podľa sily stlačenia) |
| ľavá páka doľava / doprava | otočenie (hore / dole nerobí nič) |
| pravá páka | dopredu / dozadu, doľava / doprava |
| `A` | zapnúť / vypnúť motory (prepínač) – zapnutie hneď a iba pri nulovom plyne, vypnutie až po podržaní 1 s, aby ho náhodný stisk za letu nespustil |
| `B` | vypnúť motory hneď |

- Ovládač k PC: USB alebo Bluetooth, prehliadač ho zaregistruje až po prvom stlačení tlačidla (v lište „Ovládač: áno“).
- V Game Mode treba pre prehliadač v Steam Input zvoliť rozloženie ako gamepad, inak páky ovládajú myš.

**Telefón:** dva dotykové joysticky – ľavý: stúpanie / klesanie + yaw, pravý: pitch / roll.

## Webové UI na ESP32-S3
[`esp32-bridge/data/index.html`](esp32-bridge/data/index.html) – jeden súbor (HTML + CSS + JS, bez knižníc a internetu), ESP32-S3 ho servuje na `http://192.168.4.1/`, WebSocket na `ws://192.168.4.1/ws`.

Biely vzhľad v štýle MATLAB / tkinter (rámčeky s nadpisom, klasické sivé tlačidlá).

| Zariadenie | Detekcia (rozlíšenie obrazovky) | Rozloženie |
|---|---|---|
| PC | všetko ostatné | hore kamera v strede, vľavo náklon (roll / pitch), vpravo smer (yaw) a výška; pod tým konzoly – vľavo prijaté (RX), vpravo odoslané (TX); dole ovládanie (tlačidlo zapnúť / vypnúť motory, odosielané hodnoty, klávesy) |
| Steam Deck | presne 1280 × 800 | kamera cez celý stred, vľavo náklon a odosielané hodnoty, vpravo smer a ovládanie; bez joystickov na obrazovke |
| Telefón | kratšia strana < 768 px | joysticky po stranách, nad nimi aktuálne hodnoty, v strede hore náklon a smer, pod nimi čo najväčšia kamera; na výšku výzva otočiť telefón |

- Rozloženie sa dá vynútiť cez výber v hornej lište alebo `?mode=pc|deck|phone` v adrese.
- Kamera: MJPEG stream z `http://192.168.4.1:81/stream`, pri výpadku čierna plocha s prečiarknutou kamerou a nový pokus každých 5 s.
- **Export do Excelu** (tlačidlo v hornej lište, iba pri vypnutých motoroch): súbor `.xlsx` s dvoma hárkami – *Odoslané* (čas, plyn, otočenie, dopredu, do strany, motory, spojenie, rámec v hex) a *Prijaté* (čas, do strany, dopredu, otočenie, výška, batéria, stav, rámec v hex). Zaznamenáva sa od otvorenia stránky, max 100 000 riadkov na hárok.
- Celé UI je po slovensky; stavy dronu: `VYPNUTÝ`, `PRIPRAVENÝ`, `LETÍ`, `PRISTÁVA` (hodnoty 0–3 v telemetrii).
- Konzoly zapisujú pravidelné rámce max 10× za sekundu, udalosti a chyby (spojenie, zmena stavu, chybné CRC, neznámy rámec) vždy; „Pozastaviť výpis“ zastaví posúvanie.
- Odosielanie ovládania 50 Hz, plynulý nábeh hodnôt (0,4 s na plnú hodnotu).
- Pri strate WebSocket spojenia sa automaticky znova pripája a bit arm sa vynuluje.

## Návrh UI (prvotný mockup)
> Toto je **prvotný mockup** – základ toho, čo chceme robiť. Rozloženie, ovládanie aj vzhľad sa budú ešte meniť.
> Na obrázkoch sú ukážkové dáta, kamera zatiaľ nie je pripojená.

**PC (1440 × 900)** – kamera v strede, okolo náklon a smer, pod tým konzoly prijatých a odoslaných dát, dole ovládanie.

![UI na PC](img/ui-pc.png)

**Steam Deck (1280 × 800)** – čo najväčšia kamera, pod ňou konzoly, po stranách dáta; ovládanie fyzickými pákami.

![UI na Steam Decku](img/ui-deck.png)

**Telefón (844 × 390, na šírku)** – kamera cez celú výšku, po stranách dotykové joysticky (dá sa ovládať dvoma prstami naraz).

![UI na telefóne](img/ui-phone.png)

## Úlohy
- [ ] STM32: UART DMA príjem + parser rámcov + CRC8
- [ ] STM32: odosielanie telemetrie
- [ ] STM32: failsafe pri strate spojenia
- [ ] ESP32-S3: SoftAP + WebSocket ↔ UART most
- [ ] ESP32-S3: MJPEG stream z kamery + zobrazenie v UI
- [ ] Meranie latencie ovládania so zapnutou kamerou a bez nej
- [ ] Aplikácia: spoločná knižnica protokolu (TypeScript)
- [ ] Aplikácia: Electron – ovládanie klávesnicou
- [ ] Aplikácia: Capacitor – joystick UI
- [ ] Meranie latencie a stratovosti
