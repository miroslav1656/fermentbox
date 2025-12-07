#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "config.h"
#include "sensors.h"
#include "control.h"

//------------------------------------------------------
//  WiFi credentials - uložené v NVS
//------------------------------------------------------
static char wifiSSID[64] = "Rehakovi";
static char wifiPass[64] = "123789Lucinka";

//------------------------------------------------------
//  Fallback AP mód
//------------------------------------------------------
static const char *AP_SSID = "FermentorBox";
static const char *AP_PASS = "fermentor123";

//------------------------------------------------------
static WebServer server(80);

//------------------------------------------------------
// Historie pro graf - ukládá se na ESP32
//------------------------------------------------------
#define HISTORY_SIZE 120
static float historyTemp[HISTORY_SIZE];
static float historyHum[HISTORY_SIZE];
static uint32_t historyTime[HISTORY_SIZE];
static int historyIndex = 0;
static int historyCount = 0;
static uint32_t lastHistorySample = 0;

static void addHistorySample() {
  uint32_t now = millis();
  if (now - lastHistorySample < 3000) return;
  lastHistorySample = now;

  float t1 = sensors.temp1;
  float t2 = sensors.temp2;
  float avgTemp = NAN;
  
  if (! isnan(t1) && !isnan(t2)) {
    avgTemp = (t1 + t2) * 0.5f;
  } else if (!isnan(t1)) {
    avgTemp = t1;
  } else if (!isnan(t2)) {
    avgTemp = t2;
  }

  historyTemp[historyIndex] = avgTemp;
  historyHum[historyIndex] = sensors.humidity;
  historyTime[historyIndex] = now / 1000;

  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) historyCount++;
}

