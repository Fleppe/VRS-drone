# VRS-drone

Práca so senzorom DFRobot BMI160
Potrebné úlohy:
- Zabezpečiť I2C komunikáciu medzi senzorom a STM32
- Čítanie 6 osí a prepočet hodnôt na stupne
- Kalibrácia gyroskopu
- Odhad náklonu
- Posielanie dát do telemetrie

---

## Komunikácia PC / telefón ↔ STM32 (Ondrejka)

Bezdrôtové ovládanie dronu a príjem telemetrie. Všetka logika beží na STM32; rádiový modul je len transparentný UART most bez vlastného firmvéru.

### Hardvér
| Modul | Typ | Cena | Electron (PC) | Capacitor (telefón) | Poznámka |
|---|---|---|---|---|---|
| **HM-10 / JDY-23** (odporúčaný) | BLE 4.x, UART | ~3 € | Web Bluetooth | Android + iOS (`@capacitor-community/bluetooth-le`) | max 20 B na zápis, latencia ~15–50 ms |
| HC-05 / HC-06 | Bluetooth Classic (SPP), UART | ~4 € | Web Serial (virtuálny COM port) | iba Android, cez plugin | iOS nepodporuje SPP |

- Zapojenie: modul TX → STM32 RX, modul RX → STM32 TX, spoločná zem, 3,3 V logika.
- STM32: UART + DMA príjem s IDLE-line prerušením, kruhový buffer, parser rámcov.

### Protokol (binárny)
```
[0xAA][TYPE][LEN][PAYLOAD (LEN B)][CRC8]
```
| TYPE | Smer | Payload | Frekvencia |
|---|---|---|---|
| `0x01` ovládanie | PC → STM | throttle, roll, pitch, yaw (`int16` každý), flags (`uint8`, bit0 = arm) | 50 Hz |
| `0x10` telemetria | STM → PC | roll, pitch, yaw (°×100, `int16`), napätie batérie (mV, `uint16`), stav (`uint8`) | 20–50 Hz |
| `0x20` príkaz | PC → STM | ID príkazu + parametre (arm/disarm, kalibrácia gyra, PID) | na požiadanie |
| `0x21` ACK | STM → PC | ID príkazu + výsledok | odpoveď na `0x20` |

- Little-endian, CRC8 počítané cez `TYPE`, `LEN` a `PAYLOAD`.
- Rámec s chybným CRC alebo neznámym `TYPE` sa zahodí.
- Ovládanie sa posiela ako **stav** (aktuálne hodnoty v pravidelnom intervale), nie ako udalosti stlačenia klávesu.

### Bezpečnosť (failsafe)
- Ak STM32 nedostane platný ovládací rámec **250 ms**, motory idú na nulu / riadené klesanie a dron sa odzbrojí (disarm).
- Arm je povolený len pri throttle = 0.

### Aplikácia
- **Electron (notebook):** WASD = pitch / roll, šípky ↑↓ = throttle, ←→ = yaw. Hodnoty plynulo nabiehajú (rampa), nie skokom.
- **Capacitor (telefón):** dva virtuálne joysticky – ľavý throttle / yaw, pravý pitch / roll.
- Spoločný kód pre protokol a ovládací stav, rozdielna len vrstva vstupu a transportu.
- Zobrazenie telemetrie: náklon, napätie, stav spojenia (strata paketov, latencia).

### Úlohy
- [ ] Vybrať a objednať modul (HM-10 / JDY-23)
- [ ] STM32: UART DMA príjem + parser rámcov + CRC8
- [ ] STM32: odosielanie telemetrie (napojenie na dáta z BMI160)
- [ ] STM32: failsafe pri strate spojenia
- [ ] Zdieľaná knižnica protokolu (TypeScript)
- [ ] Electron aplikácia – BLE pripojenie, ovládanie klávesnicou
- [ ] Capacitor aplikácia – BLE pripojenie, joystick UI
- [ ] Meranie latencie a stratovosti
