#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

RuntimeConfig config;

static const char *NVS_NAMESPACE = "fermentor";

void initConfig() {
  config.targetTemp      = 28.0f;
  config.tempHyst        = 0.5f;
  config.targetHumidity  = 70.0f;
  config.humHyst         = 3.0f;
  config.autoMode        = true;

  config.temp1Offset     = 0.0f;
  config.temp2Offset     = 0.0f;
  config.humOffset       = 0.0f;

  Serial.println("[config] Initialized default config");
}

// --- Upravené fermentační programy ---
ProgramSettings programs[PROGRAM_COUNT] = {
  // PANETTONE
  { "Panet1 testo",    26.0f, 60.0f, true,  720 },  // 12 h
  { "Panet2 forma",    28.0f, 70.0f, true,  300 },  // 5 h

  // CROISSANT
  { "Crois1 testo",    24.0f, 70.0f, true,  120 },  // 2 h
  { "Crois2 kynutí",   25.5f, 75.0f, true,   90 },  // 1.5 h

  // CHLÉB
  { "Chleb1 teplo",    25.0f, 70.0f, true,  120 },  // bulk teplý
  { "Chleb1 ledn.",     5.0f, 60.0f, true,  720 },  // bulk lednice
  { "Chleb2 forma",    25.0f, 75.0f, true,   45 },  // ve formě

  // LIEVITO MADRE
  { "LM aktivace",     28.0f, 50.0f, false, 240 },  // 4 h
  { "LM spanek",       18.0f, 50.0f, true,  480 },  // 8 h chlazení

  // VLASTNÍ
  { "Vlastni",         25.0f, 60.0f, true,    0 }   // neomezený
};


FermentationProgram currentProgram = PROGRAM_PANETTONE_BULK;

const ProgramSettings &getActiveProgram() {
  return programs[currentProgram];
}

void setProgram(FermentationProgram p) {
  if (p < PROGRAM_COUNT) {
    currentProgram = p;
  }
}

void saveConfig() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.println("[config] ERROR: prefs.begin() failed in saveConfig()");
    return;
  }

  prefs.putFloat("tTemp",  config.targetTemp);
  prefs.putFloat("tHyst",  config.tempHyst);
  prefs.putFloat("hTarg",  config.targetHumidity);
  prefs.putFloat("hHyst",  config.humHyst);
  prefs.putBool ("auto",   config.autoMode);
  prefs.putFloat("t1Off",  config.temp1Offset);
  prefs.putFloat("t2Off",  config.temp2Offset);
  prefs.putFloat("hOff",   config.humOffset);
  prefs.putUChar("prog", (uint8_t)currentProgram);

  // Vlastní program
  ProgramSettings &custom = programs[PROGRAM_CUSTOM];
  prefs.putFloat ("cTemp",  custom.targetTempC);
  prefs.putFloat ("cHum",   custom.targetHumidity);
  prefs.putBool  ("cCH",    custom.controlHumidity);
  prefs.putUShort("cDur",   custom.durationMin);

  prefs.end();
  Serial.println("[config] Configuration saved to NVS");
}

void loadConfig() {
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {
    Serial.println("[config] WARNING: prefs.begin() failed in loadConfig(), using defaults");
    return;
  }

  config.targetTemp     = prefs.getFloat("tTemp", config.targetTemp);
  config.tempHyst       = prefs.getFloat("tHyst", config.tempHyst);
  config.targetHumidity = prefs.getFloat("hTarg", config.targetHumidity);
  config.humHyst        = prefs.getFloat("hHyst", config.humHyst);
  config.autoMode       = prefs.getBool ("auto",  config.autoMode);
  config.temp1Offset    = prefs.getFloat("t1Off", config.temp1Offset);
  config.temp2Offset    = prefs.getFloat("t2Off", config.temp2Offset);
  config.humOffset      = prefs.getFloat("hOff",  config.humOffset);

  uint8_t prog = prefs.getUChar("prog", (uint8_t)currentProgram);
  if (prog < PROGRAM_COUNT) {
    currentProgram = (FermentationProgram)prog;
  }

  ProgramSettings &custom = programs[PROGRAM_CUSTOM];
  custom.targetTempC     = prefs.getFloat ("cTemp", custom.targetTempC);
  custom.targetHumidity  = prefs.getFloat ("cHum",  custom.targetHumidity);
  custom.controlHumidity = prefs.getBool  ("cCH",   custom.controlHumidity);
  custom.durationMin     = prefs.getUShort("cDur",  custom.durationMin);

  prefs.end();
  Serial.println("[config] Configuration loaded from NVS");
}
