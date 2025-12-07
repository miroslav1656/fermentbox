#include <Arduino.h>
#include "io.h"
#include "control.h"

void initIO() {
  // Relé / MOSFETy jako digitální výstupy
  pinMode(PIN_HEATER_RELAY, OUTPUT);
  pinMode(PIN_HUMIDIFIER_RELAY, OUTPUT);
  pinMode(PIN_COOL_FAN_RELAY, OUTPUT);

  digitalWrite(PIN_HEATER_RELAY, LOW);
  digitalWrite(PIN_HUMIDIFIER_RELAY, LOW);
  digitalWrite(PIN_COOL_FAN_RELAY, LOW);

  // PWM pro vnitřní ventilátor (Noctua) – modrý drát na PIN_FAN_PWM (GPIO19)
  pinMode(PIN_FAN_PWM, OUTPUT);

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
  // Na ESP32 zvedneme frekvenci PWM na 25 kHz, aby to bylo mimo slyšitelné pásmo
  analogWriteFrequency(PIN_FAN_PWM, FAN_PWM_FREQ_HZ);
#endif

  analogWrite(PIN_FAN_PWM, 0);  // ventilátor vypnutý
}

void applyActuators() {
  // Převod stavu z 'actuators' na GPIO
  digitalWrite(PIN_HEATER_RELAY,     actuators.heaterOn     ? HIGH : LOW);
  digitalWrite(PIN_HUMIDIFIER_RELAY, actuators.humidifierOn ? HIGH : LOW);
  digitalWrite(PIN_COOL_FAN_RELAY,   actuators.coolFanOn    ? HIGH : LOW);

  // Ventilátor – převod procent (0–100) na PWM (0–255)
  int fanPercent = actuators.fanPercent;

  // (volitelné) Potlačení "bručení" při úplně nízkém duty:
  //  - 0 %       = úplně vypnuto
  //  - 1–29 %    = zaokrouhlíme na 30 %
  //  - 30–100 %  = normální PWM
  if (fanPercent > 0 && fanPercent < 30) {
    fanPercent = 30;  // minimální "rozumné" otáčky
  }

  int duty = map(fanPercent, 0, 100, 0, 255);
  duty = constrain(duty, 0, 255);

  analogWrite(PIN_FAN_PWM, duty);
}
