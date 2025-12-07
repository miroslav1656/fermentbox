#pragma once
#include <Arduino.h>
#include "control.h"

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