//------------------------------------------------------
// HTML UI STRÁNKA – optimalizovaná pro iPad Pro
//------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="cs">
<head>
  <meta charset="UTF-8">
  <title>FermentorBox</title>
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <link rel="apple-touch-icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🍞</text></svg>">
  <script src="https://cdn.jsdelivr.net/npm/chart. js"></script>
  <style>
    :root {
      --bg-dark: #0a0f1a;
      --bg-card: #111827;
      --bg-input: #1f2937;
      --border: #374151;
      --text: #f9fafb;
      --text-dim: #9ca3af;
      --text-muted: #6b7280;
      --green: #22c55e;
      --red: #ef4444;
      --orange: #f97316;
      --blue: #3b82f6;
      --yellow: #eab308;
      --cyan: #06b6d4;
      --purple: #a855f7;
    }

    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      -webkit-tap-highlight-color: transparent;
    }

    body {
      font-family: -apple-system, BlinkMacSystemFont, "SF Pro Display", "Segoe UI", Roboto, sans-serif;
      background: var(--bg-dark);
      color: var(--text);
      min-height: 100vh;
      overflow-x: hidden;
    }

    /* ===== HEADER ===== */
    . header {
      background: linear-gradient(180deg, #1f2937 0%, #111827 100%);
      border-bottom: 1px solid var(--border);
      padding: 16px 24px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      position: sticky;
      top: 0;
      z-index: 100;
    }

    . header-left {
      display: flex;
      align-items: center;
      gap: 16px;
    }

    .logo {
      font-size: 36px;
      filter: drop-shadow(0 2px 4px rgba(0,0,0,0.3));
    }

    .header-title {
      font-size: 26px;
      font-weight: 700;
      letter-spacing: -0.5px;
    }

    .header-sub {
      font-size: 13px;
      color: var(--text-dim);
      margin-top: 2px;
    }

    .panic-btn {
      background: linear-gradient(135deg, #dc2626 0%, #b91c1c 100%);
      border: 3px solid #fca5a5;
      border-radius: 16px;
      padding: 16px 32px;
      color: white;
      font-size: 18px;
      font-weight: 800;
      cursor: pointer;
      text-transform: uppercase;
      letter-spacing: 2px;
      box-shadow: 0 8px 24px rgba(220, 38, 38, 0.5);
      transition: all 0.2s ease;
    }

    .panic-btn:active {
      transform: scale(0.95);
    }

    .panic-btn. active {
      background: linear-gradient(135deg, #16a34a 0%, #15803d 100%);
      border-color: #86efac;
      box-shadow: 0 8px 24px rgba(22, 163, 74, 0.5);
    }

    /* ===== ALARM BANNER ===== */
    .alarm-banner {
      background: linear-gradient(90deg, rgba(239,68,68,0.2) 0%, rgba(239,68,68,0.1) 100%);
      border: 2px solid var(--red);
      border-radius: 16px;
      padding: 16px 24px;
      margin: 16px 24px;
      display: none;
      align-items: center;
      gap: 16px;
      animation: pulse 2s infinite;
    }

    . alarm-banner.visible {
      display: flex;
    }

    @keyframes pulse {
      0%, 100% { opacity: 1; border-color: var(--red); }
      50% { opacity: 0.7; border-color: #fca5a5; }
    }

    .alarm-icon {
      font-size: 32px;
    }

    .alarm-text {
      flex: 1;
      font-size: 16px;
      font-weight: 600;
      color: #fca5a5;
    }

    /* ===== MAIN LAYOUT ===== */
    .main-container {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 24px;
      padding: 24px;
      max-width: 1600px;
      margin: 0 auto;
    }

    @media (max-width: 1024px) {
      .main-container {
        grid-template-columns: 1fr;
        padding: 16px;
        gap: 16px;
      }
    }

    /* ===== CARDS ===== */
    .card {
      background: var(--bg-card);
      border: 1px solid var(--border);
      border-radius: 20px;
      padding: 24px;
      box-shadow: 0 4px 24px rgba(0,0,0,0.3);
    }

    .card-header {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 20px;
      padding-bottom: 16px;
      border-bottom: 1px solid var(--border);
    }

    .card-icon {
      font-size: 28px;
    }

    .card-title {
      font-size: 18px;
      font-weight: 700;
      letter-spacing: -0.3px;
    }

    /* ===== SENSOR DISPLAY ===== */
    .sensors-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 20px;
    }

    .sensor-box {
      background: var(--bg-dark);
      border: 2px solid var(--border);
      border-radius: 16px;
      padding: 20px;
      text-align: center;
      transition: all 0. 3s ease;
    }

    .sensor-box. heating {
      border-color: var(--red);
      box-shadow: 0 0 30px rgba(239, 68, 68, 0.3);
    }

    .sensor-box.cooling {
      border-color: var(--blue);
      box-shadow: 0 0 30px rgba(59, 130, 246, 0. 3);
    }

    .sensor-icon {
      font-size: 48px;
      margin-bottom: 8px;
    }

    .sensor-label {
      font-size: 13px;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 8px;
    }

    .sensor-value {
      font-size: 48px;
      font-weight: 800;
      line-height: 1;
      letter-spacing: -2px;
    }

    . sensor-unit {
      font-size: 24px;
      font-weight: 400;
      color: var(--text-dim);
    }

    . sensor-details {
      font-size: 12px;
      color: var(--text-muted);
      margin-top: 12px;
    }

    . sensor-target {
      display: inline-block;
      background: rgba(34, 197, 94, 0.15);
      border: 1px solid var(--green);
      color: var(--green);
      padding: 4px 12px;
      border-radius: 20px;
      font-size: 13px;
      font-weight: 600;
      margin-top: 8px;
    }

    /* ===== ACTUATORS ===== */
    .actuators-grid {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 12px;
      margin-top: 20px;
    }

    .actuator-box {
      background: var(--bg-dark);
      border: 2px solid var(--border);
      border-radius: 14px;
      padding: 16px 12px;
      text-align: center;
      transition: all 0.3s ease;
    }

    .actuator-box.on {
      border-color: var(--green);
      background: rgba(34, 197, 94, 0.1);
    }

    .actuator-box.on. heat {
      border-color: var(--red);
      background: rgba(239, 68, 68, 0. 15);
    }

    .actuator-box.on.cool {
      border-color: var(--blue);
      background: rgba(59, 130, 246, 0.15);
    }

    .actuator-box.on.humid {
      border-color: var(--cyan);
      background: rgba(6, 182, 212, 0.15);
    }

    .actuator-icon {
      font-size: 28px;
      margin-bottom: 6px;
    }

    .actuator-name {
      font-size: 11px;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }

    . actuator-status {
      font-size: 16px;
      font-weight: 700;
      margin-top: 4px;
    }

    /* ===== PROGRAM SECTION ===== */
    .program-header {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 16px;
      flex-wrap: wrap;
    }

    .program-name {
      font-size: 28px;
      font-weight: 800;
      letter-spacing: -0. 5px;
    }

    .program-details {
      font-size: 14px;
      color: var(--text-dim);
      margin-top: 4px;
    }

    .mode-badge {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 10px 20px;
      border-radius: 30px;
      font-size: 14px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 1px;
    }

    .mode-badge. auto {
      background: rgba(34, 197, 94, 0. 15);
      border: 2px solid var(--green);
      color: var(--green);
    }

    .mode-badge.test {
      background: rgba(249, 115, 22, 0.15);
      border: 2px solid var(--orange);
      color: var(--orange);
    }

    .mode-badge. panic {
      background: rgba(239, 68, 68, 0.15);
      border: 2px solid var(--red);
      color: var(--red);
      animation: pulse 1s infinite;
    }

    . mode-dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: currentColor;
    }

    /* ===== PROGRESS ===== */
    .progress-section {
      margin-top: 24px;
      padding-top: 20px;
      border-top: 1px solid var(--border);
    }

    .progress-header {
      display: flex;
      justify-content: space-between;
      margin-bottom: 12px;
    }

    .progress-text {
      font-size: 14px;
      color: var(--text-dim);
    }

    .progress-bar {
      height: 12px;
      background: var(--bg-dark);
      border-radius: 6px;
      overflow: hidden;
      border: 1px solid var(--border);
    }

    .progress-fill {
      height: 100%;
      background: linear-gradient(90deg, var(--green), var(--yellow), var(--orange));
      border-radius: 6px;
      transition: width 0. 5s ease;
    }

    .countdown {
      text-align: center;
      margin-top: 16px;
      font-size: 42px;
      font-weight: 800;
      color: var(--cyan);
      letter-spacing: 2px;
      font-variant-numeric: tabular-nums;
    }

    /* ===== CHART ===== */
    .chart-container {
      height: 280px;
      margin-top: 16px;
    }

    /* ===== FORMS ===== */
    .form-section {
      margin-top: 20px;
      padding-top: 20px;
      border-top: 1px solid var(--border);
    }

    .form-title {
      font-size: 14px;
      font-weight: 700;
      color: var(--text-dim);
      margin-bottom: 16px;
      text-transform: uppercase;
      letter-spacing: 1px;
    }

    .form-row {
      display: flex;
      align-items: center;
      gap: 12px;
      margin-bottom: 12px;
    }

    .form-row label {
      font-size: 14px;
      color: var(--text-dim);
      min-width: 130px;
    }

    .form-row input,
    .form-row select {
      flex: 1;
      padding: 12px 16px;
      border-radius: 12px;
      border: 2px solid var(--border);
      background: var(--bg-input);
      color: var(--text);
      font-size: 16px;
      transition: border-color 0.2s;
    }

    .form-row input:focus,
    .form-row select:focus {
      outline: none;
      border-color: var(--blue);
    }

    /* ===== BUTTONS ===== */
    .btn {
      padding: 14px 24px;
      border-radius: 12px;
      border: none;
      font-size: 16px;
      font-weight: 700;
      cursor: pointer;
      transition: all 0. 2s ease;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
    }

    .btn:active {
      transform: scale(0.98);
    }

    .btn-primary {
      background: linear-gradient(135deg, var(--blue) 0%, #2563eb 100%);
      color: white;
      box-shadow: 0 4px 12px rgba(59, 130, 246, 0. 4);
    }

    .btn-success {
      background: linear-gradient(135deg, var(--green) 0%, #16a34a 100%);
      color: white;
      box-shadow: 0 4px 12px rgba(34, 197, 94, 0. 4);
    }

    .btn-warning {
      background: linear-gradient(135deg, var(--orange) 0%, #ea580c 100%);
      color: white;
      box-shadow: 0 4px 12px rgba(249, 115, 22, 0. 4);
    }

    .btn-secondary {
      background: var(--bg-input);
      border: 2px solid var(--border);
      color: var(--text);
    }

    .btn-full {
      width: 100%;
    }

    /* ===== TOGGLE SWITCH ===== */
    .toggle-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 16px 0;
      border-bottom: 1px solid var(--border);
    }

    . toggle-label {
      display: flex;
      align-items: center;
      gap: 12px;
      font-size: 16px;
      font-weight: 500;
    }

    .toggle-icon {
      font-size: 24px;
    }

    .toggle-switch {
      position: relative;
      width: 60px;
      height: 34px;
    }

    .toggle-switch input {
      opacity: 0;
      width: 0;
      height: 0;
    }

    .toggle-slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: var(--bg-dark);
      border: 2px solid var(--border);
      border-radius: 34px;
      transition: 0.3s;
    }

    .toggle-slider:before {
      position: absolute;
      content: "";
      height: 26px;
      width: 26px;
      left: 2px;
      bottom: 2px;
      background: var(--text-muted);
      border-radius: 50%;
      transition: 0.3s;
    }

    .toggle-switch input:checked + .toggle-slider {
      background: var(--green);
      border-color: var(--green);
    }

    . toggle-switch input:checked + .toggle-slider:before {
      transform: translateX(26px);
      background: white;
    }

    . toggle-switch input:disabled + .toggle-slider {
      opacity: 0.4;
      cursor: not-allowed;
    }

    /* ===== SLIDER ===== */
    .slider-row {
      display: flex;
      align-items: center;
      gap: 16px;
      padding: 16px 0;
    }

    .slider-row label {
      font-size: 16px;
      min-width: 100px;
      display: flex;
      align-items: center;
      gap: 8px;
    }

    . slider-row input[type="range"] {
      flex: 1;
      height: 8px;
      -webkit-appearance: none;
      background: var(--bg-dark);
      border-radius: 4px;
      border: 1px solid var(--border);
    }

    .slider-row input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 28px;
      height: 28px;
      background: var(--blue);
      border-radius: 50%;
      cursor: pointer;
      box-shadow: 0 2px 8px rgba(59, 130, 246, 0. 5);
    }

    .slider-value {
      min-width: 60px;
      text-align: right;
      font-size: 18px;
      font-weight: 700;
      color: var(--cyan);
    }

    /* ===== WIFI STATUS ===== */
    .wifi-status {
      display: flex;
      align-items: center;
      gap: 10px;
      font-size: 14px;
      color: var(--text-dim);
      margin-bottom: 16px;
      padding: 12px 16px;
      background: var(--bg-dark);
      border-radius: 12px;
    }

    .wifi-dot {
      width: 12px;
      height: 12px;
      border-radius: 50%;
      background: var(--green);
    }

    .wifi-dot.disconnected { background: var(--red); }
    .wifi-dot. ap { background: var(--orange); }

    /* ===== UPTIME BAR ===== */
    .uptime-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 16px 0;
      margin-top: 16px;
      border-top: 1px solid var(--border);
      font-size: 14px;
      color: var(--text-muted);
    }

    /* ===== NOTIFICATION ===== */
    .notification-row {
      display: flex;
      align-items: center;
      gap: 12px;
      padding: 12px 0;
      font-size: 14px;
      color: var(--text-dim);
    }

    .notification-row input[type="checkbox"] {
      width: 20px;
      height: 20px;
      accent-color: var(--green);
    }

    /* ===== HINT ===== */
    .hint {
      font-size: 12px;
      color: var(--text-muted);
      margin-top: 8px;
    }

    . divider {
      border-top: 1px solid var(--border);
      margin: 20px 0;
    }
  </style>
</head>
<body>

<!-- HEADER -->
<header class="header">
  <div class="header-left">
    <span class="logo">🍞</span>
    <div>
      <div class="header-title">FermentorBox</div>
      <div class="header-sub">Profesionální řízení kynutí</div>
    </div>
  </div>
  <button id="panicBtn" class="panic-btn" onclick="togglePanic()">
    🛑 PANIC STOP
  </button>
</header>

<!-- ALARM -->
<div id="alarmBanner" class="alarm-banner">
  <span class="alarm-icon">⚠️</span>
  <span id="alarmText" class="alarm-text">Upozornění</span>
  <button class="btn btn-secondary" onclick="dismissAlarm()">Zavřít</button>
</div>

<!-- MAIN CONTENT -->
<div class="main-container">

  <!-- LEFT COLUMN -->
  <div>
    <!-- SENSORS -->
    <div class="card">
      <div class="card-header">
        <span class="card-icon">📊</span>
        <span class="card-title">Aktuální stav</span>
      </div>

      <div class="sensors-grid">
        <div class="sensor-box" id="tempBox">
          <div class="sensor-icon">🌡️</div>
          <div class="sensor-label">Teplota</div>
          <div class="sensor-value">
            <span id="temp">--</span><span class="sensor-unit">°C</span>
          </div>
          <div class="sensor-details">
            T1: <span id="temp1">--</span> | T2: <span id="temp2">--</span>
          </div>
          <div class="sensor-target">Cíl: <span id="targetTemp">--</span>°C</div>
        </div>

        <div class="sensor-box" id="humBox">
          <div class="sensor-icon">💧</div>
          <div class="sensor-label">Vlhkost</div>
          <div class="sensor-value">
            <span id="hum">--</span><span class="sensor-unit">%</span>
          </div>
          <div class="sensor-details">DHT22 průměr</div>
          <div class="sensor-target">Cíl: <span id="targetHum">--</span>%</div>
        </div>
      </div>

      <!-- ACTUATORS -->
      <div class="actuators-grid">
        <div class="actuator-box heat" id="actHeater">
          <div class="actuator-icon">🔥</div>
          <div class="actuator-name">Topení</div>
          <div class="actuator-status" id="heaterStatus">OFF</div>
        </div>
        <div class="actuator-box humid" id="actHumid">
          <div class="actuator-icon">💨</div>
          <div class="actuator-name">Zvlhčovač</div>
          <div class="actuator-status" id="humidStatus">OFF</div>
        </div>
        <div class="actuator-box cool" id="actCool">
          <div class="actuator-icon">❄️</div>
          <div class="actuator-name">Chlazení</div>
          <div class="actuator-status" id="coolStatus">OFF</div>
        </div>
        <div class="actuator-box" id="actFan">
          <div class="actuator-icon">🌀</div>
          <div class="actuator-name">Ventilátor</div>
          <div class="actuator-status"><span id="fanStatus">0</span>%</div>
        </div>
      </div>

      <div class="uptime-bar">
        <span>⏱️ Uptime: <span id="uptime">0</span>s</span>
        <span id="sensorStatus">✅ Senzory OK</span>
      </div>
    </div>

    <!-- CHART -->
    <div class="card" style="margin-top: 24px;">
      <div class="card-header">
        <span class="card-icon">📈</span>
        <span class="card-title">Historie (6 minut)</span>
      </div>
      <div class="chart-container">
        <canvas id="chart"></canvas>
      </div>
    </div>
  </div>

  <!-- RIGHT COLUMN -->
  <div>
    <!-- PROGRAM -->
    <div class="card">
      <div class="card-header">
        <span class="card-icon">🎯</span>
        <span class="card-title">Program</span>
      </div>

      <div class="program-header">
        <div>
          <div class="program-name" id="progName">Načítám... </div>
          <div class="program-details" id="progDetails">T: --°C | V: --%</div>
        </div>
        <div class="mode-badge auto" id="modeBadge">
          <span class="mode-dot"></span>
          <span id="modeText">AUTO</span>
        </div>
      </div>

      <div class="progress-section">
        <div class="progress-header">
          <span class="progress-text" id="progElapsed">Uplynulo: --</span>
          <span class="progress-text" id="progRemaining">Zbývá: --</span>
        </div>
        <div class="progress-bar">
          <div class="progress-fill" id="progFill" style="width: 0%"></div>
        </div>
        <div class="countdown" id="countdown">--:--:--</div>
      </div>

      <div class="form-section">
        <div class="form-row">
          <label>Vybrat program:</label>
          <select id="programSelect" onchange="setProgram(this.value)"></select>
        </div>
      </div>

      <div class="form-section">
        <div class="form-title">⚙️ Vlastní program</div>
        <div class="form-row">
          <label>Teplota (°C):</label>
          <input type="number" id="customTemp" step="0.5" min="5" max="40" value="25">
        </div>
        <div class="form-row">
          <label>Vlhkost (%):</label>
          <input type="number" id="customHum" step="1" min="0" max="100" value="70">
        </div>
        <div class="form-row">
          <label>Délka (min):</label>
          <input type="number" id="customDur" step="1" min="0" max="2880" value="60">
        </div>
        <div class="hint">0 minut = bez časového omezení</div>
        <button class="btn btn-primary btn-full" style="margin-top: 16px;" onclick="saveCustom()">
          💾 Uložit a spustit
        </button>
      </div>
    </div>

    <!-- MANUAL CONTROL -->
    <div class="card" style="margin-top: 24px;">
      <div class="card-header">
        <span class="card-icon">🔧</span>
        <span class="card-title">Ruční ovládání</span>
      </div>

      <div class="toggle-row">
        <div class="toggle-label">Režim</div>
        <button id="btnMode" class="btn btn-secondary" onclick="toggleTestMode()">
          Přepnout na TEST
        </button>
      </div>
      <div class="hint">AUTO = automatická regulace | TEST = ruční ovládání</div>

      <div class="divider"></div>

      <div class="toggle-row">
        <div class="toggle-label">
          <span class="toggle-icon">🔥</span> Topení
        </div>
        <label class="toggle-switch">
          <input type="checkbox" id="manHeater" onchange="applyManual()">
          <span class="toggle-slider"></span>
        </label>
      </div>

      <div class="toggle-row">
        <div class="toggle-label">
          <span class="toggle-icon">💨</span> Zvlhčovač
        </div>
        <label class="toggle-switch">
          <input type="checkbox" id="manHumid" onchange="applyManual()">
          <span class="toggle-slider"></span>
        </label>
      </div>

      <div class="toggle-row">
        <div class="toggle-label">
          <span class="toggle-icon">❄️</span> Chlazení
        </div>
        <label class="toggle-switch">
          <input type="checkbox" id="manCool" onchange="applyManual()">
          <span class="toggle-slider"></span>
        </label>
      </div>

      <div class="slider-row">
        <label><span class="toggle-icon">🌀</span> Ventilátor</label>
        <input type="range" id="manFan" min="0" max="100" step="5" value="0" 
               oninput="document.getElementById('manFanVal').textContent=this.value"
               onchange="applyManual()">
        <span class="slider-value"><span id="manFanVal">0</span>%</span>
      </div>
    </div>

    <!-- SETTINGS -->
    <div class="card" style="margin-top: 24px;">
      <div class="card-header">
        <span class="card-icon">⚙️</span>
        <span class="card-title">Nastavení</span>
      </div>

      <div class="form-title">Kalibrace senzorů</div>
      <div class="form-row">
        <label>Offset T1 (°C):</label>
        <input type="number" id="cfgT1Off" step="0. 1" value="0">
      </div>
      <div class="form-row">
        <label>Offset T2 (°C):</label>
        <input type="number" id="cfgT2Off" step="0. 1" value="0">
      </div>
      <div class="form-row">
        <label>Offset vlhkost (%):</label>
        <input type="number" id="cfgHumOff" step="0.1" value="0">
      </div>
      <div class="form-row">
        <label>Hystereze T (°C):</label>
        <input type="number" id="cfgTHyst" step="0.1" value="0. 5">
      </div>
      <div class="form-row">
        <label>Hystereze V (%):</label>
        <input type="number" id="cfgHHyst" step="0.1" value="3">
      </div>
      <button class="btn btn-success btn-full" style="margin-top: 16px;" onclick="saveConfig()">
        💾 Uložit kalibraci
      </button>

      <div class="divider"></div>

      <div class="form-title">📶 WiFi nastavení</div>
      <div class="wifi-status">
        <span class="wifi-dot" id="wifiDot"></span>
        <span id="wifiStatus">Připojeno</span>
        <span id="wifiIP"></span>
      </div>
      <div class="form-row">
        <label>SSID:</label>
        <input type="text" id="wifiSSID" placeholder="Název sítě">
      </div>
      <div class="form-row">
        <label>Heslo:</label>
        <input type="password" id="wifiPass" placeholder="Heslo">
      </div>
      <button class="btn btn-warning btn-full" style="margin-top: 16px;" onclick="saveWifi()">
        📶 Uložit WiFi a restartovat
      </button>

      <div class="divider"></div>

      <div class="notification-row">
        <input type="checkbox" id="soundEnabled" checked>
        <label for="soundEnabled">🔔 Zvukové upozornění při alarmu</label>
      </div>
    </div>
  </div>
</div>

<script>
let lastStatus = null;
let chart = null;
let alarmDismissed = false;

const programNames = [
  "Panettone - těsto",
  "Panettone - ve formě",
  "Croissant - těsto",
  "Croissant - kynutí",
  "Chleba - bulk (teplý)",
  "Chleba - bulk (lednice)",
  "Chleba - ve formě",
  "LM - aktivace",
  "LM - spánek",
  "Vlastní"
];

window.addEventListener('load', () => {
  initProgramSelect();
  initChart();
  refresh();
  setInterval(refresh, 3000);
});

function initProgramSelect() {
  const sel = document.getElementById('programSelect');
  programNames.forEach((name, idx) => {
    const opt = document.createElement('option');
    opt.value = idx;
    opt.textContent = name;
    sel.appendChild(opt);
  });
}

function initChart() {
  const ctx = document. getElementById('chart').getContext('2d');
  chart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: [],
      datasets: [
        {
          label: 'Teplota (°C)',
          data: [],
          borderColor: '#eab308',
          backgroundColor: 'rgba(234,179,8,0.1)',
          tension: 0.4,
          borderWidth: 3,
          fill: true,
          yAxisID: 'y'
        },
        {
          label: 'Vlhkost (%)',
          data: [],
          borderColor: '#3b82f6',
          backgroundColor: 'rgba(59,130,246,0.1)',
          tension: 0. 4,
          borderWidth: 3,
          fill: true,
          yAxisID: 'y1'
        }
      ]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      interaction: { intersect: false, mode: 'index' },
      scales: {
        x: {
          ticks: { color: '#6b7280', maxTicksLimit: 8, font: { size: 12 } },
          grid: { color: 'rgba(55,65,81,0.3)' }
        },
        y: {
          type: 'linear',
          position: 'left',
          suggestedMin: 15,
          suggestedMax: 40,
          ticks: { color: '#eab308', font: { size: 13 } },
          grid: { color: 'rgba(55,65,81,0.3)' },
          title: { display: true, text: '°C', color: '#eab308', font: { size: 14, weight: 'bold' } }
        },
        y1: {
          type: 'linear',
          position: 'right',
          min: 0,
          max: 100,
          ticks: { color: '#3b82f6', font: { size: 13 } },
          grid: { display: false },
          title: { display: true, text: '%', color: '#3b82f6', font: { size: 14, weight: 'bold' } }
        }
      },
      plugins: {
        legend: {
          labels: { color: '#f9fafb', font: { size: 13 }, padding: 20 }
        }
      }
    }
  });
}

