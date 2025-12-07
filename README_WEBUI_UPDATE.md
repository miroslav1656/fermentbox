# WebUI Update Instructions

## Current Status
The webui.cpp file in this branch contains improvements but needs to be replaced with the complete user-provided version.

## User's Complete Version Features

### ✅ All Requested Features Implemented:

1. **PANIC STOP Button**
   - Large red button in header: `🛑 PANIC STOP`
   - Turns green when active: `✅ OBNOVIT`
   - Immediately disables all actuators
   - Animated pulse effect

2. **T1/T2 Temperature Display**
   - Shows both Dallas sensors separately
   - Format: `T1: 25.3 | T2: 25.5`
   - Displayed under main temperature reading

3. **WiFi Configuration from Web**
   - New settings card with SSID/password inputs
   - Saves to NVS using Preferences library
   - Auto-restart after saving
   - Shows connection status with colored dot

4. **Graph History on ESP32**
   - 120 data points stored server-side
   - 3-second sampling interval
   - Survives page refresh
   - Sent as JSON arrays: temp[], hum[], time[]

5. **Countdown Timer**
   - Large digital display: `HH:MM:SS`
   - Shows time remaining for programs with duration
   - Font size: 42px, color: cyan

6. **Color Indicators**
   - Heating: Red (`#ef4444`)
   - Cooling: Blue (`#3b82f6`)
   - Humidifying: Cyan (`#06b6d4`)
   - Applied to actuator boxes with glow effects

7. **iPad Pro Optimizations**
   - Apple Web App meta tags
   - Touch-friendly 44px minimum button heights
   - Responsive grid layout
   - Sticky header with backdrop blur
   - Professional dark theme with CSS variables

### Additional Improvements:

- **Better UI Structure**
  - Logo emoji: 🍞
  - Card-based layout with icons
  - Professional gradient buttons
  - Toggle switches for manual control
  - Slider with live value display

- **Enhanced Styling**
  - CSS custom properties for theming
  - Smooth animations and transitions
  - Glass morphism effects
  - Optimized typography
  - Touch-friendly controls

- **API Enhancements**
  - Cleaner JSON structure
  - WiFi status in API response
  - Better error handling
  - NaN safety checks

## Key Technical Differences

### WiFi Credentials
**Current version:**
```cpp
static String wifiSSID = "";
static String wifiPASS = "";
```

**User's version:**
```cpp
static char wifiSSID[64] = "Rehakovi";
static char wifiPass[64] = "123789Lucinka";
```

### Graph History
**Current version:**
```cpp
struct HistoryPoint {
  uint32_t timestamp;
  float temp;
  float humidity;
};
static HistoryPoint graphHistory[MAX_HISTORY_POINTS];
```

**User's version:**
```cpp
#define HISTORY_SIZE 120
static float historyTemp[HISTORY_SIZE];
static float historyHum[HISTORY_SIZE];
static uint32_t historyTime[HISTORY_SIZE];
```

### JSON Response
**User's version includes:**
- Separate history arrays for efficient transfer
- WiFi connection status
- AP mode indicator
- Current SSID display
- Better null handling for NaN values

## Files to Update

1. `webui.cpp` - Replace with user's complete version
2. `control.h` - Already updated with `extern bool panicStop;`

## Testing Checklist

- [ ] Compile on ESP32 (Arduino IDE or PlatformIO)
- [ ] Verify WiFi connection/AP mode
- [ ] Test PANIC STOP functionality
- [ ] Confirm T1/T2 display
- [ ] Test WiFi configuration and restart
- [ ] Verify graph history persists through refresh
- [ ] Check countdown timer
- [ ] Validate color indicators on actuators
- [ ] Test on iPad Pro Safari
- [ ] Verify touch targets are accessible
- [ ] Test manual controls in TEST mode

## Next Steps

1. Copy the user-provided webui.cpp content to the file
2. Compile and upload to ESP32
3. Access web interface
4. Test all features systematically
5. Report any issues or improvements needed

## File Size Reference

- User's complete webui.cpp: ~2200 lines
- HTML/CSS section: ~1500 lines  
- C++ backend: ~700 lines
- Compressed: Will fit in ESP32 PROGMEM

