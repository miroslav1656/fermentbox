#pragma once
#include <Arduino.h>
#include "control.h"

// ⚠️ VAROVÁNÍ: GPIO piny ESP32 jsou 3.3V tolerantní!
// NIKDY nepřipojujte 12V přímo na GPIO - zničí to desku!
//
// Správné zapojení Noctua PWM ventilátoru:
// - Černá (GND) → společná zem (ESP32 + 12V zdroj)
// - Žlutá (+12V) → pouze na 12V zdroj (NE na ESP32!)
// - Zelená (TACHO) → nepřipojovat nebo přes dělič napětí
// - Modrá (PWM) → GPIO 19 + pull-up 10kΩ na 5V (nebo 3.3V)
//
// MOSFET zapojení:
// - Gate → GPIO pin (3.3V logika)
// - Source → GND (společná zem)
// - Drain → záporný pól zařízení (topení/zvlhčovač)
// - Kladný pól 12V → přímo na zařízení

// Bezpečné GPIO piny pro ESP32 DevKit
constexpr int PIN_HEATER_RELAY     = 23;  // MOSFET pro PTC topení
constexpr int PIN_HUMIDIFIER_RELAY = 22;  // MOSFET / relé pro zvlhčovač
constexpr int PIN_COOL_FAN_RELAY   = 21;  // MOSFET / relé pro chladicí ventilátor
constexpr int PIN_FAN_PWM          = 19;  // PWM výstup na vnitřní ventilátor

// PWM nastavení pro vnitřní ventilátor
// Na ESP32 použijeme analogWriteFrequency() pro nastavení na 25 kHz
constexpr int FAN_PWM_FREQ_HZ = 25000;    // 25 kHz – mimo slyšitelné pásmo

void initIO();
void applyActuators();
