#pragma once
#include <Arduino.h>

// Stav výstupů (aktuátorů)
struct ActuatorState {
    bool    heaterOn;
    bool    humidifierOn;
    uint8_t fanPercent;  // vnitřní cirkulace 0–100 %
    bool    coolFanOn;   // ventilátor, který žene studený vzduch
};

extern ActuatorState actuators;

// Stav regulace pro UI (čas běhu programu apod.)
struct ControlRuntimeState {
    float elapsedMinutes;   // jak dlouho aktuální program běží (od posledního resetu / přepnutí)
};

extern ControlRuntimeState controlState;

void initControl();
void updateControl();

// Globální bezpečnostní pojistky (platí pro AUTO i TEST režim)
void applyGlobalSafety();