async function refresh() {
  try {
    const r = await fetch('/api/status');
    if (! r.ok) return;
    const s = await r.json();
    lastStatus = s;
    updateUI(s);
  } catch (e) {
    console.error('Refresh error:', e);
  }
}

function updateUI(s) {
  // Temperatures
  document.getElementById('temp').textContent = s.temp != null ? s.temp.toFixed(1) : '--';
  document. getElementById('temp1'). textContent = s. temp1 != null ?  s.temp1. toFixed(1) + '°C' : '--';
  document.getElementById('temp2').textContent = s.temp2 != null ? s.temp2.toFixed(1) + '°C' : '--';
  document.getElementById('targetTemp').textContent = (s.targetTemp || 0).toFixed(1);
  
  // Humidity
  document.getElementById('hum').textContent = s.humidity != null ? s. humidity. toFixed(0) : '--';
  document.getElementById('targetHum').textContent = (s.targetHumidity || 0).toFixed(0);

  // Sensor box styling
  const tempBox = document.getElementById('tempBox');
  tempBox.classList.remove('heating', 'cooling');
  if (s.heaterOn) tempBox.classList.add('heating');
  else if (s.coolFanOn) tempBox. classList.add('cooling');

  // Actuators
  updateActuator('actHeater', 'heaterStatus', s. heaterOn, 'heat');
  updateActuator('actHumid', 'humidStatus', s.humidifierOn, 'humid');
  updateActuator('actCool', 'coolStatus', s.coolFanOn, 'cool');
  
  const fanPct = s.fanPercent || 0;
  document.getElementById('fanStatus').textContent = fanPct;
  document.getElementById('actFan').classList.toggle('on', fanPct > 0);

  // Uptime & sensors
  document.getElementById('uptime').textContent = s.uptimeSec || 0;
  
  const sensorEl = document.getElementById('sensorStatus');
  if (s.sensorTimeout) {
    sensorEl.innerHTML = '❌ Chyba senzorů';
    sensorEl.style.color = '#ef4444';
  } else if (s.sensorWarning) {
    sensorEl.innerHTML = '⚠️ Pozor';
    sensorEl.style.color = '#f97316';
  } else {
    sensorEl.innerHTML = '✅ Senzory OK';
    sensorEl.style.color = '#22c55e';
  }

  // Program
  document.getElementById('progName').textContent = s.programName || 'Neznámý';
  document.getElementById('progDetails').textContent = `T: ${(s.targetTemp||0).toFixed(1)}°C | V: ${(s.targetHumidity||0).toFixed(0)}%`;

  // Mode badge
  const modeBadge = document.getElementById('modeBadge');
  const modeText = document.getElementById('modeText');
  const btnMode = document.getElementById('btnMode');
  const panicBtn = document. getElementById('panicBtn');

  if (s. panicStop) {
    modeBadge.className = 'mode-badge panic';
    modeText.textContent = 'PANIC STOP';
    panicBtn.classList. add('active');
    panicBtn.textContent = '✅ OBNOVIT';
  } else if (s.testMode) {
    modeBadge. className = 'mode-badge test';
    modeText. textContent = 'TEST';
    btnMode.textContent = 'Přepnout na AUTO';
    panicBtn.classList.remove('active');
    panicBtn.textContent = '🛑 PANIC STOP';
  } else {
    modeBadge. className = 'mode-badge auto';
    modeText. textContent = 'AUTO';
    btnMode.textContent = 'Přepnout na TEST';
    panicBtn.classList.remove('active');
    panicBtn.textContent = '🛑 PANIC STOP';
  }

  // Program select
  const progSel = document.getElementById('programSelect');
  if (progSel.value !== String(s.programId)) progSel.value = s.programId;

  // Progress
  document.getElementById('progElapsed').textContent = `Uplynulo: ${formatTime(s.elapsedMin)}`;
  if (s. durationMin > 0) {
    document.getElementById('progRemaining').textContent = `Zbývá: ${formatTime(s.remainingMin)}`;
    document.getElementById('countdown').textContent = formatCountdown(s.remainingMin);
  } else {
    document.getElementById('progRemaining').textContent = 'Bez omezení';
    document.getElementById('countdown').textContent = formatCountdown(s.elapsedMin);
  }
  document.getElementById('progFill').style. width = (s.progress || 0) + '%';

  // Manual controls
  const disabled = ! s.testMode || s.panicStop;
  document. getElementById('manHeater').disabled = disabled;
  document.getElementById('manHumid').disabled = disabled;
  document.getElementById('manCool'). disabled = disabled;
  document.getElementById('manFan').disabled = disabled;

  document.getElementById('manHeater').checked = !!s.heaterOn;
  document.getElementById('manHumid').checked = !!s. humidifierOn;
  document.getElementById('manCool'). checked = !!s.coolFanOn;
  document. getElementById('manFan').value = fanPct;
  document.getElementById('manFanVal').textContent = fanPct;

  // Config
  document.getElementById('cfgT1Off').value = (s.temp1Offset || 0). toFixed(1);
  document.getElementById('cfgT2Off').value = (s.temp2Offset || 0).toFixed(1);
  document.getElementById('cfgHumOff').value = (s.humOffset || 0).toFixed(1);
  document.getElementById('cfgTHyst').value = (s.tempHyst || 0. 5).toFixed(1);
  document.getElementById('cfgHHyst').value = (s.humHyst || 3).toFixed(1);

  // Custom program
  if (s.customTemp !== undefined) {
    document.getElementById('customTemp').value = s.customTemp. toFixed(1);
    document.getElementById('customHum').value = s. customHum.toFixed(0);
    document. getElementById('customDur').value = s. customDur;
  }

  // WiFi
  if (s.wifiConnected) {
    document.getElementById('wifiDot').className = 'wifi-dot';
    document.getElementById('wifiStatus').textContent = 'Připojeno';
    document.getElementById('wifiIP').textContent = s.wifiIP || '';
  } else if (s.apMode) {
    document.getElementById('wifiDot').className = 'wifi-dot ap';
    document.getElementById('wifiStatus'). textContent = 'AP mód';
    document. getElementById('wifiIP').textContent = s.wifiIP || '';
  } else {
    document. getElementById('wifiDot').className = 'wifi-dot disconnected';
    document.getElementById('wifiStatus').textContent = 'Odpojeno';
  }

  // Alarm
  const alarmBanner = document.getElementById('alarmBanner');
  if (s.alarmActive && !alarmDismissed) {
    alarmBanner.classList.add('visible');
    document.getElementById('alarmText').textContent = s.alarmMessage || 'Upozornění';
  } else {
    alarmBanner.classList.remove('visible');
  }

  // Chart
  if (s.history) updateChart(s.history);
}

