#include "display.h"
#include "sensors.h"
#include "config.h"
#include "control.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

static const int OLED_SDA_PIN = 32;
static const int OLED_SCL_PIN = 33;

static Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static bool oledOk = false;
static uint32_t lastDisplayMs = 0;

// pro blikání hvězdičky u aktivního parametru
static uint32_t lastBlinkMs = 0;
static bool blinkState      = false;

// limity pro ALARM
static const uint32_t SENSOR_TIMEOUT_MS = 8000UL;
static const float    MAX_TEMP          = 50.0f;
static const float    MAX_HUMIDITY      = 98.0f;

// globální proměnné z mainu
extern bool testMode;
extern bool panicStop;
extern uint8_t hwParamIndex;

void initDisplay() {
  Serial.println("[display] Init SH1106 OLED (CZ)");

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  if (!display.begin(0x3C, true)) {
    Serial.println("[display] OLED nenalezen, prepinam na Serial");
    oledOk = false;
    return;
  }

  oledOk = true;
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("FermentorBox");
  display.setCursor(0, 12);
  display.println("Spoustim...");
  display.display();
  delay(1500);
}

static void printTemp(float t) {
  if (isnan(t)) {
    display.print("--.-C");
  } else {
    display.print(t, 1);
    display.print("C");
  }
}

void updateDisplay() {
  uint32_t now = millis();

  // refresh ~4× za sekundu
  if (now - lastDisplayMs < 250) return;
  lastDisplayMs = now;

  // blikání hvězdičky každých 500 ms
  if (now - lastBlinkMs > 500) {
    blinkState = !blinkState;
    lastBlinkMs = now;
  }

  const ProgramSettings &p = getActiveProgram();

  float t1  = sensors.temp1;
  float t2  = sensors.temp2;
  float hum = sensors.humidity;

  float avgT = NAN;
  if (!isnan(t1) && !isnan(t2))      avgT = (t1 + t2) * 0.5f;
  else if (!isnan(t1))               avgT = t1;
  else if (!isnan(t2))               avgT = t2;

  bool sensorTimeout = (now - sensors.lastUpdateMs > SENSOR_TIMEOUT_MS);

  bool alarmActive = false;
  if (sensorTimeout)                       alarmActive = true;
  else if (isnan(avgT) || isnan(hum))      alarmActive = true;
  else if (avgT > MAX_TEMP)                alarmActive = true;
  else if (hum > MAX_HUMIDITY)             alarmActive = true;

  // -------------------------------
  //  Pokud OLED nefunguje → Serial
  // -------------------------------
  if (!oledOk) {
    Serial.print("[display] ");
    Serial.print(p.name);
    Serial.print(" T1=");
    Serial.print(t1,1);
    Serial.print("C T2=");
    Serial.print(t2,1);
    Serial.print("C V=");
    Serial.print(hum,1);
    Serial.print("% Mode=");
    Serial.print(testMode ? "TEST" : "AUTO");
    if (panicStop) Serial.print(" STOP");
    if (alarmActive) Serial.print(" ALARM");
    Serial.println();
    return;
  }

  // -------------------------------
  //  OLED vykreslení
  // -------------------------------
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);

  // 1) Program + rezim (radek Y=0)
  display.setCursor(0, 0);
  display.print(p.name);
  display.print(" [");
  if (panicStop) {
    display.print("STOP");
  } else {
    display.print(testMode ? "TEST" : "AUTO");
  }
  display.print("]");
  if (alarmActive) display.print(" !");

  // 2) Cilove hodnoty + hvězdička u aktivního parametru (radek Y=9)
  display.setCursor(0, 9);
  display.print("Cil T");
  if (hwParamIndex == 0 && blinkState) {
    display.print("*");
  }
  display.print("=");
  display.print(p.targetTempC, 0);
  display.print("C ");

  display.print("V");
  if (hwParamIndex == 1 && blinkState) {
    display.print("*");
  }
  display.print("=");
  display.print(p.targetHumidity, 0);
  display.print("%");

  // 3) T1 a T2 (radek Y=20)
  display.setCursor(0, 20);
  display.print("T1:");
  printTemp(t1);
  display.print("  T2:");
  printTemp(t2);

  // 4) Vlhkost (radek Y=31)
  display.setCursor(0, 31);
  display.print("Vl:");
  if (isnan(hum)) {
    display.print("--%");
  } else {
    display.print(hum, 1);
    display.print("%");
  }

  // 5) Stav vystupu / STOP / ALARM (spodni dva radky)
  if (panicStop) {
    display.setCursor(0, 42);
    display.print("PANIC STOP: vse OFF");
  } else if (alarmActive) {
    display.setCursor(0, 42);
    display.print("REZIM:");
    display.print(testMode ? "TEST" : "AUTO");
    display.print(" ALARM!");
  } else {
    // radek 1: Top + Mlha (Y=42)
    display.setCursor(0, 42);
    display.print("Top:");
    display.print(actuators.heaterOn ? "On " : "Off ");
    display.print("Mlha:");
    display.print(actuators.humidifierOn ? "On" : "Off");

    // radek 2: Vent + Chl (Y=52)
    display.setCursor(0, 52);
    display.print("Vent");
    if (hwParamIndex == 2 && blinkState) {
      display.print("*");
    }
    display.print(":");
    display.print(actuators.fanPercent);
    display.print("%  Chl:");
    display.print(actuators.coolFanOn ? "On" : "Off");
  }

  display.display();
}
