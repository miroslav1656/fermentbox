#include <Arduino.h>
#include <DHT.h>

#include "sensors.h"
#include "config.h"

SensorState sensors;

// Typ čidla – používáme DHT22 (AM2302)
#define DHTTYPE DHT22

static DHT dht1(PIN_DHT1, DHTTYPE);
static DHT dht2(PIN_DHT2, DHTTYPE);

// DHT22 je pomalejší – nečíst častěji než 1× za ~2 s
static const uint32_t DHT_INTERVAL_MS = 2000;

void initSensors() {
  dht1.begin();
  dht2.begin();

  sensors.temp1        = NAN;
  sensors.temp2        = NAN;
  sensors.hum1         = NAN;
  sensors.hum2         = NAN;
  sensors.humidity     = NAN;
  sensors.lastUpdateMs = 0;

  Serial.println("[sensors] Initialized (2x DHT22)");
}

void updateSensors() {
  uint32_t now = millis();
  if (now - sensors.lastUpdateMs < DHT_INTERVAL_MS) {
    // ještě nečteme, moc brzy
    return;
  }

  // --- ČTENÍ OBOU ČIDEL ---
  float h1 = dht1.readHumidity();
  float t1 = dht1.readTemperature();  // °C

  float h2 = dht2.readHumidity();
  float t2 = dht2.readTemperature();  // °C

  bool ok1 = !isnan(t1) && !isnan(h1);
  bool ok2 = !isnan(t2) && !isnan(h2);

  if (!ok1 && !ok2) {
    // ani jedno čidlo nedalo platná data – necháme lastUpdateMs beze změny,
    // tím se po čase aktivuje fail-safe v control.cpp
    Serial.println("[sensors] ERROR: both DHT22 reads failed (NaN)");
    return;
  }

  // --- ZPRACOVÁNÍ DAT ---
  float useTemp1 = NAN;
  float useTemp2 = NAN;
  float useHum1  = NAN;
  float useHum2  = NAN;
  float useHumAvg = NAN;

  if (ok1 && ok2) {
    // obě čidla OK – použijeme je tak, jak jsou
    useTemp1 = t1;
    useTemp2 = t2;
    useHum1  = h1;
    useHum2  = h2;
    useHumAvg = (h1 + h2) * 0.5f;

    Serial.print("[sensors] DHT1 T=");
    Serial.print(t1, 1);
    Serial.print("C RH=");
    Serial.print(h1, 1);
    Serial.print("% | DHT2 T=");
    Serial.print(t2, 1);
    Serial.print("C RH=");
    Serial.print(h2, 1);
    Serial.println("%");
  }
  else if (ok1 && !ok2) {
    // funguje jen první čidlo – použijeme ho pro obě teploty
    useTemp1 = t1;
    useTemp2 = t1;
    useHum1  = h1;
    useHum2  = NAN;
    useHumAvg = h1;

    Serial.print("[sensors] DHT1 OK T=");
    Serial.print(t1, 1);
    Serial.print("C RH=");
    Serial.print(h1, 1);
    Serial.println("% | DHT2 FAIL");
  }
  else if (!ok1 && ok2) {
    // funguje jen druhé čidlo
    useTemp1 = t2;
    useTemp2 = t2;
    useHum1  = NAN;
    useHum2  = h2;
    useHumAvg = h2;

    Serial.print("[sensors] DHT1 FAIL | DHT2 OK T=");
    Serial.print(t2, 1);
    Serial.print("C RH=");
    Serial.print(h2, 1);
    Serial.println("%");
  }

  // --- APLIKACE KALIBRAČNÍCH OFFSETŮ ---
  useTemp1 += config.temp1Offset;
  useTemp2 += config.temp2Offset;
  useHumAvg += config.humOffset;
  if (!isnan(useHum1)) useHum1 += config.humOffset;
  if (!isnan(useHum2)) useHum2 += config.humOffset;

  // --- ZAPSÁNÍ DO GLOBÁLNÍ STRUKTURY ---
  sensors.temp1        = useTemp1;
  sensors.temp2        = useTemp2;
  sensors.hum1         = useHum1;
  sensors.hum2         = useHum2;
  sensors.humidity     = useHumAvg;
  sensors.lastUpdateMs = now;
}
