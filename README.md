# FermentorBox

Inteligentní fermentační komora pro pečivo s automatickou regulací teploty a vlhkosti.

## Popis projektu

FermentorBox je Arduino projekt pro ESP32, který řídí fermentační komoru určenou pro kynutí různých druhů pečiva (panettone, croissanty, chleba, kváskové těsto). Systém automaticky udržuje nastavenou teplotu a vlhkost pomocí topení, chlazení a zvlhčovače.

### Hlavní funkce

- **Automatická regulace teploty** (20-35°C) pomocí PTC topení a chladicího ventilátoru
- **Regulace vlhkosti** (50-90%) pomocí ultrazvukového zvlhčovače
- **Přednastavené programy** pro různé druhy pečiva
- **Webové rozhraní** pro ovládání a monitoring
- **Hardware watchdog** pro bezpečnost při závadě
- **Anti-cycling ochrana** zabraňující častému sepínání topení a zvlhčovače
- **Fail-safe mechanismy** při selhání senzorů

## Seznam komponent

### Řídící jednotka
- **ESP32 DevKit** (30 pinů) - hlavní řídící jednotka
- **2× DHT22** - teplotní a vlhkostní senzory (záloha při selhání jednoho)
- **OLED displej SH1106** (128×64, I2C) - zobrazení dat
- **Potenciometr 10kΩ** - ruční nastavení parametrů
- **6× tlačítka** - hardwarové ovládání

### Výkonové komponenty
- **2× MOSFET LR7843** (nebo ekvivalent, logic-level N-channel)
  - Jeden pro topení
  - Jeden pro zvlhčovač
- **PTC topení 12V/30W** - ohřev komory
- **Noctua NF-R8 redux PWM** - vnitřní cirkulační ventilátor
- **12V ventilátor** - chladicí ventilátor (nasává studený vzduch z garáže)
- **Ultrazvukový zvlhčovač 12V** - zvyšování vlhkosti

### Napájení
- **12V DC adaptér** (min. 3A doporučeno)
- **Step-down měnič 12V→5V** pro napájení ESP32

## ⚠️ KRITICKÉ: Správné zapojení Noctua PWM ventilátoru

**VAROVÁNÍ:** Nesprávné zapojení PWM ventilátoru může **zničit ESP32 desku!**

Noctua PWM ventilátor má 4 vodiče:
- **Černá (GND)** → Společná zem (ESP32 + 12V zdroj)
- **Žlutá (+12V)** → **POUZE na 12V zdroj! NIKDY ne na ESP32!**
- **Zelená (TACHO)** → Nepoužívá se (nebo přes rezistorový dělič napětí 12V→3.3V)
- **Modrá (PWM)** → GPIO 19 + pull-up rezistor 10kΩ na 5V (nebo 3.3V)

### Proč je to důležité?

- GPIO piny ESP32 jsou **pouze 3.3V tolerantní**
- Připojení 12V přímo na GPIO pin **okamžitě zničí ESP32**
- Žlutý (+12V) vodič ventilátoru slouží pouze k napájení motoru
- PWM signál (modrý vodič) ovládá rychlost otáček při konstantním napájení 12V

## GPIO piny

### Senzory a displej
| Pin | Funkce | Poznámka |
|-----|--------|----------|
| 32 | I2C SDA | OLED displej SH1106 |
| 33 | I2C SCL | OLED displej SH1106 |
| 27 | DHT22 #1 Data | Teplotní/vlhkostní senzor |
| 26 | DHT22 #2 Data | Teplotní/vlhkostní senzor (záloha) |

### Výkonové výstupy
| Pin | Funkce | Poznámka |
|-----|--------|----------|
| 23 | Topení (MOSFET) | PTC 12V/30W |
| 22 | Zvlhčovač (MOSFET) | Ultrazvukový 12V |
| 21 | Chladicí ventilátor | 12V |
| 19 | PWM ventilátor | Noctua PWM (viz varování výše!) |

### Hardwarové ovládání
| Pin | Funkce | Poznámka |
|-----|--------|----------|
| 34 | Potenciometr | Nastavení T/RH/FAN |
| 4  | Tlačítko MODE | AUTO ↔ MANUAL |
| 5  | Tlačítko PARAM | Přepínání T/RH/FAN |
| 25 | Tlačítko HEATER | Zapnout/vypnout topení (MANUAL) |
| 13 | Tlačítko HUMID | Zapnout/vypnout zvlhčovač (MANUAL) |
| 14 | Tlačítko COOL | Zapnout/vypnout chlazení (MANUAL) |
| 15 | Tlačítko STOP | PANIC STOP - vypnout vše |

**Poznámka:** GPIO 12 je strapping pin a může způsobit problémy při bootu ESP32, proto byl nahrazen GPIO 25 pro tlačítko topení.

## Schéma zapojení

