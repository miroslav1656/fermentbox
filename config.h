#pragma once
#include <Arduino.h>

// ----------------- RuntimeConfig (obecné nastavení) -----------------

struct RuntimeConfig {
  float targetTemp;      // cílová teplota (°C)
  float tempHyst;        // hystereze teploty (°C)
  float targetHumidity;  // cílová vlhkost (%)
  float humHyst;         // hystereze vlhkosti (%)
  bool  autoMode;        // true = automat, false = manuál

  float temp1Offset;
  float temp2Offset;
  float humOffset;
};

extern RuntimeConfig config;
extern bool testMode;

// ----------------- Fermentation programs -----------------

enum FermentationProgram : uint8_t {
    PROGRAM_PANETTONE_BULK = 0,
    PROGRAM_PANETTONE_PROOF,
    PROGRAM_CROISSANT_BULK,
    PROGRAM_CROISSANT_PROOF,
    PROGRAM_CHLEBA_BULK,
    PROGRAM_CHLEBA_COLD,
    PROGRAM_CHLEBA_PROOF,
    PROGRAM_LM_ACTIVE,
    PROGRAM_LM_SLEEP,
    PROGRAM_CUSTOM,
    PROGRAM_COUNT
};

struct ProgramSettings {
    const char *name;
    float targetTempC;
    float targetHumidity;
    bool  controlHumidity;
    uint16_t durationMin;
};

extern ProgramSettings programs[PROGRAM_COUNT];
extern FermentationProgram currentProgram;

const ProgramSettings &getActiveProgram();
void setProgram(FermentationProgram p);
void initConfig();
void saveConfig();
void loadConfig();
