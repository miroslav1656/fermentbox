#include "control.h"
#include "sensors.h"
#include "config.h"
#include "io.h"
#include <math.h>

// Globální stav aktuátorů
ActuatorState actuators;

// Stav pro UI (čas běhu programu)
ControlRuntimeState controlState;

// interní časovač pro výpočet elapsedMinutes
static uint32_t lastControlUpdateMs = 0;

// "GLOBAL" bezpečnostní limity pro všechny režimy (AUTO i TEST)
static const uint32_t GLOBAL_SENSOR_TIMEOUT_MS = 8000UL;  // 8 s bez nových dat
static const float    GLOBAL_MAX_TEMP          = 50.0f;   // °C
static const float    GLOBAL_MAX_HUMIDITY      = 98.0f;   // %

// Funkce, která hlídá extrémní stavy a případně přepíše actuators
void applyGlobalSafety() {
  uint32_t now = millis();

  float t1  = sensors.temp1;
  float t2  = sensors.temp2;
  float hum = sensors.humidity;

  float avgT = NAN;
  if (!isnan(t1) && !isnan(t2))      avgT = 0.5f * (t1 + t2);
  else if (!isnan(t1))               avgT = t1;
  else if (!isnan(t2))               avgT = t2;

  bool sensorTimeout = (now - sensors.lastUpdateMs > GLOBAL_SENSOR_TIMEOUT_MS);

  // Kritický stav – senzory neaktualizují, nebo nesmyslné hodnoty
  if (sensorTimeout || isnan(avgT) || isnan(hum)) {
    actuators.heaterOn     = false;
    actuators.humidifierOn = false;
    actuators.coolFanOn    = true;
    actuators.fanPercent   = 100;
    return;
  }

  // Příliš vysoká teplota – vynutit chlazení
  if (avgT > GLOBAL_MAX_TEMP) {
    actuators.heaterOn   = false;
    actuators.coolFanOn  = true;
    actuators.fanPercent = 100;
  }

  // Příliš vysoká vlhkost – nikdy nedovolit zapnutý zvlhčovač
  if (hum > GLOBAL_MAX_HUMIDITY) {
    actuators.humidifierOn = false;
  }
}


// ----------------------------------------------------------
// Inicializace aktuátorů + runtime stavu
// ----------------------------------------------------------
void initControl() {
  actuators.heaterOn      = false;
  actuators.humidifierOn  = false;
  actuators.fanPercent    = 30;   // základní větrání
  actuators.coolFanOn     = false;

  controlState.elapsedMinutes = 0.0f;
  lastControlUpdateMs         = millis();

  Serial.println("[control] Init");
}

// ----------------------------------------------------------
// FAIL-SAFE OCHRANY
// ----------------------------------------------------------

static const float    MAX_TEMP          = 50.0f;   // absolutní strop, nouzové vypnutí
static const float    MAX_HUMIDITY      = 98.0f;   // extrémní vlhkost (kondenzace)
static const uint32_t SENSOR_TIMEOUT_MS = 8000UL;  // pokud se senzor neaktualizuje 8 s

static void failSafeMessage(const char *msg) {
  Serial.print("[FAIL-SAFE] ");
  Serial.println(msg);
}