function updateActuator(boxId, statusId, isOn, type) {
  const box = document.getElementById(boxId);
  const status = document.getElementById(statusId);
  if (isOn) {
    box.classList.add('on');
    status. textContent = 'ON';
    status.style.color = type === 'heat' ? '#ef4444' : type === 'cool' ? '#3b82f6' : '#06b6d4';
  } else {
    box.classList.remove('on');
    status.textContent = 'OFF';
    status. style.color = '#6b7280';
  }
}

function updateChart(history) {
  if (!history || ! history.temp || history.temp.length === 0) return;
  chart.data.labels = history.time. map(t => {
    const m = Math.floor(t / 60) % 60;
    const s = t % 60;
    return m + ':' + String(s).padStart(2, '0');
  });
  chart.data. datasets[0].data = history.temp.map(v => v === null ? null : v);
  chart.data.datasets[1]. data = history.hum.map(v => v === null ?  null : v);
  chart.update('none');
}

function formatTime(min) {
  if (min == null || min < 0) return '--';
  const h = Math.floor(min / 60);
  const m = Math.floor(min % 60);
  return h > 0 ? h + 'h ' + m + 'm' : m + ' min';
}

function formatCountdown(min) {
  if (min == null || min < 0) return '--:--:--';
  const h = Math.floor(min / 60);
  const m = Math.floor(min % 60);
  const s = Math.floor((min * 60) % 60);
  return String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0') + ':' + String(s). padStart(2, '0');
}

