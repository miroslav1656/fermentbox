#include <Arduino.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "sensors.h"
#include "display.h"
#include "webui.h"
#include "io.h"
#include "control.h"
#include "mqtt.h"  

// -------------------------------------------------------
// TEST MODE + PANIC STOP
// -------------------------------------------------------
// testMode = true → NEPOUZIVA se automaticka regulace,
// ale aktuatory se ridi rucne pres Serial / web / HW tlacitka.
// VZDY ale plati globalni bezpecnost (applyGlobalSafety).
bool testMode  = false;

// panicStop = true → VSE OFF, regulace se neprovadi
bool panicStop = false;


// -------------------------------------------------------
// HARDWARE OVLÁDÁNÍ – potenciometr + 6 tlačítek
// -------------------------------------------------------

constexpr int PIN_POT         = 34; // otočný potenciometr (střední vývod na tento pin)
constexpr int PIN_BTN_MODE    = 4;  // přepnutí AUTO <-> MANUAL (TEST)
constexpr int PIN_BTN_PARAM   = 5;  // přepíná, co ovládá potenciometr (T / RH / FAN)
constexpr int PIN_BTN_HEATER  = 25; // v MANUAL (TEST) režimu: přepnout topení (změněno z GPIO 12 - strapping pin)
constexpr int PIN_BTN_HUMID   = 13; // v MANUAL (TEST) režimu: přepnout zvlhčovač
constexpr int PIN_BTN_COOL    = 14; // v MANUAL (TEST) režimu: přepnout chladicí ventilátor
constexpr int PIN_BTN_STOP    = 15; // PANIC: vypnout topení, zvlhčovač, chlazení, vnitřní fan

// 0 = teplota, 1 = vlhkost, 2 = ventilátor
// (není static, aby na to viděl i display.cpp přes extern)
uint8_t hwParamIndex = 0;

static int lastPotRaw = -1;
static int lastBtnMode   = HIGH;
static int lastBtnParam  = HIGH;
static int lastBtnHeater = HIGH;
static int lastBtnHumid  = HIGH;
static int lastBtnCool   = HIGH;
static int lastBtnStop   = HIGH;

void initHardwareControls() {
  pinMode(PIN_POT, INPUT);

  pinMode(PIN_BTN_MODE,   INPUT_PULLUP);
  pinMode(PIN_BTN_PARAM,  INPUT_PULLUP);
  pinMode(PIN_BTN_HEATER, INPUT_PULLUP);
  pinMode(PIN_BTN_HUMID,  INPUT_PULLUP);
  pinMode(PIN_BTN_COOL,   INPUT_PULLUP);
  pinMode(PIN_BTN_STOP,   INPUT_PULLUP);

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12); // rozsah 0–4095
#endif
}

// Pomocná funkce – přečte potenciometr a nastaví cílovou T/RH/FAN
static void handlePotentiometer() {
  int raw = analogRead(PIN_POT); // 0–4095 na ESP32
  if (lastPotRaw < 0) {
    lastPotRaw = raw;
  }

  // Mrtvá zóna – ignorovat drobné chvění
  if (abs(raw - lastPotRaw) < 8) {
    return;
  }
  lastPotRaw = raw;

  ProgramSettings &prog = programs[currentProgram];

  if (hwParamIndex == 0) {
    // Teplota 20–35 °C
    float t = 20.0f + (15.0f * raw / 4095.0f);
    prog.targetTempC  = t;
    config.targetTemp = t;  // pro zobrazení
    Serial.print("[POT] T = ");
    Serial.println(t, 1);
  }
  else if (hwParamIndex == 1) {
    // Vlhkost 50–90 %
    float h = 50.0f + (40.0f * raw / 4095.0f);
    prog.targetHumidity   = h;
    config.targetHumidity = h;
    Serial.print("[POT] RH = ");
    Serial.println(h, 0);
  }
  else {
    // Ventilátor 0–100 % (jen když nejsme v PANIC)
    int fan = (int)round(100.0f * raw / 4095.0f);
    if (fan < 0)   fan = 0;
    if (fan > 100) fan = 100;
    if (!panicStop) {
      actuators.fanPercent = (uint8_t)fan;
    }
    Serial.print("[POT] FAN = ");
    Serial.println(fan);
  }
}

// Edge detection – zaznamená jen přechod HIGH -> LOW (stisk)
static bool buttonPressed(int pin, int &last) {
  int cur = digitalRead(pin);
  bool pressed = (last == HIGH && cur == LOW);
  last = cur;
  return pressed;
}

