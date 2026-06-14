/*
 * Arduino Multi-Mode Lighting Controller
 * 
 * Author: Jordan (ChampagneCODE3D)
 * Education: Diploma in MET (Mechanical Engineering Technology) - SAIT
 * Background: Experience in:
 *   - Industrial robotics and automation
 *   - PLC programming and ladder logic
 *   - HMI/SCADA interface design
 *   - Sensor integration and process control
 * 
 * Repository: https://github.com/ChampagneCODE3D/Arduino-sensors
 * 
 * AI DECLARATION:
 * This code was developed collaboratively with GitHub Copilot AI assistance.
 * The AI helped with:
 * - Debugging PIR/LDR sensor integration
 * - Refactoring from global occupancy state to per-mode timers
 * - Implementing early-return pattern for Modes 3-6 (bypassing switch-case static variable issues)
 * - Designing adaptive lighting algorithms (Mode 4 energy-saving, Mode 6 scanning)
 * - Creating animation state machines (Mode 5 sunrise/sunset with bounce-back effect)
 * - Optimizing RAM usage (removing SD logging, stabilizing at 56% usage)
 * - LCD UI design and countdown logic consistency
 * 
 * All design decisions, feature requirements, and testing were directed by the human developer.
 * The core project concept, sensor selection, and mode behaviors are original human work.
 * 
 * Date: June 2026
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>
#include "ButtonMap.h"

// -------------------------
// PIN SETUP
// -------------------------

const uint8_t ledPins[9] = {5, 6, 7, 8, 9, 10, 11, 12, 3};  // Green: D5-D7, Yellow: D8-D10, Red: D11, D12, D3
const int pirPin = 4;
const int ldrPin = A0;
const int irPin = 2;

LiquidCrystal_I2C lcd(0x27, 16, 2);

byte sunChar[8] = {
  B00100,
  B10101,
  B01110,
  B11111,
  B01110,
  B10101,
  B00100,
  B00000
};

// -------------------------
// STATE
// -------------------------

ProgramMode currentMode = MODE_IDLE;
const int DEFAULT_BRIGHT     = 800;
const int DEFAULT_DARK       = 77;
const int DEFAULT_SOUND_CEIL = 250;
int darkValue  = DEFAULT_DARK;
int brightValue = DEFAULT_BRIGHT;
int smoothedLightValue = 0;
int lastBarLevel = -1;
int lastRawLightValue = -1;
int lastDisplayedLight = -1;
int lastDisplayedMode = -1;
int lastDisplayedPresence = -1;
int lastMenuPage = -1;
unsigned long lastIrTime = 0;
uint8_t lastIrCode = 0xFF;
unsigned long lastEqToggleTime = 0;
enum WarnState { WARN_IDLE, WARN_INTRO, WARN_OUTRO, WARN_COUNTDOWN };
WarnState warnState = WARN_IDLE;
int warnLedIndex = 0;
unsigned long warnStepTime = 0;
unsigned long warnLastMotionTime = 0;
enum StreetState { STREET_OFF, STREET_ON, STREET_COLLAPSING };
StreetState streetState = STREET_OFF;
unsigned long streetCollapseStartTime = 0;
bool streetLightEnabled = false;  // Street light can only activate after sustained darkness
unsigned long streetLightDarkStart = 0;  // When darkness began
unsigned long streetLightBrightStart = 0;  // When brightness began
enum HallwayState { HALL_OFF, HALL_ON, HALL_FADING };
HallwayState hallwayState = HALL_OFF;
int hallwayLedCount = 0;
unsigned long hallwayFadeStartTime = 0;
unsigned long hallwayLastMotionTime = 0;
unsigned long energySaveLastMotionTime = 0;
unsigned long roomLightLastMotionTime = 0;

// -------------------------
// TEMPERATURE MODE STATE
// (Mode 7 - LM35 on A2)
// -------------------------
const int tempPin = A2;
const int soundPin = A1;  // DFR0034 sound sensor
const int uvPin    = A3;  // GUVA-S12SD UV sensor
#define UV_RAW_MAX 100  // Calibrated: raw=25 = full sun through window; adjust after outdoor test
int lastDisplayedUV = -1;  // For Mode 9 LCD refresh
bool lcdOn = true;         // LCD backlight/display toggle
bool holdDisplay = false;  // ST/REPT freezes LCD readout like a meter hold
bool ledsOff = false;      // FUNC kills LED output without leaving mode
bool inSettings = false;   // EQ settings menu active
int settingsParam = 0;     // 0 = brightValue, 1 = darkValue
int soundCeiling = DEFAULT_SOUND_CEIL;  // Sound bar top of scale (VOL+/- adjusts)
TempUnitPair tempUnitPair = TEMP_C_F;
float smoothedTempC = 0.0f;       // smoothed temperature in deg C
float lastDisplayedTempC = -999.0f; // sentinel: force first draw
TempUnitPair lastDisplayedPair = (TempUnitPair)(-1);
enum WakeUpState { WAKE_OFF, WAKE_RISING, WAKE_ON, WAKE_COUNTDOWN, WAKE_FALLING, WAKE_BOUNCING };
WakeUpState wakeUpState = WAKE_OFF;
unsigned long wakeUpStartTime = 0;
unsigned long wakeUpLastMotionTime = 0;
int wakeUpBounceFromCount = 0;  // LED count when bounce started
int lastPirStateMode3 = -1;  // For Mode 3 change detection
int lastStreetStateMode3 = -1;  // For Mode 3 change detection
unsigned long streetLightLastMotionTime = 0;  // For Mode 3 UI countdown (always runs)
int lastDisplayedSound = -1;  // For Mode 8 LCD refresh

// -------------------------
// DICE ROLLER STATE
// (Mode 10)
// -------------------------
const uint16_t dndPresets[] PROGMEM = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
const uint8_t  dndPresetCount = 19;
uint16_t diceSides  = 20;      // Current die size (2-1000)
uint16_t diceResult = 0;       // Last roll result (0 = not yet rolled)
enum DiceState : uint8_t { DICE_SELECT, DICE_ROLLING, DICE_RESULT };
DiceState diceState = DICE_SELECT;
unsigned long diceRollStart = 0;
bool     diceNumEntry = false; // User is typing a custom die size
char     diceNumBuf[5];        // "1000\0"
uint8_t  diceNumLen   = 0;

// -------------------------
// TRACK LOGGER STATE
// (Mode 11)
// -------------------------
bool      trackRunning   = false;  // actively logging
uint32_t  trackSampleNum = 0;      // row counter
unsigned long trackLastSample = 0; // last sample timestamp (ms)
#define TRACK_INTERVAL_MS 1000     // one reading per second

// -------------------------
// MANUAL TEST MODE STATE
// (Mode 12)
// -------------------------
uint16_t testLedMask = 0;   // bit 0..8 controls each LED
uint8_t  testBarCount = 0;  // 0..9 for bar mode via Up/Down
enum TestEffect : uint8_t { TEST_SOLID, TEST_KNIGHT, TEST_SPARKLE };
TestEffect testEffect = TEST_SOLID;
bool testAnimRun = false;
unsigned long testStepAt = 0;
uint8_t testKnightPos = 0;
int8_t  testKnightDir = 1;

// Dice crit flash (nat 1 / max roll)
unsigned long diceCritFlashUntil = 0;
uint8_t diceCritFlashPhase = 0;

// Track heartbeat style
uint8_t trackHeartbeatStyle = 0; // 0=single pulse, 1=double pulse

// -------------------------
// SERIAL MESSAGE MODE STATE
// (Mode 13)
// -------------------------
char serialMsgLine1[17] = "Serial Msg Mode";
char serialMsgLine2[17] = "Send: L1|L2";
char serialMsgBuf[40];
uint8_t serialMsgLen = 0;

// -------------------------
// HELPERS
// -------------------------

void allOff() {
  for (int i = 0; i < 9; i++) {
	digitalWrite(ledPins[i], LOW);
  }
}

void allOn() {
  for (int i = 0; i < 9; i++) {
	digitalWrite(ledPins[i], HIGH);
  }
}

void setLedCount(int count) {
  if (ledsOff) return;
  count = constrain(count, 0, 9);
  for (int i = 0; i < 9; i++) {
	digitalWrite(ledPins[i], (i < count) ? HIGH : LOW);
  }
}

void greenOnly() {
  if (ledsOff) return;
  allOff();
  digitalWrite(ledPins[0], HIGH);
  digitalWrite(ledPins[1], HIGH);
  digitalWrite(ledPins[2], HIGH);
}

// ---- Dice helpers ----
void diceNextPreset() {
  for (uint8_t i = 0; i < dndPresetCount; i++) {
    uint16_t p = pgm_read_word(&dndPresets[i]);
    if (p > diceSides) { diceSides = p; return; }
  }
  diceSides = pgm_read_word(&dndPresets[0]);  // wrap to d2
}
void dicePrevPreset() {
  for (int8_t i = dndPresetCount - 1; i >= 0; i--) {
    uint16_t p = pgm_read_word(&dndPresets[i]);
    if (p < diceSides) { diceSides = p; return; }
  }
  diceSides = pgm_read_word(&dndPresets[dndPresetCount - 1]);  // wrap to d100
}
// ----------------------

void showTrackLcd() {
  static bool lastRun = false;
  static uint32_t lastSample = 0xFFFFFFFF;
  if (trackRunning == lastRun && trackSampleNum == lastSample) return;
  lastRun = trackRunning;
  lastSample = trackSampleNum;

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  if (!trackRunning) {
    lcd.print(F("Track  Stopped"));
    lcd.setCursor(0, 1);
    lcd.print(F("Play Start Log"));
  } else {
    lcd.print(F("Track #"));
    lcd.print(trackSampleNum);
    lcd.setCursor(0, 1);
    lcd.print(F("Play Stop Log"));
  }
}

void showManualTestLcd() {
  static uint16_t lastMask = 0xFFFF;
  static uint8_t  lastBar = 255;
  static uint8_t  lastEff = 255;
  static bool     lastRun = false;
  if (testLedMask == lastMask && testBarCount == lastBar && (uint8_t)testEffect == lastEff && testAnimRun == lastRun) return;
  lastMask = testLedMask;
  lastBar  = testBarCount;
  lastEff  = (uint8_t)testEffect;
  lastRun  = testAnimRun;

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  if (testEffect == TEST_SOLID) {
    lcd.print(F("FX Solid "));
    lcd.print(testBarCount);
  } else if (testEffect == TEST_KNIGHT) {
    lcd.print(F("FX Knight "));
    lcd.print(testAnimRun ? F("Run") : F("Stop"));
  } else {
    lcd.print(F("FX Spark  "));
    lcd.print(testAnimRun ? F("Run") : F("Stop"));
  }

  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  if (testEffect == TEST_SOLID) {
    lcd.print(F("1-9 Tgl Up/Dn"));
  } else {
    lcd.print(F("F/R Eff P Run"));
  }
}

void showSerialMsgLcd() {
  static char last1[17] = "";
  static char last2[17] = "";
  if (strncmp(last1, serialMsgLine1, 16) == 0 && strncmp(last2, serialMsgLine2, 16) == 0) return;
  strncpy(last1, serialMsgLine1, 16); last1[16] = '\0';
  strncpy(last2, serialMsgLine2, 16); last2[16] = '\0';

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  lcd.print(serialMsgLine1);
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(serialMsgLine2);
}

void processSerialMessageLine(const char* line) {
  // Format: line1|line2  (line2 optional)
  const char* sep = strchr(line, '|');
  if (sep) {
    uint8_t n1 = (uint8_t)min(16, (int)(sep - line));
    strncpy(serialMsgLine1, line, n1);
    serialMsgLine1[n1] = '\0';
    strncpy(serialMsgLine2, sep + 1, 16);
    serialMsgLine2[16] = '\0';
  } else {
    strncpy(serialMsgLine1, line, 16);
    serialMsgLine1[16] = '\0';
    serialMsgLine2[0] = '\0';
  }
  Serial.print(F("MSG: "));
  Serial.print(serialMsgLine1);
  Serial.print(F(" | "));
  Serial.println(serialMsgLine2);
}

// ----------------------

// Collapse inward: step 0 = all 9 on, increasing step turns off
// LEDs from both ends moving toward center (index 4).
void collapseToMiddle(int step) {
  if (ledsOff) return;
  step = constrain(step, 0, 5);
  for (int i = 0; i < 9; i++) {
    bool on = (i >= step) && (i <= 8 - step);
    digitalWrite(ledPins[i], on ? HIGH : LOW);
  }
}

void updateLightBar(int lightValue) {
  int level = map(lightValue, darkValue, brightValue, 9, 0);
  level = constrain(level, 0, 9);

  if (level == lastBarLevel && lightValue == lastRawLightValue) {
	return;
  }

  lastBarLevel = level;
  lastRawLightValue = lightValue;

  for (int i = 0; i < 9; i++) {
	digitalWrite(ledPins[i], (i < level) ? HIGH : LOW);
  }
}

bool roomDark(int lightValue) {
  return lightValue < 500;
}

// -------------------------
// TEMPERATURE HELPERS
// -------------------------

// LM35: Vout = 10 mV/deg C. With 5V ref and 10-bit ADC:
//   temp (deg C x 10) = raw x 500 / 1023
float readTempC() {
  // LM35 on PCB module: 10mV/deg C, 5V reference
  return analogRead(tempPin) * 0.48876f;
}

// LDR brightness 0-100% mapped across darkValue..brightValue range

// Map 15-35 deg C to 0-9 LEDs across human comfort range (green -> yellow -> red)
void updateTempLedBar(float tempC) {
  // Scale: 0C = 0 LEDs, 30C = 9 LEDs
  // 19C -> ~6 LEDs (into yellow), 25C -> 8 LEDs (warm/red)
  int count = (int)(tempC / 30.0f * 9.0f);
  count = constrain(count, 0, 9);
  setLedCount(count);
}

void showTempOnLcd() {
  lcd.backlight();

  // Only redraw every 20 seconds or when unit pair changes
  static unsigned long lastTempLcdUpdate = 0;
  if (tempUnitPair == lastDisplayedPair &&
      lastDisplayedTempC != -999.0f &&
      millis() - lastTempLcdUpdate < 20000) {
    return;
  }
  lastTempLcdUpdate = millis();
  lastDisplayedTempC = smoothedTempC;
  lastDisplayedPair  = tempUnitPair;

  // Convert to all unit pairs as floats
  float tC = smoothedTempC;
  float tF = tC * 9.0f / 5.0f + 32.0f;
  float tK = tC + 273.1f;
  float tR = tK * 9.0f / 5.0f;

  float v1, v2;
  char u1, u2;
  switch (tempUnitPair) {
    case TEMP_C_F: v1 = tC; u1 = 'C'; v2 = tF; u2 = 'F'; break;
    case TEMP_K_C: v1 = tK; u1 = 'K'; v2 = tC; u2 = 'C'; break;
    case TEMP_K_R: v1 = tK; u1 = 'K'; v2 = tR; u2 = 'R'; break;
    case TEMP_R_F: v1 = tR; u1 = 'R'; v2 = tF; u2 = 'F'; break;
    default:       v1 = tC; u1 = 'C'; v2 = tF; u2 = 'F'; break;
  }

  // Line 1: value + degree + scale name  e.g. "21.4°  Celsius"
  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  lcd.print(v1, 1); lcd.write(0xDF);
  lcd.setCursor(6, 0);
  lcd.print(getTempUnitName(u1));

  // Line 2: value + degree + scale name  e.g. "73.5°  Fahrenheit"
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(v2, 1); lcd.write(0xDF);
  lcd.setCursor(6, 1);
  lcd.print(getTempUnitName(u2));
}

void showSettingsLcd() {
  static int lastBright = -1;
  static int lastDark = -1;
  static int lastSound = -1;
  static int lastParam = -1;
  static int lastMode = -1;
  static int lastHelpView = -1;
  static int lastHelpPage = -1;

  int helpTick = (int)(millis() / 2500UL);
  int helpView = helpTick & 1;        // 0 = values, 1 = help
  int helpPage = helpTick % 3;        // 0..2 command hints

  if (brightValue == lastBright && darkValue == lastDark && soundCeiling == lastSound &&
      settingsParam == lastParam && (int)currentMode == lastMode &&
      helpView == lastHelpView && helpPage == lastHelpPage) {
    return;
  }

  lastBright = brightValue;
  lastDark = darkValue;
  lastSound = soundCeiling;
  lastParam = settingsParam;
  lastMode = (int)currentMode;
  lastHelpView = helpView;
  lastHelpPage = helpPage;

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);

  if (helpView == 1) {
    lcd.print(F("Eq Settings"));
    lcd.setCursor(0, 1);
    if (helpPage == 0) {
      lcd.print(F("Up Down Adjust"));
    } else if (helpPage == 1) {
      lcd.print(F("Fwd Rev Param"));
    } else {
      lcd.print(F("Func Defaults"));
    }
    return;
  }

  if (currentMode == MODE_SOUND_BAR) {
    // Sound Bar: param 0 = SndCeil, param 1 = Bright
    if (settingsParam == 0) {
      lcd.print(F(">SndCeil:"));
      lcd.print(soundCeiling);
      lcd.setCursor(0, 1);
      lcd.print(F(" Bright: "));
      lcd.print(brightValue);
    } else {
      lcd.print(F(" SndCeil:"));
      lcd.print(soundCeiling);
      lcd.setCursor(0, 1);
      lcd.print(F(">Bright: "));
      lcd.print(brightValue);
    }
  } else if (settingsParam == 0) {
    lcd.print(F(">Bright: "));
    lcd.print(brightValue);
    lcd.setCursor(0, 1);
    lcd.print(F(" Dark:   "));
    lcd.print(darkValue);
  } else {
    lcd.print(F(" Bright: "));
    lcd.print(brightValue);
    lcd.setCursor(0, 1);
    lcd.print(F(">Dark:   "));
    lcd.print(darkValue);
  }
}

// ---- Dice LCD ----
void showDiceLcd() {
  static DiceState lastDState  = (DiceState)255;
  static uint16_t  lastDResult = 65535;
  static uint16_t  lastDSides  = 65535;
  static bool      lastDNum    = false;

  bool changed = (diceState    != lastDState  ||
                  diceResult   != lastDResult ||
                  diceSides    != lastDSides  ||
                  diceNumEntry != lastDNum);
  if (!changed) return;
  lastDState  = diceState;
  lastDResult = diceResult;
  lastDSides  = diceSides;
  lastDNum    = diceNumEntry;

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);

  if (diceState == DICE_ROLLING) {
    lcd.print(F("Rolling d"));
    lcd.print(diceSides);
    lcd.print(F("..."));
  } else if (diceState == DICE_RESULT) {
    lcd.print(F("d"));
    lcd.print(diceSides);
    lcd.setCursor(7, 0);
    lcd.print(F("=> "));
    lcd.print(diceResult);
    lcd.setCursor(0, 1);
    lcd.print(F("Play Reroll"));
  } else {
    // DICE_SELECT
    lcd.print(F("> d"));
    if (diceNumEntry) {
      lcd.print(diceNumBuf);
      lcd.print(F("_"));
    } else {
      lcd.print(diceSides);
    }
    lcd.setCursor(0, 1);
    if (diceNumEntry) {
      lcd.print(F("Play Set Rev Del"));
    } else {
      int help = (int)((millis() / 2200UL) % 3);
      if (help == 0) {
        lcd.print(F("Play Roll"));
      } else if (help == 1) {
        lcd.print(F("F/R Common Eq+"));
      } else {
        lcd.print(F("0-9 Custom"));
      }
    }
  }
}
// ------------------

void showIdleMenu() {
  int page = (millis() / 2500) % 7;
  if (page == lastMenuPage) {
	return;
  }

  lastMenuPage = page;
  lcd.clear();

  switch (page) {
	case 0:
	  lcd.setCursor(0, 0);
	  lcd.print(F("Func Menu"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("1 Room Light"));
	  break;
	case 1:
	  lcd.setCursor(0, 0);
	  lcd.print(F("2 Hallway"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("3 Streetlight"));
	  break;
	case 2:
	  lcd.setCursor(0, 0);
	  lcd.print(F("4 Eco Mode"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("5 Smart Home"));
	  break;
	case 3:
	  lcd.setCursor(0, 0);
	  lcd.print(F("6 Warning Light"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("7 Temperature"));
	  break;
	case 4:
	  lcd.setCursor(0, 0);
	  lcd.print(F("8 Sound Bar"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("9 UV Index"));
	  break;
	case 5:
	  lcd.setCursor(0, 0);
	  lcd.print(F("Play Dice"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("Vol+ Track"));
	  break;
	case 6:
	  lcd.setCursor(0, 0);
	  lcd.print(F("Vol- LED FX"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("Rev Serial Msg"));
	  break;
  }
}



void showModeOnLcd(int lightValue, int pirState, int soundValue) {
  lcd.backlight();
  const char* cornerLabel = getModeCornerLabel(currentMode);

  if (currentMode == MODE_IDLE) {
	showIdleMenu();
	return;
  }

  if (currentMode == MODE_DICE) {
	showDiceLcd();
	return;
  }

  if (currentMode == MODE_TRACK) {
	showTrackLcd();
	return;
  }

  if (currentMode == MODE_MANUAL_TEST) {
	showManualTestLcd();
	return;
  }

  if (currentMode == MODE_SERIAL_MSG) {
	showSerialMsgLcd();
	return;
  }

  if (currentMode == MODE_TEMPERATURE) {
	showTempOnLcd();
	return;
  }

  if (currentMode == MODE_SOUND_BAR) {
	int raw = soundValue;
	if (lastDisplayedSound == -1 || abs(raw - lastDisplayedSound) > 5) {
	  lastDisplayedSound = raw;
	  Serial.print(F("Sound:")); Serial.println(raw);
	  lcd.setCursor(0, 0);
	  lcd.print(F("                "));
	  lcd.setCursor(0, 0);
	  lcd.print(F("Sound:"));
	  lcd.print(raw);
	  lcd.setCursor(10, 0);
	  lcd.print(cornerLabel);
	  int bars = map(raw, 0, soundCeiling, 0, 16);
	  bars = constrain(bars, 1, 16);
	  lcd.setCursor(0, 1);
	  for (int i = 0; i < 16; i++) {
		lcd.write(i < bars ? 0xFF : ' ');
	  }
	}
	return;
  }

  if (currentMode == MODE_UV_INDEX) {
	int raw = analogRead(uvPin);
	static unsigned long lastUvDebug = 0;
	if (millis() - lastUvDebug > 1000) {
	  lastUvDebug = millis();
	  Serial.print(F("A0:")); Serial.print(analogRead(A0));
	  Serial.print(F(" A1:")); Serial.print(analogRead(A1));
	  Serial.print(F(" A2:")); Serial.print(analogRead(A2));
	  Serial.print(F(" A3:")); Serial.println(analogRead(A3));
	}
	if (lastDisplayedUV == -1 || abs(raw - lastDisplayedUV) > 3) {
	  lastDisplayedUV = raw;
	  lcd.setCursor(0, 0);
	  lcd.print(F("                "));
	  lcd.setCursor(0, 0);
	  lcd.print(F("UV: "));
	  int pct = constrain((int)((long)raw * 100 / UV_RAW_MAX), 0, 100);
	  lcd.print(pct);
	  lcd.print(F("%"));
	  lcd.setCursor(10, 0);
	  lcd.print(cornerLabel);
	  int bars = map(raw, 0, UV_RAW_MAX, 0, 16);
	  bars = constrain(bars, 0, 16);
	  lcd.setCursor(0, 1);
	  for (int i = 0; i < 16; i++) {
		lcd.write(i < bars ? 0xFF : ' ');
	  }
	}
	return;
  }

  if (lightValue != lastDisplayedLight || (int)currentMode != lastDisplayedMode) {
	lcd.setCursor(0, 0);
	lcd.print(F("                "));
	lcd.setCursor(0, 0);
	lcd.write((uint8_t)0);
	lcd.print(F(" "));
	lcd.print(lightValue);
	lcd.setCursor(10, 0);
	lcd.print(cornerLabel);
	lastDisplayedLight = lightValue;
	lastDisplayedMode = (int)currentMode;
  }

  // Determine occupancy state and calculate timeout remaining
  int currentPresence = (pirState == HIGH) ? 1 : 0;
  int secondsRemaining = 0;
  bool showCountdown = false;

  // Mode 5 special handling: show "Occupied" during sunrise and bounce animations
  if (currentMode == MODE_SMART_HOME_LIGHTING && (wakeUpState == WAKE_RISING || wakeUpState == WAKE_BOUNCING)) {
	currentPresence = 1; // Treat as occupied during animations
  }

  // Mode 6 special handling: show "Occupied" during scanning animation
  if (currentMode == MODE_NIGHT_WARNING && (warnState == WARN_INTRO || warnState == WARN_OUTRO)) {
	currentPresence = 1; // Treat as occupied during scanning
  }

  if (!currentPresence) {
	// PIR is LOW - check if we're in timeout/animation period for each mode
	if (currentMode == MODE_SMART_ROOM_LIGHT) {
	  unsigned long elapsed = millis() - roomLightLastMotionTime;
	  if (elapsed < 15000UL) {
		secondsRemaining = (15000UL - elapsed) / 1000;
		showCountdown = true;
	  }
	} else if (currentMode == MODE_HALLWAY_LIGHT) {
	  unsigned long elapsed = millis() - hallwayLastMotionTime;
	  if (elapsed < 12000) {
		secondsRemaining = (12000 - elapsed) / 1000;
		showCountdown = true;
	  }
	} else if (currentMode == MODE_STREETLIGHT) {
	  // Mode 3: Always show 10-second countdown after motion stops (UI consistency)
	  unsigned long elapsed = millis() - streetLightLastMotionTime;
	  if (elapsed < 10000) {
		secondsRemaining = (10000 - elapsed) / 1000;
		showCountdown = true;
	  }
	} else if (currentMode == MODE_ENERGY_SAVING_ROOM) {
	  unsigned long elapsed = millis() - energySaveLastMotionTime;
	  if (elapsed < 15000UL) {
		secondsRemaining = (15000UL - elapsed) / 1000;
		showCountdown = true;
	  }
	} else if (currentMode == MODE_SMART_HOME_LIGHTING) {
	  // Mode 5: Show countdown during the 20-second occupancy period
	  unsigned long elapsed = millis() - wakeUpLastMotionTime;
	  if (elapsed < 20000UL && wakeUpState == WAKE_ON) {
		secondsRemaining = (20000UL - elapsed) / 1000;
		showCountdown = true;
	  }
	} else if (currentMode == MODE_NIGHT_WARNING) {
	  // Mode 6: Show countdown during the 10-second vacant period
	  if (warnState == WARN_COUNTDOWN) {
		unsigned long elapsed = millis() - warnLastMotionTime;
		if (elapsed >= 10000UL && elapsed < 20000UL) {
		  secondsRemaining = (20000UL - elapsed) / 1000;
		  showCountdown = true;
		}
	  }
	}
  }

  // Update LCD line 2 if presence or countdown changed
  static int lastSecondsRemaining = -1;

  if (currentPresence != lastDisplayedPresence || secondsRemaining != lastSecondsRemaining || (secondsRemaining == 0 && lastSecondsRemaining > 0)) {
	lcd.setCursor(0, 1);
	lcd.print(F("                "));
	lcd.setCursor(0, 1);

	if (currentPresence) {
	  lcd.print(F("Occupied"));
	} else if (showCountdown && secondsRemaining > 0) {
	  lcd.print(F("Vacant in "));
	  lcd.print(secondsRemaining);
	  lcd.print(F("s"));
	} else {
	  lcd.print(F("Vacant"));
	}
	lastDisplayedPresence = currentPresence;
	lastSecondsRemaining = secondsRemaining;
  }
}

void applyMode(int pirState, int lightValue, int soundValue) {

  // Mode 3 special handling - switch statement not working for this case
  if (currentMode == MODE_STREETLIGHT) {
	// Track motion time for UI countdown (always, regardless of light level)
	if (pirState == HIGH) {
	  streetLightLastMotionTime = millis();
	}

	// Street light: only turn on if dark AND motion detected
	if (pirState != lastPirStateMode3 || streetState != lastStreetStateMode3) {
	  Serial.print(F("Mode3: PIR="));
	  Serial.print(pirState);
	  Serial.print(F(" Light="));
	  Serial.print(lightValue);
	  Serial.print(F(" Bright="));
	  Serial.print(brightValue);
	  Serial.print(F(" State="));
	  Serial.println(streetState);
	  lastPirStateMode3 = pirState;
	  lastStreetStateMode3 = streetState;
	}

	if (pirState == HIGH && lightValue < brightValue) {
	  // Motion detected in dark - turn on
	  if (streetState != STREET_ON) Serial.println(F("-> STREET_ON"));
	  streetState = STREET_ON;
	  allOn();
	} else if (streetState == STREET_COLLAPSING) {
	  // Already collapsing - let it finish regardless of light level
	  unsigned long elapsed = millis() - streetCollapseStartTime;
	  if (elapsed >= 10000) {
		Serial.println(F("-> Collapse done, OFF"));
		streetState = STREET_OFF;
		allOff();
	  } else {
		int step = (int)(elapsed / 2000);
		collapseToMiddle(step);
	  }
	} else if (pirState == LOW && streetState == STREET_ON) {
	  // Motion stopped and we were on - start collapse
	  Serial.println(F("-> Starting collapse"));
	  streetState = STREET_COLLAPSING;
	  streetCollapseStartTime = millis();
	} else if (streetState == STREET_OFF && lightValue >= brightValue) {
	  // Already off and it's bright - stay off
	  allOff();
	}
	return; // Exit early, don't run switch
  }

  // Mode 4 special handling - switch statement not working for this case either
  if (currentMode == MODE_ENERGY_SAVING_ROOM) {
	static int lastPirMode4 = -1;
	static int lastLightMode4 = -1;

	if (pirState != lastPirMode4 || abs(lightValue - lastLightMode4) > 100) {
	  Serial.print(F("Mode4: PIR="));
	  Serial.print(pirState);
	  Serial.print(F(" Light="));
	  Serial.println(lightValue);
	  lastPirMode4 = pirState;
	  lastLightMode4 = lightValue;
	}

	if (pirState == HIGH) {
	  energySaveLastMotionTime = millis();
	}

	if (millis() - energySaveLastMotionTime < 15000UL) {
	  // Occupied or counting down - adjust LED count based on current light level
	  if (lightValue >= 500) {
		// Bright: only 1 green LED
		digitalWrite(ledPins[0], HIGH);
		digitalWrite(ledPins[1], LOW);
		digitalWrite(ledPins[2], LOW);
		digitalWrite(ledPins[3], LOW);
		digitalWrite(ledPins[4], LOW);
		digitalWrite(ledPins[5], LOW);
		digitalWrite(ledPins[6], LOW);
		digitalWrite(ledPins[7], LOW);
		digitalWrite(ledPins[8], LOW);
	  } else if (lightValue >= 250) {
		// Medium: 2 green LEDs
		digitalWrite(ledPins[0], HIGH);
		digitalWrite(ledPins[1], HIGH);
		digitalWrite(ledPins[2], LOW);
		digitalWrite(ledPins[3], LOW);
		digitalWrite(ledPins[4], LOW);
		digitalWrite(ledPins[5], LOW);
		digitalWrite(ledPins[6], LOW);
		digitalWrite(ledPins[7], LOW);
		digitalWrite(ledPins[8], LOW);
	  } else {
		// Dark: all 3 green LEDs
		greenOnly();
	  }
	} else {
	  allOff();
	}
	return; // Exit early, don't run switch
  }

  // Mode 5: Wake-Up Light - progressive animation
  if (currentMode == MODE_SMART_HOME_LIGHTING) {
	static int lastPirMode5 = -1;
	static int lastLightMode5 = -1;
	static WakeUpState lastWakeState = WAKE_OFF;

	if (pirState != lastPirMode5 || abs(lightValue - lastLightMode5) > 100 || wakeUpState != lastWakeState) {
	  Serial.print(F("Mode5: PIR="));
	  Serial.print(pirState);
	  Serial.print(F(" Light="));
	  Serial.print(lightValue);
	  Serial.print(F(" Dark="));
	  Serial.print(roomDark(lightValue));
	  Serial.print(F(" State="));
	  Serial.println(wakeUpState);
	  lastPirMode5 = pirState;
	  lastLightMode5 = lightValue;
	  lastWakeState = wakeUpState;
	}

	// Update motion timer
	if (pirState == HIGH) {
	  wakeUpLastMotionTime = millis();
	}

	// State machine
	if (wakeUpState == WAKE_OFF) {
	  // Only start if dark and motion detected
	  if (pirState == HIGH && roomDark(lightValue)) {
		Serial.println(F("-> Starting sunrise (10s)"));
		wakeUpState = WAKE_RISING;
		wakeUpStartTime = millis();
	  } else {
		allOff();
	  }
	} else if (wakeUpState == WAKE_RISING) {
	  // Rising animation (10 seconds) - continue regardless of light level
	  unsigned long elapsed = millis() - wakeUpStartTime;
	  if (elapsed >= 10000) {
		Serial.println(F("-> Sunrise complete, fully ON"));
		wakeUpState = WAKE_ON;
		allOn();
	  } else {
		int ledCount = (int)((elapsed * 9) / 10000);
		setLedCount(ledCount);
	  }
	} else if (wakeUpState == WAKE_ON) {
	  // Stay fully on while occupied - continue regardless of light level
	  allOn();
	  if (millis() - wakeUpLastMotionTime >= 20000) {
		Serial.println(F("-> Starting sunset (10s)"));
		wakeUpState = WAKE_FALLING;
		wakeUpStartTime = millis();
	  }
	} else if (wakeUpState == WAKE_FALLING) {
	  // Falling animation (10 seconds, reverse) - but can be interrupted by motion
	  if (pirState == HIGH) {
		// Calculate current LED count during sunset
		unsigned long elapsed = millis() - wakeUpStartTime;
		int currentLedCount = 9 - (int)((elapsed * 9) / 10000);
		wakeUpBounceFromCount = currentLedCount;
		Serial.print(F("-> Motion during sunset at LED "));
		Serial.print(currentLedCount);
		Serial.println(F(", bouncing back!"));
		wakeUpState = WAKE_BOUNCING;
		wakeUpStartTime = millis();
	  } else {
		unsigned long elapsed = millis() - wakeUpStartTime;
		if (elapsed >= 10000) {
		  Serial.println(F("-> Sunset complete, OFF"));
		  wakeUpState = WAKE_OFF;
		  allOff();
		} else {
		  int ledCount = 9 - (int)((elapsed * 9) / 10000);
		  setLedCount(ledCount);
		}
	  }
	} else if (wakeUpState == WAKE_BOUNCING) {
	  // Bounce back from where sunset was interrupted to fully on
	  int ledsToAdd = 9 - wakeUpBounceFromCount;  // How many LEDs to turn back on
	  if (ledsToAdd == 0) {
		// Already at 9, just go to ON
		Serial.println(F("-> Bounce complete, fully ON"));
		wakeUpState = WAKE_ON;
		allOn();
	  } else {
		unsigned long bounceDuration = ledsToAdd * 500;  // 500ms per LED
		unsigned long elapsed = millis() - wakeUpStartTime;
		if (elapsed >= bounceDuration) {
		  Serial.println(F("-> Bounce complete, fully ON"));
		  wakeUpState = WAKE_ON;
		  allOn();
		} else {
		  int additionalLeds = (int)((elapsed * ledsToAdd) / bounceDuration);
		  int ledCount = wakeUpBounceFromCount + additionalLeds;
		  setLedCount(ledCount);
		}
	  }
	}
	return; // Exit early, don't run switch
  }

  // Mode 6: Night Warning - Adaptive LED cycling pattern
  if (currentMode == MODE_NIGHT_WARNING) {
	static int lastPirMode6 = -1;
	static int lastLightMode6 = -1;
	static WarnState lastWarnState = WARN_IDLE;

	if (pirState != lastPirMode6 || abs(lightValue - lastLightMode6) > 100 || warnState != lastWarnState) {
	  Serial.print(F("Mode6: PIR="));
	  Serial.print(pirState);
	  Serial.print(F(" Light="));
	  Serial.print(lightValue);
	  Serial.print(F(" State="));
	  Serial.println(warnState);
	  lastPirMode6 = pirState;
	  lastLightMode6 = lightValue;
	  lastWarnState = warnState;
	}

	// Update motion timer
	if (pirState == HIGH) {
	  warnLastMotionTime = millis();
	  if (warnState == WARN_IDLE || warnState == WARN_COUNTDOWN) {
		Serial.println(F("-> Starting warning cycle"));
		warnState = WARN_INTRO;
		warnStepTime = millis();
		warnLedIndex = 0;
	  }
	}

	if (warnState == WARN_INTRO || warnState == WARN_OUTRO || warnState == WARN_COUNTDOWN) {
	  // Determine LED range based on current light level
	  int startLed, endLed;
	  if (lightValue >= 500) {
		// Bright: only red LEDs (6, 7, 8)
		startLed = 6;
		endLed = 8;
	  } else if (lightValue >= 250) {
		// Medium: yellow + red LEDs (3-8)
		startLed = 3;
		endLed = 8;
	  } else {
		// Dark: all LEDs (0-8)
		startLed = 0;
		endLed = 8;
	  }

	  // Cycling speed: 100ms per step
	  if (millis() - warnStepTime >= 100) {
		warnStepTime = millis();

		// Turn off all LEDs in the active range
		for (int i = startLed; i <= endLed; i++) {
		  digitalWrite(ledPins[i], LOW);
		}

		// Turn on the current LED in the cycle
		if (warnLedIndex >= startLed && warnLedIndex <= endLed) {
		  digitalWrite(ledPins[warnLedIndex], HIGH);
		}

		// Move to next LED
		warnLedIndex++;
		if (warnLedIndex > endLed) {
		  warnLedIndex = startLed;  // Loop back
		}
	  }

	  // Check if we should transition states
	  if (warnState != WARN_COUNTDOWN && millis() - warnLastMotionTime >= 10000) {
		Serial.println(F("-> Starting countdown (lights still scanning)"));
		warnState = WARN_COUNTDOWN;
	  } else if (warnState == WARN_COUNTDOWN && millis() - warnLastMotionTime >= 20000) {
		Serial.println(F("-> Countdown complete, IDLE"));
		warnState = WARN_IDLE;
		allOff();
	  }
	} else if (warnState == WARN_IDLE) {
	  allOff();
	}
	return; // Exit early, don't run switch
  }

  // Mode 7: Temperature display - LED bar maps 15-35 deg C comfort range to 0-9 LEDs
  if (currentMode == MODE_TEMPERATURE) {
    updateTempLedBar(smoothedTempC);
    return;
  }

  // Mode 8: Sound bar - peak-hold LED bar responding to mic level
  if (currentMode == MODE_SOUND_BAR) {
    int count = map(soundValue, 0, soundCeiling, 0, 9);
    count = constrain(count, 1, 9);
    setLedCount(count);
    return;
  }

  // Mode 9: UV index bar - GUVA-S12SD on A3
  if (currentMode == MODE_UV_INDEX) {
    int raw = analogRead(uvPin);
    int count = map(raw, 0, UV_RAW_MAX, 0, 9);
    count = constrain(count, 0, 9);
    setLedCount(count);
    return;
  }

  // Mode 10: Dice roller - LED bar shows roll result percentage
  if (currentMode == MODE_DICE) {
    if (diceState == DICE_ROLLING) {
      unsigned long elapsed = millis() - diceRollStart;
      if (elapsed >= 700) {
        diceResult = (uint16_t)random(1, (long)diceSides + 1);
        diceState  = DICE_RESULT;
        int ledCount = map(diceResult, 1, diceSides, 1, 9);
        setLedCount(constrain(ledCount, 1, 9));
        // tiny fun: crit flash on nat-1 or max roll
        if (diceResult == 1 || diceResult == diceSides) {
          diceCritFlashUntil = millis() + 450;
          diceCritFlashPhase = 0;
        }
        showDiceLcd();
        Serial.print(F("d")); Serial.print(diceSides);
        Serial.print(F(" => ")); Serial.println(diceResult);
      } else {
        // Counting-up animation: one new LED every ~80ms
        int frame = (int)(elapsed / 80) % 9 + 1;
        setLedCount(frame);
      }
    } else if (diceState == DICE_RESULT) {
      if (diceCritFlashUntil > millis()) {
        // 3-phase flash quickly toggles all LEDs
        if ((millis() / 90) % 2 == 0) allOn(); else allOff();
      } else {
        int ledCount = map(diceResult, 1, diceSides, 1, 9);
        setLedCount(constrain(ledCount, 1, 9));
      }
    } else {
      allOff();
    }
    return;
  }

  // Mode 11: Track Logger - stream CSV to Serial, blink LEDs while recording
  if (currentMode == MODE_TRACK) {
	if (trackRunning) {
	  unsigned long now = millis();
	  if (now - trackLastSample >= TRACK_INTERVAL_MS) {
		trackLastSample = now;
		trackSampleNum++;
		int ldr   = analogRead(ldrPin);
		int snd   = analogRead(soundPin);
		int pir   = digitalRead(pirPin);
		float tmp = readTempC();
		// CSV: sample#, ms, ldr, sound, pir, tempC
		Serial.print(trackSampleNum); Serial.print(',');
		Serial.print(now);            Serial.print(',');
		Serial.print(ldr);            Serial.print(',');
		Serial.print(snd);            Serial.print(',');
		Serial.print(pir);            Serial.print(',');
		Serial.println(tmp, 1);
		// heartbeat styles
		if (trackHeartbeatStyle == 0) {
		  digitalWrite(ledPins[0], (trackSampleNum & 1) ? HIGH : LOW);
		} else {
		  // double pulse-ish pattern on LED0/LED1
		  digitalWrite(ledPins[0], HIGH);
		  digitalWrite(ledPins[1], (trackSampleNum & 1) ? HIGH : LOW);
		}
		// refresh LCD counter every 5 samples to save redraws
		if (trackSampleNum % 5 == 0) showTrackLcd();
	  }
	} else {
	  allOff();
	}
	return;
  }

  // Mode 12: Manual LED Test
  if (currentMode == MODE_MANUAL_TEST) {
	if (testEffect == TEST_SOLID || !testAnimRun) {
	  for (int i = 0; i < 9; i++) {
		bool on = ((testLedMask >> i) & 0x01) != 0;
		digitalWrite(ledPins[i], on ? HIGH : LOW);
	  }
	} else if (millis() - testStepAt >= 120) {
	  testStepAt = millis();
	  if (testEffect == TEST_KNIGHT) {
		allOff();
		digitalWrite(ledPins[testKnightPos], HIGH);
		if (testKnightPos == 8) testKnightDir = -1;
		else if (testKnightPos == 0) testKnightDir = 1;
		testKnightPos = (uint8_t)((int)testKnightPos + testKnightDir);
	  } else if (testEffect == TEST_SPARKLE) {
		testLedMask ^= (uint16_t)(1U << random(0, 9));
		for (int i = 0; i < 9; i++) {
		  bool on = ((testLedMask >> i) & 0x01) != 0;
		  digitalWrite(ledPins[i], on ? HIGH : LOW);
		}
	  }
	}
	return;
  }

  switch (currentMode) {
	case MODE_IDLE:
	  allOff();
	  break;

	case MODE_SMART_ROOM_LIGHT:
	  // Mode 1: Room light with 15-second timeout
	  if (pirState == HIGH) {
		roomLightLastMotionTime = millis();
		if (lightValue < brightValue) {
		  updateLightBar(lightValue);
		}
	  } else if (millis() - roomLightLastMotionTime < 15000UL) {
		if (lightValue < brightValue) {
		  updateLightBar(lightValue);
		}
	  } else {
		allOff();
	  }
	  break;

	case MODE_HALLWAY_LIGHT:
	  // Track motion for occupancy countdown
	  if (pirState == HIGH) {
		hallwayLastMotionTime = millis();
		hallwayState = HALL_ON;
	  }

	  // Check if still within occupancy window (12 seconds after last motion)
	  unsigned long timeSinceMotion = millis() - hallwayLastMotionTime;

	  // Always turn off instantly if too bright (regardless of state)
	  if (lightValue >= brightValue) {
		allOff();
		if (timeSinceMotion >= 12000) {
		  hallwayState = HALL_OFF;  // Past occupancy, stay off
		}
	  } else if (timeSinceMotion < 12000) {
		// Still within occupancy window and dim enough
		hallwayState = HALL_ON;
		allOn();
	  } else {
		// Past occupancy window - fade out
		if (hallwayState == HALL_ON) {
		  hallwayState = HALL_FADING;
		  hallwayLedCount = 9;
		  hallwayFadeStartTime = millis();
		}
		if (hallwayState == HALL_FADING) {
		  unsigned long fadeElapsed = millis() - hallwayFadeStartTime;
		  if (fadeElapsed >= 12000) {
			hallwayState = HALL_OFF;
			allOff();
		  } else {
			hallwayLedCount = 9 - (int)((fadeElapsed * 9) / 12000);
			setLedCount(hallwayLedCount);
		  }
		}
	  }
	  break;
  }
}

void setMode(ProgramMode mode) {
  Serial.print(F("setMode called: "));
  Serial.println((int)mode);

  currentMode = mode;
  lastDisplayedMode = -1;
  lastDisplayedPresence = -1;
  lastDisplayedLight = -1;
  lcd.clear();

  // Reset mode-specific timers
  roomLightLastMotionTime = 0;  // Mode 1: Start with no recent motion
  hallwayLastMotionTime = 0;  // Mode 2: Start with no recent motion
  energySaveLastMotionTime = 0;  // Mode 4: Start with no recent motion
  streetLightLastMotionTime = 0;  // Mode 3: Start with no recent motion
  hallwayState = HALL_OFF;
  warnState = WARN_IDLE;
  streetState = STREET_OFF;
  streetLightEnabled = false;  // Mode 3: reset street light enable state
  streetLightDarkStart = 0;
  streetLightBrightStart = 0;
  wakeUpState = WAKE_OFF;
  tempUnitPair     = TEMP_C_F;
  lastDisplayedTempC = -999;
  lastDisplayedPair  = (TempUnitPair)(-1);
  lastDisplayedSound = -1;
  lastDisplayedUV    = -1;
  diceState    = DICE_SELECT;
  diceResult   = 0;
  diceNumEntry = false;
  diceNumLen   = 0;
  diceNumBuf[0] = '\0';
  trackRunning   = false;
  trackSampleNum = 0;
  trackLastSample = 0;
  testLedMask   = 0;
  testBarCount  = 0;
  testEffect    = TEST_SOLID;
  testAnimRun   = false;
  testStepAt    = 0;
  testKnightPos = 0;
  testKnightDir = 1;
  diceCritFlashUntil = 0;
  diceCritFlashPhase = 0;
  serialMsgLine1[0] = '\0';
  serialMsgLine2[0] = '\0';
  strncpy(serialMsgLine1, "Serial Msg Mode", 16); serialMsgLine1[16] = '\0';
  strncpy(serialMsgLine2, "Send: L1|L2", 16); serialMsgLine2[16] = '\0';
  serialMsgLen = 0;

  // SD logging removed to save RAM

  // Always turn off LEDs when switching modes to prevent state leakage
  allOff();

  if (currentMode == MODE_IDLE) {
    lastMenuPage = -1;
    showIdleMenu();
    return;
  }

  lcd.clear();
}

// -------------------------
// SETUP
// -------------------------

void setup() {
  Serial.begin(9600);

  for (int i = 0; i < 9; i++) {
	pinMode(ledPins[i], OUTPUT);
  }

  pinMode(pirPin, INPUT);
  smoothedLightValue = analogRead(ldrPin);
  smoothedTempC = readTempC();  // seed with real reading so smoothing starts correct

  lcd.init();
  lcd.createChar(0, sunChar);
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("RAW FW v1"));
  lcd.setCursor(0, 1);
  lcd.print(F("Select 1-9"));

  randomSeed(analogRead(A0) ^ (unsigned long)analogRead(A1) << 10 ^ millis());

  IrReceiver.begin(irPin, DISABLE_LED_FEEDBACK);

  pinMode(11, OUTPUT);
  pinMode(12, OUTPUT);

  setMode(MODE_IDLE);
}

// -------------------------
// MAIN LOOP
// -------------------------

void loop() {
  int pirState = digitalRead(pirPin);
  int lightValue = analogRead(ldrPin);
  int soundValue = analogRead(soundPin);
  smoothedLightValue = (smoothedLightValue * 2 + lightValue) / 3;
  smoothedTempC = (smoothedTempC * 2.0f + readTempC()) / 3.0f;

  // Dedicated serial message input (only active in Serial Msg mode)
  if (currentMode == MODE_SERIAL_MSG) {
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        serialMsgBuf[serialMsgLen] = '\0';
        if (serialMsgLen > 0) {
          processSerialMessageLine(serialMsgBuf);
          showSerialMsgLcd();
        }
        serialMsgLen = 0;
      } else if (serialMsgLen < sizeof(serialMsgBuf) - 1) {
        serialMsgBuf[serialMsgLen++] = c;
      }
    }
  } else {
    serialMsgLen = 0;
  }

  if (IrReceiver.decode()) {
	uint16_t addr = IrReceiver.decodedIRData.address;
	uint8_t  cmd  = IrReceiver.decodedIRData.command;
	IrReceiver.resume();

	if (addr == IR_ADDR) {
	  bool sameCode = (cmd == lastIrCode) && (millis() - lastIrTime < 350);
	  if (!sameCode) {
		lastIrCode = cmd;
		lastIrTime = millis();

		// Any button press wakes LCD if it was off
		if (!lcdOn && cmd != CMD_POWER) {
		  lcdOn = true;
		  lcd.backlight();
		  if (inSettings) {
			showSettingsLcd();
		  } else {
			lastDisplayedMode = -1;  // force redraw without changing mode
		  }
		  Serial.println(F("LCD wake"));
		  return;
		}

		if (cmd == CMD_EQ) {
		  if (millis() - lastEqToggleTime < 600) {
			return;
		  }
		  lastEqToggleTime = millis();
		  if (inSettings) {
			inSettings = false;
			setMode(currentMode);
			Serial.println(F("Settings exit"));
		  } else if (currentMode == MODE_IDLE ||
					 currentMode == MODE_TEMPERATURE ||
					 currentMode == MODE_UV_INDEX ||
					 currentMode == MODE_TRACK ||
					 currentMode == MODE_MANUAL_TEST ||
					 currentMode == MODE_SERIAL_MSG) {
			Serial.println(F("EQ not available"));
		  } else {
			inSettings = true;
			// Sound Bar: param 0 = soundCeiling; others: param 0 = brightValue
			settingsParam = 0;
			showSettingsLcd();
			Serial.println(F("Settings menu"));
		  }
		} else if (inSettings && cmd == CMD_UP) {
		  if (currentMode == MODE_SOUND_BAR && settingsParam == 0) {
			soundCeiling = min(soundCeiling + 25, 1023);
		  } else if (settingsParam == 0) { brightValue = min(brightValue + 10, 1023); }
		  else                           { darkValue   = min(darkValue   + 10, 1023); }
		  showSettingsLcd();
		  Serial.print(F("Bright:")); Serial.print(brightValue);
		  Serial.print(F(" Dark:")); Serial.print(darkValue);
		  Serial.print(F(" SndCeil:")); Serial.println(soundCeiling);
		} else if (inSettings && cmd == CMD_DOWN) {
		  if (currentMode == MODE_SOUND_BAR && settingsParam == 0) {
			soundCeiling = max(soundCeiling - 25, 25);
		  } else if (settingsParam == 0) { brightValue = max(brightValue - 10, 0); }
		  else                           { darkValue   = max(darkValue   - 10, 0); }
		  showSettingsLcd();
		  Serial.print(F("Bright:")); Serial.print(brightValue);
		  Serial.print(F(" Dark:")); Serial.print(darkValue);
		  Serial.print(F(" SndCeil:")); Serial.println(soundCeiling);
		} else if (inSettings && currentMode == MODE_DICE && cmd == CMD_UP) {
		  diceNextPreset();
		  showDiceLcd();
		  Serial.print(F("Dice preset: d")); Serial.println(diceSides);
		} else if (inSettings && currentMode == MODE_DICE && cmd == CMD_DOWN) {
		  dicePrevPreset();
		  showDiceLcd();
		  Serial.print(F("Dice preset: d")); Serial.println(diceSides);
		} else if (inSettings && currentMode == MODE_DICE && cmd == CMD_EQ) {
		  // exit dice preset selector
		  inSettings = false;
		  showDiceLcd();
		  Serial.println(F("Dice preset exit"));
		} else if (inSettings && cmd == CMD_FORWARD) {
		  settingsParam = 1; showSettingsLcd();
		} else if (inSettings && cmd == CMD_REVERSE) {
		  settingsParam = 0; showSettingsLcd();
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_UP) {
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; diceNumEntry = false; }
		  diceSides = constrain(diceSides + 1, 2, 1000);
		  showDiceLcd();
		  Serial.print(F("Dice sides: ")); Serial.println(diceSides);
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_DOWN) {
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; diceNumEntry = false; }
		  diceSides = constrain(diceSides - 1, 2, 1000);
		  showDiceLcd();
		  Serial.print(F("Dice sides: ")); Serial.println(diceSides);
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_FORWARD) {
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; diceNumEntry = false; diceNumLen = 0; diceNumBuf[0] = '\0'; }
		  diceNextPreset();
		  showDiceLcd();
		  Serial.print(F("Dice preset: d")); Serial.println(diceSides);
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_REVERSE) {
		  if (diceNumEntry && diceNumLen > 0) {
			// Backspace
			diceNumBuf[--diceNumLen] = '\0';
			if (diceNumLen == 0) diceNumEntry = false;
			showDiceLcd();
		  } else {
			if (diceState != DICE_SELECT) { diceState = DICE_SELECT; }
			dicePrevPreset();
			showDiceLcd();
			Serial.print(F("Dice preset: d")); Serial.println(diceSides);
		  }
		} else if (currentMode == MODE_DICE && !inSettings &&
				   (cmd == CMD_0 || (cmd >= CMD_1 && cmd <= CMD_9))) {
		  // Number key — start or continue typing a custom die size
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; diceNumLen = 0; diceNumBuf[0] = '\0'; }
		  if (!diceNumEntry) { diceNumEntry = true; diceNumLen = 0; diceNumBuf[0] = '\0'; }
		  if (diceNumLen < 4) {
			uint8_t digit = 0;
			switch (cmd) {
			  case CMD_0: digit=0; break; case CMD_1: digit=1; break; case CMD_2: digit=2; break;
			  case CMD_3: digit=3; break; case CMD_4: digit=4; break; case CMD_5: digit=5; break;
			  case CMD_6: digit=6; break; case CMD_7: digit=7; break; case CMD_8: digit=8; break;
			  case CMD_9: digit=9; break;
			}
			diceNumBuf[diceNumLen++] = '0' + digit;
			diceNumBuf[diceNumLen]   = '\0';
			showDiceLcd();
		  }
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_PLAY) {
		  // Confirm any typed number, then roll
		  if (diceNumEntry && diceNumLen > 0) {
			uint16_t newSides = (uint16_t)constrain((int)atoi(diceNumBuf), 2, 1000);
			diceSides    = newSides;
			diceNumEntry = false;
			diceNumLen   = 0;
			diceNumBuf[0] = '\0';
		  }
		  diceState     = DICE_ROLLING;
		  diceRollStart = millis();
		  showDiceLcd();
		  Serial.print(F("Roll d")); Serial.println(diceSides);
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_FUNC) {
		  // Keep 0 as numeric entry in Dice; use FUNC as quick exit to menu
		  if (!lcdOn) { lcdOn = true; lcd.backlight(); }
		  holdDisplay = false;
		  ledsOff = false;
		  setMode(MODE_IDLE);
		  Serial.println(F("FUNC -> Menu"));
		} else if (!inSettings && cmd == CMD_FUNC) {
		  if (currentMode == MODE_DICE) {
			// Dice uses FUNC for quick menu exit so 0 can stay numeric
			if (!lcdOn) { lcdOn = true; lcd.backlight(); }
			holdDisplay = false;
			ledsOff = false;
			setMode(MODE_IDLE);
			Serial.println(F("FUNC -> Menu"));
		  } else {
			ledsOff = !ledsOff;
			if (ledsOff) {
			  allOff();
			} else {
			  int pir = digitalRead(pirPin);
			  applyMode(pir, smoothedLightValue, analogRead(soundPin));
			  lastDisplayedMode = -1;
			}
			Serial.println(ledsOff ? F("LEDs OFF") : F("LEDs ON"));
		  }
		} else if (!inSettings && cmd == CMD_0) {
		  if (currentMode == MODE_DICE) {
			// 0 is digit entry in Dice mode
		  } else {
			Serial.println(F("0 unused"));
		  }
		} else if (cmd == CMD_POWER) {
		  lcdOn = !lcdOn;
		  if (lcdOn) {
			lcd.backlight();
			if (inSettings) {
			  showSettingsLcd();
			} else {
			  lastDisplayedMode = -1;  // force redraw of current mode
			}
			Serial.println(F("LCD on"));
		  } else {
			lcd.noBacklight();
			lcd.clear();
			Serial.println(F("LCD off"));
		  }
		} else if (!inSettings && cmd == CMD_FORWARD) {
		  if (currentMode == MODE_TEMPERATURE) {
			// Cycle unit pair forward: C/F -> K/C -> K/R -> R/F -> C/F
			tempUnitPair = (TempUnitPair)(((int)tempUnitPair + 1) % 4);
			lastDisplayedTempC = -999;  // force LCD refresh
			lastDisplayedPair  = (TempUnitPair)(-1);
			Serial.print(F("Temp pair >> : "));
			Serial.println(getTempPairLabel(tempUnitPair));
		  } else if (currentMode == MODE_MANUAL_TEST) {
			testEffect = (TestEffect)(((int)testEffect + 1) % 3);
			showManualTestLcd();
			Serial.println(F("LED FX next"));
		  } else {
			Serial.println(F("FWD ignored"));
		  }
		} else if (!inSettings && cmd == CMD_REVERSE) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_SERIAL_MSG);
			Serial.println(F("REV -> Serial Msg"));
		  } else if (currentMode == MODE_TEMPERATURE) {
			// Cycle unit pair backward: C/F -> R/F -> K/R -> K/C -> C/F
			tempUnitPair = (TempUnitPair)(((int)tempUnitPair + 3) % 4);
			lastDisplayedTempC = -999;  // force LCD refresh
			lastDisplayedPair  = (TempUnitPair)(-1);
			Serial.print(F("Temp pair << : "));
			Serial.println(getTempPairLabel(tempUnitPair));
		  } else if (currentMode == MODE_MANUAL_TEST) {
			testEffect = (TestEffect)(((int)testEffect + 2) % 3);
			showManualTestLcd();
			Serial.println(F("LED FX prev"));
		  } else {
			Serial.println(F("REV ignored"));
		  }
		} else if (!inSettings && isMappedCmd(cmd)) {
		  ProgramMode selectedMode = getModeFromCmd(cmd);
		  setMode(selectedMode);
		  Serial.print(F("Mode: "));
		  Serial.println(getModeLabel(currentMode));
		} else if (!inSettings && cmd == CMD_STREPT) {
		  holdDisplay = !holdDisplay;
		  if (!holdDisplay) lastDisplayedMode = -1;
		  Serial.println(holdDisplay ? F("Hold ON") : F("Hold OFF"));
		} else if (inSettings && cmd == CMD_PLAY) {
		  // PLAY is a no-op in settings -- just redraw so the screen can't go blank
		  showSettingsLcd();
		  Serial.println(F("PLAY ignored (settings)"));
		} else if (currentMode == MODE_TRACK && !inSettings && cmd == CMD_PLAY) {
		  trackRunning = !trackRunning;
		  if (trackRunning) {
			trackSampleNum = 0;
			trackLastSample = millis();
			// Print CSV header
			Serial.println(F("sample,ms,ldr,sound,pir,tempC"));
			Serial.println(F("---TRACK START---"));
		  } else {
			Serial.println(F("---TRACK STOP---"));
		  }
		  showTrackLcd();
		} else if (currentMode == MODE_MANUAL_TEST && !inSettings && cmd == CMD_PLAY) {
		  testAnimRun = !testAnimRun;
		  if (testAnimRun) testStepAt = millis();
		  showManualTestLcd();
		  Serial.println(testAnimRun ? F("LED FX run") : F("LED FX stop"));
		} else if (currentMode == MODE_MANUAL_TEST && !inSettings && cmd == CMD_UP) {
		  testBarCount = (uint8_t)((testBarCount + 1) % 10);
		  testLedMask = (testBarCount == 0) ? 0 : (uint16_t)((1UL << testBarCount) - 1);
		  showManualTestLcd();
		} else if (currentMode == MODE_MANUAL_TEST && !inSettings && cmd == CMD_DOWN) {
		  testBarCount = (uint8_t)((testBarCount == 0) ? 9 : (testBarCount - 1));
		  testLedMask = (testBarCount == 0) ? 0 : (uint16_t)((1UL << testBarCount) - 1);
		  showManualTestLcd();
		} else if (currentMode == MODE_MANUAL_TEST && !inSettings && (cmd >= CMD_1 && cmd <= CMD_9)) {
		  uint8_t idx = 0;
		  switch (cmd) {
			case CMD_1: idx=0; break; case CMD_2: idx=1; break; case CMD_3: idx=2; break;
			case CMD_4: idx=3; break; case CMD_5: idx=4; break; case CMD_6: idx=5; break;
			case CMD_7: idx=6; break; case CMD_8: idx=7; break; case CMD_9: idx=8; break;
		  }
		  testLedMask ^= (uint16_t)(1U << idx);
		  uint8_t count = 0;
		  for (int i = 0; i < 9; i++) if ((testLedMask >> i) & 0x01) count++;
		  testBarCount = count;
		  showManualTestLcd();
		} else if (currentMode == MODE_MANUAL_TEST && !inSettings && cmd == CMD_0) {
		  testLedMask = 0;
		  testBarCount = 0;
		  showManualTestLcd();
		} else if (currentMode == MODE_SERIAL_MSG && !inSettings && cmd == CMD_PLAY) {
		  serialMsgLine1[0] = '\0';
		  serialMsgLine2[0] = '\0';
		  strncpy(serialMsgLine1, "Serial Msg Mode", 16); serialMsgLine1[16] = '\0';
		  strncpy(serialMsgLine2, "Send: L1|L2", 16); serialMsgLine2[16] = '\0';
		  serialMsgLen = 0;
		  showSerialMsgLcd();
		  Serial.println(F("MSG cleared"));
		} else if (!inSettings && cmd == CMD_PLAY) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_DICE);
			Serial.println(F("PLAY -> Dice"));
		  } else {
			ledsOff = false;
			holdDisplay = false;
			setMode(currentMode);
			Serial.println(F("Mode reset"));
		  }
		} else if (!inSettings && cmd == CMD_VOLUP) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_TRACK);
			Serial.println(F("VOL+ -> Track"));
		  } else if (currentMode == MODE_SOUND_BAR) {
			soundCeiling = min(soundCeiling + 25, 1023);
			Serial.print(F("SndCeil: ")); Serial.println(soundCeiling);
		  } else {
			Serial.println(F("VOL+ ignored"));
		  }
		} else if (!inSettings && cmd == CMD_VOLDN) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_MANUAL_TEST);
			Serial.println(F("VOL- -> LED FX"));
		  } else if (currentMode == MODE_SOUND_BAR) {
			soundCeiling = max(soundCeiling - 25, 25);
			Serial.print(F("SndCeil: ")); Serial.println(soundCeiling);
		  } else {
			Serial.println(F("VOL- ignored"));
		  }
		} else if (inSettings && cmd == CMD_FUNC) {
		  brightValue  = DEFAULT_BRIGHT;
		  darkValue    = DEFAULT_DARK;
		  soundCeiling = DEFAULT_SOUND_CEIL;
		  lcd.clear();
		  lcd.setCursor(0, 0);
		  lcd.print(F("Defaults set!"));
		  lcd.setCursor(0, 1);
		  lcd.print(F("EQ to exit"));
		  delay(1200);
		  showSettingsLcd();
		  Serial.println(F("Settings defaults reset"));
		} else {
		  Serial.print(F("Ignored CMD: 0x"));
		  Serial.println(cmd, HEX);
		}
	  }
		}
	  }

	if (!inSettings) {
	  applyMode(pirState, smoothedLightValue, soundValue);

	  if (lcdOn && !holdDisplay) {
		showModeOnLcd(smoothedLightValue, pirState, soundValue);
	  }
	} else if (lcdOn) {
	  // Keep EQ help/value pages cycling while settings menu is open
	  showSettingsLcd();
	}

  delay(150);
}