function dismissAlarm() {
  alarmDismissed = true;
  document.getElementById('alarmBanner').classList. remove('visible');
  setTimeout(() => { alarmDismissed = false; }, 60000);
}

// API calls
async function setProgram(id) {
  await fetch('/setProgram? id=' + id);
  refresh();
}

async function saveCustom() {
  const t = document.getElementById('customTemp').value;
  const h = document.getElementById('customHum').value;
  const d = document.getElementById('customDur').value;
  await fetch('/setCustom?temp=' + t + '&hum=' + h + '&dur=' + d);
  await setProgram(9);
}

async function saveConfig() {
  const p = new URLSearchParams({
    t1off: document.getElementById('cfgT1Off').value,
    t2off: document.getElementById('cfgT2Off').value,
    hoff: document.getElementById('cfgHumOff').value,
    thyst: document.getElementById('cfgTHyst').value,
    hhyst: document. getElementById('cfgHHyst').value
  });
  const r = await fetch('/setConfig?' + p);
  if (r.ok) alert('✅ Kalibrace uložena');
}

async function toggleTestMode() {
  const next = !(lastStatus?. testMode);
  await fetch('/setTestMode? mode=' + (next ?  '1' : '0'));
  refresh();
}

async function togglePanic() {
  const next = !(lastStatus?.panicStop);
  await fetch('/setPanic?stop=' + (next ?  '1' : '0'));
  refresh();
}