void handleHardwareControls() {
  // Potenciometr můžeš používat i v AUTO – jemná korekce cíle
  handlePotentiometer();

  // MODE – přepínání AUTO/TEST (MANUAL)
  if (buttonPressed(PIN_BTN_MODE, lastBtnMode)) {
    testMode = !testMode;
    Serial.print("[HW] TEST mode = ");
    Serial.println(testMode ? "ON (MANUAL)" : "OFF (AUTO)");
  }

  // PARAM – přepnutí, co potenciometr ovládá (T / RH / FAN)
  if (buttonPressed(PIN_BTN_PARAM, lastBtnParam)) {
    hwParamIndex = (hwParamIndex + 1) % 3;
    Serial.print("[HW] Pot controls: ");
    if      (hwParamIndex == 0) Serial.println("TEMP (20-35C)");
    else if (hwParamIndex == 1) Serial.println("HUMIDITY (50-90%)");
    else                        Serial.println("FAN (0-100%)");
  }

  // STOP – PANIC (funguje vždy, AUTO i TEST)
  if (buttonPressed(PIN_BTN_STOP, lastBtnStop)) {
    panicStop = !panicStop;
    Serial.print("[HW] PANIC STOP = ");
    Serial.println(panicStop ? "ON" : "OFF");

    if (panicStop) {
      // okamžitě všechno vypnout
      actuators.heaterOn     = false;
      actuators.humidifierOn = false;
      actuators.coolFanOn    = false;
      actuators.fanPercent   = 0;
    }
  }

  // Další tlačítka mají smysl hlavně v MANUAL / TEST režimu
  if (!testMode || panicStop) return; // v AUTO nebo STOP nehrabeme do jednotlivých výstupů

  if (buttonPressed(PIN_BTN_HEATER, lastBtnHeater)) {
    actuators.heaterOn = !actuators.heaterOn;
    Serial.print("[HW MANUAL] HEATER = ");
    Serial.println(actuators.heaterOn ? "ON" : "OFF");
  }

  if (buttonPressed(PIN_BTN_HUMID, lastBtnHumid)) {
    actuators.humidifierOn = !actuators.humidifierOn;
    Serial.print("[HW MANUAL] HUMIDIFIER = ");
    Serial.println(actuators.humidifierOn ? "ON" : "OFF");
  }

  if (buttonPressed(PIN_BTN_COOL, lastBtnCool)) {
    actuators.coolFanOn = !actuators.coolFanOn;
    Serial.print("[HW MANUAL] COOL FAN = ");
    Serial.println(actuators.coolFanOn ? "ON" : "OFF");
  }
}


