#pragma once
#include <Arduino.h>

// GPIO piny pro DHT22 senzory (data pin)
constexpr int PIN_DHT1 = 27;  // první DHT22
constexpr int PIN_DHT2 = 26;  // druhý DHT22

// Stav senzorů – sdíleno mezi zbytkem programu
struct SensorState {
  float temp1;         // °C – senzor 1
  float temp2;         // °C – senzor 2
  float hum1;          // %RH – vlhkost z čidla 1
  float hum2;          // %RH – vlhkost z čidla 2
  float humidity;      // %RH – průměr (nebo jedno čidlo, když druhé nefunguje)
  uint32_t lastUpdateMs; // čas poslední aktualizace (millis)
};

extern SensorState sensors;

// Inicializace a periodická aktualizace
void initSensors();
void updateSensors();