async function applyManual() {
  if (! lastStatus?. testMode) return;
  const p = new URLSearchParams({
    heater: document.getElementById('manHeater').checked ? '1' : '0',
    humidifier: document.getElementById('manHumid').checked ?  '1' : '0',
    cool: document.getElementById('manCool').checked ? '1' : '0',
    fan: document.getElementById('manFan'). value
  });
  await fetch('/setManual?' + p);
}

async function saveWifi() {
  const ssid = document.getElementById('wifiSSID').value;
  const pass = document.getElementById('wifiPass').value;
  if (! ssid) { alert('Zadejte SSID'); return; }
  const r = await fetch('/setWifi? ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass));
  if (r.ok) alert('✅ WiFi uloženo.  Zařízení se restartuje.. .');
}
</script>
</body>
</html>
)rawliteral";

//------------------------------------------------------
// API Handlers
//------------------------------------------------------
static void handleStatus() {
  addHistorySample();
  const ProgramSettings &p = getActiveProgram();

  float t1 = sensors.temp1;
  float t2 = sensors.temp2;
  float temp = NAN;
  if (! isnan(t1) && !isnan(t2)) temp = (t1 + t2) * 0.5f;
  else if (! isnan(t1)) temp = t1;
  else if (!isnan(t2)) temp = t2;

  float hum = sensors. humidity;
  uint32_t up = millis() / 1000;
  uint32_t now = millis();
  bool sensorTimeout = (now - sensors.lastUpdateMs > 8000UL);

  bool alarmActive = false;
  String alarmMessage;
  if (sensorTimeout) { alarmActive = true; alarmMessage = "Senzory neodpovídají! "; }
  else if (! isnan(temp) && temp > 45. 0f) { alarmActive = true; alarmMessage = "Teplota příliš vysoká!"; }
  else if (!isnan(hum) && hum > 95.0f) { alarmActive = true; alarmMessage = "Vlhkost příliš vysoká!"; }

  bool sensorWarning = ! sensorTimeout && ((! isnan(temp) && temp > 40.0f) || (!isnan(hum) && hum > 88.0f));

  float elapsedMin = controlState.elapsedMinutes;
  float durationMin = p.durationMin;
  float remainingMin = durationMin > 0 ? max(0.0f, durationMin - elapsedMin) : -1. 0f;
  int progress = durationMin > 0 ? constrain((int)(elapsedMin / durationMin * 100), 0, 100) : 0;

  String json = "{";
  json += "\"programId\":" + String((int)currentProgram) + ",";
  json += "\"programName\":\"" + String(p.name) + "\",";
  json += "\"targetTemp\":" + String(p. targetTempC, 2) + ",";
  json += "\"targetHumidity\":" + String(p.targetHumidity, 2) + ",";
  json += "\"temp\":" + (isnan(temp) ?  "null" : String(temp, 2)) + ",";
  json += "\"temp1\":" + (isnan(t1) ? "null" : String(t1, 2)) + ",";
  json += "\"temp2\":" + (isnan(t2) ? "null" : String(t2, 2)) + ",";
  json += "\"humidity\":" + (isnan(hum) ? "null" : String(hum, 2)) + ",";
  json += "\"heaterOn\":" + String(actuators.heaterOn ? 1 : 0) + ",";
  json += "\"humidifierOn\":" + String(actuators.humidifierOn ? 1 : 0) + ",";
  json += "\"fanPercent\":" + String((int)actuators.fanPercent) + ",";
  json += "\"coolFanOn\":" + String(actuators.coolFanOn ? 1 : 0) + ",";
  json += "\"testMode\":" + String(testMode ? 1 : 0) + ",";
  json += "\"panicStop\":" + String(panicStop ? 1 : 0) + ",";
  json += "\"uptimeSec\":" + String(up) + ",";
  json += "\"sensorTimeout\":" + String(sensorTimeout ? 1 : 0) + ",";
  json += "\"sensorWarning\":" + String(sensorWarning ? 1 : 0) + ",";
  json += "\"alarmActive\":" + String(alarmActive ? 1 : 0) + ",";
  json += "\"alarmMessage\":\"" + alarmMessage + "\",";
  json += "\"elapsedMin\":" + String(elapsedMin, 2) + ",";
  json += "\"durationMin\":" + String(durationMin, 2) + ",";
  json += "\"remainingMin\":" + String(remainingMin, 2) + ",";
  json += "\"progress\":" + String(progress) + ",";
  json += "\"temp1Offset\":" + String(config.temp1Offset, 2) + ",";
  json += "\"temp2Offset\":" + String(config.temp2Offset, 2) + ",";
  json += "\"humOffset\":" + String(config.humOffset, 2) + ",";
  json += "\"tempHyst\":" + String(config.tempHyst, 2) + ",";
  json += "\"humHyst\":" + String(config. humHyst, 2) + ",";

  const ProgramSettings &custom = programs[PROGRAM_CUSTOM];
  json += "\"customTemp\":" + String(custom.targetTempC, 2) + ",";
  json += "\"customHum\":" + String(custom. targetHumidity, 2) + ",";
  json += "\"customDur\":" + String(custom.durationMin) + ",";

  json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? 1 : 0) + ",";
  json += "\"apMode\":" + String(WiFi.getMode() == WIFI_AP ? 1 : 0) + ",";
  json += "\"wifiIP\":\"" + (WiFi.getMode() == WIFI_AP ? WiFi. softAPIP(). toString() : WiFi.localIP().toString()) + "\",";
  json += "\"wifiSSID\":\"" + String(wifiSSID) + "\",";

  // History
  json += "\"history\":{\"temp\":[";
  for (int i = 0; i < historyCount; i++) {
    int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    if (i > 0) json += ",";
    json += isnan(historyTemp[idx]) ? "null" : String(historyTemp[idx], 1);
  }
  json += "],\"hum\":[";
  for (int i = 0; i < historyCount; i++) {
    int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    if (i > 0) json += ",";
    json += isnan(historyHum[idx]) ?  "null" : String(historyHum[idx], 1);
  }
  json += "],\"time\":[";
  for (int i = 0; i < historyCount; i++) {
    int idx = (historyIndex - historyCount + i + HISTORY_SIZE) % HISTORY_SIZE;
    if (i > 0) json += ",";
    json += String(historyTime[idx]);
  }
  json += "]}}";

  server.send(200, "application/json", json);
}