// -------------------------------------------------------
// Jednoduche ovladani pres Serial Monitor
// -------------------------------------------------------
//
// Prikazy:
//
// 0–4  : zmena programu (Panettone, Croissant, Chleba, LM, Vlastni)
// p    : vypsat aktualni program
// t    : prepnout TEST mod (AUTO <-> TEST)
// w    : ulozit konfiguraci do NVS (write)
// r    : nacist konfiguraci z NVS (read)
//
// V TEST modu navic:
// h    : prepnout topeni (heater)
// u    : prepnout zvlhcovac (Mlha)
// c    : prepnout chladici ventilator (cool fan)
// f    : cyklovat otacky vnitrniho ventilatoru (0/30/60/100 %)
// a    : vypnout vsechno (heater, mlha, cool, fan=0)
// s    : vypsat stav aktuatoru
//
void handleSerialCommands() {
  if (!Serial.available()) return;

  char c = Serial.read();

  // Prepinani programu (0–4) – funguje vzdy, i v TEST modu
  if (c >= '0' && c <= '4') {
    int idx = c - '0';

    if (idx < PROGRAM_COUNT) {
      setProgram((FermentationProgram)idx);

      const ProgramSettings &p = getActiveProgram();

      Serial.print("[ui] Program changed to #");
      Serial.print(idx);
      Serial.print(" - ");
      Serial.println(p.name);

      Serial.print("      target T = ");
      Serial.print(p.targetTempC);
      Serial.print(" °C, target H = ");
      Serial.print(p.targetHumidity);
      Serial.println(" %");
    }
  }

  // 'p' = vypise aktualni program
  else if (c == 'p' || c == 'P') {
    const ProgramSettings &p = getActiveProgram();
    Serial.print("[ui] Current program: ");
    Serial.print(p.name);

    Serial.print("  T=");
    Serial.print(p.targetTempC);
    Serial.print("°C  H=");
    Serial.print(p.targetHumidity);
    Serial.println("%");
  }

  // 't' = prepnuti test modu
  else if (c == 't' || c == 'T') {
    testMode = !testMode;
    Serial.print("[ui] TEST MODE = ");
    Serial.println(testMode ? "ON (manual control)" : "OFF (AUTO control)");
  }

  // 'w' = ulozit konfiguraci do NVS
  else if (c == 'w' || c == 'W') {
    saveConfig();
  }

  // 'r' = nacist konfiguraci z NVS
  else if (c == 'r' || c == 'R') {
    loadConfig();
    Serial.println("[ui] Config reloaded from NVS (note: some values may affect control)");
  }

  // dalsi prikazy maji smysl jen v TEST modu
  else if (testMode) {
    if (c == 'h' || c == 'H') {
      actuators.heaterOn = !actuators.heaterOn;
      Serial.print("[test] HEATER = ");
      Serial.println(actuators.heaterOn ? "ON" : "OFF");
    }
    else if (c == 'u' || c == 'U') {
      actuators.humidifierOn = !actuators.humidifierOn;
      Serial.print("[test] MLHA = ");
      Serial.println(actuators.humidifierOn ? "ON" : "OFF");
    }
    else if (c == 'c' || c == 'C') {
      actuators.coolFanOn = !actuators.coolFanOn;
      Serial.print("[test] CHLAZENI = ");
      Serial.println(actuators.coolFanOn ? "ON" : "OFF");
    }
    else if (c == 'f' || c == 'F') {
      // cyklovani 0 -> 30 -> 60 -> 100 -> 0 ...
      int cur = actuators.fanPercent;
      if      (cur < 10)  cur = 30;
      else if (cur < 40)  cur = 60;
      else if (cur < 80)  cur = 100;
      else                cur = 0;
      actuators.fanPercent = cur;

      Serial.print("[test] FAN percent = ");
      Serial.print(cur);
      Serial.println(" %");
    }
    else if (c == 'a' || c == 'A') {
      actuators.heaterOn     = false;
      actuators.humidifierOn = false;
      actuators.coolFanOn    = false;
      actuators.fanPercent   = 0;
      Serial.println("[test] ALL ACTUATORS OFF");
    }
    else if (c == 's' || c == 'S') {
      Serial.print("[test] STATE: HEAT=");
      Serial.print(actuators.heaterOn ? "ON" : "OFF");
      Serial.print(" MLHA=");
      Serial.print(actuators.humidifierOn ? "ON" : "OFF");
      Serial.print(" COOL=");
      Serial.print(actuators.coolFanOn ? "ON" : "OFF");
      Serial.print(" FAN=");
      Serial.print(actuators.fanPercent);
      Serial.println("%");
    }
  }
}


// -------------------------------------------------------
// SETUP
// -------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== FermentorBox starting ===");

  // Initialize Hardware Watchdog Timer (10 second timeout)
  // If ESP32 freezes with heater active, watchdog will restart the device
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL);
  Serial.println("[watchdog] Initialized (10s timeout)");

  initConfig();
  loadConfig();

  initSensors();
  initDisplay();
  initIO();
  initControl();
  initWebUI();
  //initMQTT(); 

  initHardwareControls();   // HW potenciometr + tlačítka

  Serial.println("Setup done.");
}


// -------------------------------------------------------
// LOOP
// -------------------------------------------------------
void loop() {
  // Reset watchdog timer to prevent ESP32 restart
  esp_task_wdt_reset();

  handleSerialCommands();  
  handleHardwareControls();   // fyzické ovládání
  updateSensors();            // senzory cteme vzdy

  if (panicStop) {
    // PANIC: vsechno vypnuto, zadna regulace
    actuators.heaterOn     = false;
    actuators.humidifierOn = false;
    actuators.coolFanOn    = false;
    actuators.fanPercent   = 0;
    applyActuators();
  } else if (testMode) {
    // TEST REZIM: rucne nastavene actuators, ALE porad plati globalni bezpecnost
    applyGlobalSafety();
    applyActuators();
  } else {
    // AUTO REZIM: automaticka regulace + globalni bezpecnost
    updateControl();
    applyGlobalSafety();
    applyActuators();
  }

  updateDisplay();
  handleWebUI();
  // mqttLoop(); 

  delay(50);
}