// ----------------------------------------------------------
// Hlavní regulační smyčka
// ----------------------------------------------------------
void updateControl() {
  const ProgramSettings &prog = getActiveProgram();

  // Teplota a vlhkost z čidel
  float avgTemp = (sensors.temp1 + sensors.temp2) * 0.5f;  // zatím průměr
  float hum     = sensors.humidity;

  float tempHyst = config.tempHyst;
  float humHyst  = config.humHyst;

  uint32_t now = millis();

  // --- aktualizace času běhu programu pro UI ---
  if (lastControlUpdateMs == 0) {
    lastControlUpdateMs = now;
  } else {
    uint32_t diff = now - lastControlUpdateMs;
    // převod ms -> min
    controlState.elapsedMinutes += (float)diff / 60000.0f;
    lastControlUpdateMs = now;
  }

  // ---------- FAIL-SAFE: senzor chcípl nebo dává nesmysly ----------
  bool sensorTimedOut = (now - sensors.lastUpdateMs > SENSOR_TIMEOUT_MS);
  bool sensorBad      = isnan(avgTemp) || isnan(hum);

  if (sensorTimedOut || sensorBad) {
    // Nouzový režim – všechno vypnout, jen větrat / chladit
    actuators.heaterOn      = false;
    actuators.humidifierOn  = false;
    actuators.coolFanOn     = true;
    actuators.fanPercent    = 100;

    if (sensorTimedOut) {
      failSafeMessage("Sensor timeout! Topeni VYPNUTO, chlazeni ZAPNUTO.");
    } else {
      failSafeMessage("Senzor poslal NaN! Nouzovy rezim.");
    }
    return; // NEPOKRAČUJEME v normální regulaci
  }

  // ---------- TOPENÍ vs CHLAZENÍ (normální logika) ----------

  // 1) Pokud je MOC ZIMA -> topíme, nechladíme
  if (avgTemp < prog.targetTempC - tempHyst) {
    actuators.heaterOn  = true;
    actuators.coolFanOn = false;
  }
  // 2) Pokud jsme v pásmu OK -> netopíme, nechladíme studeným vzduchem
  else if (avgTemp >= prog.targetTempC - tempHyst &&
           avgTemp <= prog.targetTempC + tempHyst) {
    actuators.heaterOn  = false;
    actuators.coolFanOn = false;
  }
  // 3) Pokud je MOC TEPLÁ KOMORA -> chladíme studeným vzduchem z garáže
  else if (avgTemp > prog.targetTempC + tempHyst) {
    actuators.heaterOn  = false;
    actuators.coolFanOn = true;
  }

  // ---------- VLHČENÍ ----------
  if (prog.controlHumidity) {
    if (hum < prog.targetHumidity - humHyst) {
      actuators.humidifierOn = true;
    } else if (hum > prog.targetHumidity + humHyst) {
      actuators.humidifierOn = false;
    }
  } else {
    actuators.humidifierOn = false;
  }

  // ---------- VNITŘNÍ VENTILÁTOR ----------
  float delta = avgTemp - prog.targetTempC;
  int base  = 30;
  int extra = (int)(delta * 10.0f); // každý °C = +10 %
  int fan   = constrain(base + extra, 20, 100);
  actuators.fanPercent = fan;

  // ---------- DODATEČNÉ BEZPEČNOSTNÍ LIMITY ----------

  // Příliš vysoká teplota – vždy vypnout topení a naplno větrat/chladit
  if (avgTemp > MAX_TEMP) {
    actuators.heaterOn   = false;
    actuators.coolFanOn  = true;
    actuators.fanPercent = 100;
    failSafeMessage("Teplota presahla bezpecny limit! Topeni vypnuto, chlazeni zapnuto.");
  }

  // Příliš vysoká vlhkost – vypnout zvlhčovač
  if (hum > MAX_HUMIDITY) {
    actuators.humidifierOn = false;
    failSafeMessage("Vlhkost extremne vysoka! Zvlhcovac vypnut.");
  }

  // Debug výpis
  Serial.print("[control] prog=");
  Serial.print(prog.name);
  Serial.print(" T=");
  Serial.print(avgTemp);
  Serial.print("C H=");
  Serial.print(hum);
  Serial.print("%  HEAT=");
  Serial.print(actuators.heaterOn ? "ON" : "OFF");
  Serial.print(" COOL=");
  Serial.print(actuators.coolFanOn ? "ON" : "OFF");
  Serial.print(" HUMID=");
  Serial.print(actuators.humidifierOn ? "ON" : "OFF");
  Serial.print(" FAN=");
  Serial.print(actuators.fanPercent);
  Serial.println("%");
}