static void handleSetProgram() {
  if (! server.hasArg("id")) { server.send(400, "text/plain", "Missing id"); return; }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= PROGRAM_COUNT) { server.send(400, "text/plain", "Invalid id"); return; }
  setProgram((FermentationProgram)id);
  server.send(200, "text/plain", "OK");
}

static void handleSetCustom() {
  if (!server.hasArg("temp") || !server. hasArg("hum") || !server.hasArg("dur")) {
    server. send(400, "text/plain", "Missing params"); return;
  }
  float temp = constrain(server.arg("temp").toFloat(), 5.0f, 40.0f);
  float hum = constrain(server. arg("hum"). toFloat(), 0.0f, 100.0f);
  int dur = constrain(server.arg("dur").toInt(), 0, 2880);
  
  ProgramSettings &custom = programs[PROGRAM_CUSTOM];
  custom.targetTempC = temp;
  custom.targetHumidity = hum;
  custom. durationMin = (uint16_t)dur;
  saveConfig();
  server.send(200, "text/plain", "OK");
}

static void handleSetConfig() {
  if (! server.hasArg("t1off") || ! server.hasArg("t2off") || !server.hasArg("hoff") ||
      ! server. hasArg("thyst") || !server.hasArg("hhyst")) {
    server.send(400, "text/plain", "Missing params");
    return;
  }
  config.temp1Offset = constrain(server.arg("t1off").toFloat(), -20. 0f, 20.0f);
  config.temp2Offset = constrain(server.arg("t2off").toFloat(), -20. 0f, 20.0f);
  config.humOffset = constrain(server.arg("hoff").toFloat(), -20.0f, 20.0f);
  config. tempHyst = constrain(server.arg("thyst").toFloat(), 0.1f, 10.0f);
  config.humHyst = constrain(server.arg("hhyst").toFloat(), 0. 1f, 20.0f);
  saveConfig();
  server.send(200, "text/plain", "OK");
}