```
ESP32 DevKit                    12V Power Supply
    │                                  │
    ├─[GPIO 23]─┬─[MOSFET Gate]───────┤
    │           └─[10kΩ R]─[GND]      │
    │                                  ├─[+]──[PTC Heater 30W]──[MOSFET Drain]
    │                                  │                          │
    ├─[GPIO 22]─┬─[MOSFET Gate]───────┤                         [GND]
    │           └─[10kΩ R]─[GND]      │
    │                                  ├─[+]──[Humidifier]──[MOSFET Drain]
    │                                  │                      │
    ├─[GPIO 21]─────[Relé/MOSFET]─────┼─[+]──[Cool Fan]─────[GND]
    │                                  │
    ├─[GPIO 19]─[10kΩ↑]─[PWM]─────────┤  Noctua PWM Fan:
    │            │                     │   - Yellow (+12V) → 12V supply
    │           3.3V                   │   - Blue (PWM) → GPIO 19 + 10kΩ pull-up
    │                                  │   - Black (GND) → Common GND
    ├─[GND]─────────────────────────[─┴─] - Green (TACHO) → Not connected
    │
    ├─[5V]←─[Step-down 12V→5V]───────[12V]
    │
    ├─[GPIO 27]───[DHT22 #1 Data]
    ├─[GPIO 26]───[DHT22 #2 Data]
    │
    ├─[GPIO 32/33]─[I2C: SDA/SCL]─[OLED SH1106]
    │
    └─[GPIO 34/4/5/25/13/14/15]─[Buttons/Pot]

CRITICAL: 
  - Všechny GND musí být společné (ESP32, 12V zdroj, senzory)
  - NIKDY nepřipojujte 12V přímo na GPIO!
  - MOSFET Gate piny: 10kΩ pull-down rezistor k GND
  - PWM ventilátor: 10kΩ pull-up na PWM signálu
```

## Bezpečnostní opatření

### Hardware
1. **Watchdog timer (10s)** - automatický restart při zamrznutí ESP32
2. **Anti-cycling ochrana** - minimální doba mezi spínáním (30s topení, 15s zvlhčovač)
3. **Fail-safe při selhání senzorů** - vypnutí topení, zapnutí chlazení
4. **Maximální limity** - teplota 50°C, vlhkost 98%
5. **Izolované MOSFET spínání** - logická úroveň 3.3V, výkon 12V

### Software
- Kontinuální monitoring teplotních a vlhkostních senzorů
- NaN kontrola při výpadku senzoru s fallback na fungující čidlo
- PANIC STOP tlačítko pro okamžité vypnutí všech výstupů
- Automatické vypnutí při překročení bezpečnostních limitů

## Kompilace a nahrání

### PlatformIO (doporučeno)
```bash
pio run -t upload
```

### Arduino IDE
1. Nainstalujte podporu pro ESP32: https://github.com/espressif/arduino-esp32
2. Vyberte desku: "ESP32 Dev Module"
3. Nastavte partition scheme: "Default 4MB with spiffs"
4. Nahrajte sketch

### Požadované knihovny
- ESP32 core
- DHT sensor library
- Adafruit GFX Library
- U8g2 (pro OLED displej)
- AsyncTCP
- ESPAsyncWebServer

## Použití

### AUTO režim
- Systém automaticky reguluje teplotu a vlhkost podle zvoleného programu
- Vyberte program příkazem `0-4` v Serial Monitoru nebo přes web

### MANUAL režim
- Přepnout tlačítkem MODE nebo příkazem `t`
- Ruční ovládání topení (`h`), zvlhčovače (`u`), chlazení (`c`)
- Stále platí bezpečnostní limity!

### Serial příkazy
- `0-4` - Změna programu
- `t` - Toggle AUTO/MANUAL režim
- `p` - Vypsat aktuální program
- `h/u/c/f` - Ovládání aktuátorů (v MANUAL)
- `s` - Stav aktuátorů
- `a` - Vypnout vše
- `w` - Uložit konfiguraci
- `r` - Načíst konfiguraci

## Webové rozhraní

Po připojení k WiFi je dostupné webové rozhraní na `http://<IP_adresa_ESP32>/`

Funkce:
- Zobrazení aktuální teploty a vlhkosti z obou senzorů
- Nastavení cílové teploty a vlhkosti
- Výběr programu fermentace
- Zobrazení stavu aktuátorů
- Přepínání AUTO/MANUAL režimu

## Přednastavené programy

1. **Panettone Bulk** - 28°C, 80% RH
2. **Panettone Proof** - 28°C, 80% RH
3. **Croissant Bulk** - 24°C, 75% RH
4. **Croissant Proof** - 26°C, 80% RH
5. **Chleba Bulk** - 24°C, 75% RH
6. **Chleba Cold** - 5°C (chlazení), 75% RH
7. **Chleba Proof** - 32°C, 85% RH
8. **Kváskový starter Active** - 28°C, 75% RH
9. **Kváskový starter Sleep** - 4°C (lednice), 70% RH
10. **Custom** - Vlastní nastavení

## Licence

Tento projekt je open-source. Používejte na vlastní riziko.

## Autor

FermentorBox - Fermentační komora pro pečivo

## Změny v této verzi

- ✅ Přidán hardware watchdog timer (10s timeout)
- ✅ Opravena NaN kontrola při výpadku senzoru
- ✅ Sjednoceny duplicitní bezpečnostní konstanty
- ✅ Změněn GPIO 12 na GPIO 25 (strapping pin)
- ✅ Přidána anti-cycling ochrana (30s topení, 15s zvlhčovač)
- ✅ Doplněna dokumentace zapojení s kritickým upozorněním na PWM ventilátor