static void handleSetTestMode() {
  if (!server.hasArg("mode")) {
    server.send(400, "text/plain", "Missing mode");
    return;
  }
  testMode = (server.arg("mode"). toInt() != 0);
  server.send(200, "text/plain", "OK");
}

static void handleSetPanic() {
  if (!server.hasArg("stop")) {
    server.send(400, "text/plain", "Missing stop");
    return;
  }
  panicStop = (server.arg("stop").toInt() != 0);
  if (panicStop) {
    actuators.heaterOn = false;
    actuators.humidifierOn = false;
    actuators.coolFanOn = false;
    actuators.fanPercent = 0;
  }
  server.send(200, "text/plain", "OK");
}

static void handleSetManual() {
  if (!testMode) {
    server.send(400, "text/plain", "Not in test mode");
    return;
  }
  if (server.hasArg("heater")) actuators.heaterOn = (server.arg("heater").toInt() != 0);
  if (server. hasArg("humidifier")) actuators.humidifierOn = (server.arg("humidifier"). toInt() != 0);
  if (server.hasArg("cool")) actuators.coolFanOn = (server.arg("cool"). toInt() != 0);
  if (server.hasArg("fan")) actuators.fanPercent = constrain(server.arg("fan").toInt(), 0, 100);
  server.send(200, "text/plain", "OK");
}

static void handleSetWifi() {
  if (!server.hasArg("ssid")) {
    server. send(400, "text/plain", "Missing ssid");
    return;
  }
  String ssid = server. arg("ssid");
  String pass = server.hasArg("pass") ? server.arg("pass") : "";

  Preferences prefs;
  prefs.begin("fermentbox", false);
  prefs.putString("wifiSSID", ssid);
  prefs.putString("wifiPass", pass);
  prefs.end();

  server.send(200, "text/plain", "OK");
  delay(1000);
  ESP.restart();
}

static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

//------------------------------------------------------
// Init & Loop
//------------------------------------------------------
void initWebUI() {
  // Load WiFi credentials from NVS
  Preferences prefs;
  if (prefs.begin("fermentbox", true)) {
    String ssid = prefs.getString("wifiSSID", wifiSSID);
    String pass = prefs.getString("wifiPass", wifiPass);
    ssid. toCharArray(wifiSSID, sizeof(wifiSSID));
    pass. toCharArray(wifiPass, sizeof(wifiPass));
    prefs.end();
  }

  Serial.println("[webui] Connecting to WiFi...");
  WiFi. mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPass);

  unsigned long start = millis();
  while (millis() - start < 8000UL) {
    if (WiFi. status() == WL_CONNECTED) break;
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[webui] Connected.  IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[webui] WiFi failed, starting AP.. .");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("[webui] AP started. IP: ");
    Serial.println(WiFi.softAPIP());
  }

  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/setProgram", handleSetProgram);
  server.on("/setCustom", handleSetCustom);
  server.on("/setConfig", handleSetConfig);
  server.on("/setTestMode", handleSetTestMode);
  server.on("/setPanic", handleSetPanic);
  server.on("/setManual", handleSetManual);
  server.on("/setWifi", handleSetWifi);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("[webui] Server started");
}

void handleWebUI() {
  server.handleClient();
}