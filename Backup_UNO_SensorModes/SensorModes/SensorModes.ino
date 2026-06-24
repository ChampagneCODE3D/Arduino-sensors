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
#include <math.h>
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

// Uno -> Mega telemetry (fixed-buffer, non-blocking)
const unsigned long TELEMETRY_INTERVAL_MS = 500UL;
unsigned long lastTelemetryMs = 0;
char telemetryTempBuf[12];
char telemetryLineBuf[32];

// -------------------------
// DICE ROLLER STATE
// (Mode 10)
// -------------------------
const uint16_t dndPresets[] PROGMEM = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
const uint8_t  dndPresetCount = 19;
uint16_t diceSides  = 20;      // Current die size (2-100)
uint16_t diceResult = 0;       // Last roll result (0 = not yet rolled)
enum DiceState : uint8_t { DICE_SELECT, DICE_ROLLING, DICE_RESULT };
enum CounterBoolSource : uint8_t { CNT_SRC_PIR, CNT_SRC_LDR_HIGH, CNT_SRC_SOUND_HIGH };
DiceState diceState = DICE_SELECT;
unsigned long diceRollStart = 0;
// Dice edit mode removed - always use arrows

// Setter with hard max enforcement
void setDiceSides(uint16_t value) {
  if (value > 100) value = 100;
  if (value < 2) value = 2;
  diceSides = value;
}

// -------------------------
// MANUAL TEST MODE STATE
// (Mode 11)
// -------------------------
uint16_t testLedMask = 0;   // bit 0..8 controls each LED
uint8_t  testBarCount = 0;  // 0..9 for bar mode via Up/Down
enum TestEffect : uint8_t { TEST_SOLID, TEST_SCANNER, TEST_TRAFFIC };
TestEffect testEffect = TEST_SOLID;
bool testAnimRun = false;
unsigned long testStepAt = 0;
uint8_t testKnightPos = 0;
int8_t  testKnightDir = 1;

// Dice crit flash (nat 1 / max roll)
unsigned long diceCritFlashUntil = 0;
uint8_t diceCritFlashPhase = 0;

// -------------------------
// COUNTER MODE STATE
// (Mode 12)
// -------------------------
int counterValue = 0;
bool counterNumEntry = false;
bool counterEntryNegative = false;
char counterBuf[7];      // typed numeric input
uint8_t counterLen = 0;
CounterBoolSource counterSource = CNT_SRC_PIR;
int counterBoolThreshold = 250;
bool counterLastState = false;

// -------------------------
// SERIAL MESSAGE MODE STATE
// (Mode 12)
// -------------------------
char serialMsgLine1[17] = "Serial Msg Mode";
char serialMsgLine2[17] = "1Y 2N 3M 4R";
char serialMsgBuf[40];
uint8_t serialMsgLen = 0;

// -------------------------
// HELLO MODE STATE
// (Mode 14)
// -------------------------
const char helloLangEN[] PROGMEM = "English";
const char helloLangES[] PROGMEM = "Spanish";
const char helloLangFR[] PROGMEM = "French";
const char helloLangAR[] PROGMEM = "Arabic";
const char helloLangAM[] PROGMEM = "Amharic";
const char helloLangTL[] PROGMEM = "Tagalog";

const char helloText0[] PROGMEM  = "Hello";
const char helloText1[] PROGMEM  = "Thanks";
const char helloText2[] PROGMEM  = "Please";
const char helloText3[] PROGMEM  = "Welcome";
const char helloText4[] PROGMEM  = "Bye";
const char helloText5[] PROGMEM  = "Hola/Hello";
const char helloText6[] PROGMEM  = "Gracias/Thanks";
const char helloText7[] PROGMEM  = "Por favor/Please";
const char helloText8[] PROGMEM  = "De nada/Welcome";
const char helloText9[] PROGMEM  = "Adios/Bye";
const char helloText10[] PROGMEM = "Bonjour/Hello";
const char helloText11[] PROGMEM = "Merci/Thanks";
const char helloText12[] PROGMEM = "SVP/Please";
const char helloText13[] PROGMEM = "De rien/Welcome";
const char helloText14[] PROGMEM = "Au revoir/Bye";
const char helloText15[] PROGMEM = "Salaam/Hello";
const char helloText16[] PROGMEM = "Shukran/Thanks";
const char helloText17[] PROGMEM = "Minfadlak/Please";
const char helloText18[] PROGMEM = "Afwan/Welcome";
const char helloText19[] PROGMEM = "Maasalam/Bye";
const char helloText20[] PROGMEM = "Selam/Hello";
const char helloText21[] PROGMEM = "Amesegenallo/Thx";
const char helloText22[] PROGMEM = "Ebakih/Please";
const char helloText23[] PROGMEM = "Enkwan/Welcome";
const char helloText24[] PROGMEM = "Dehna hun/Bye";
const char helloText25[] PROGMEM = "Kumusta/Hello";
const char helloText26[] PROGMEM = "Salamat/Thanks";
const char helloText27[] PROGMEM = "Pakiusap/Please";
const char helloText28[] PROGMEM = "Walang anuman/YC";
const char helloText29[] PROGMEM = "Paalam/Bye";

struct HelloEntry {
  PGM_P lang;
  PGM_P text;
};

const HelloEntry helloEntries[] PROGMEM = {
  {helloLangEN, helloText0},  {helloLangEN, helloText1},  {helloLangEN, helloText2},  {helloLangEN, helloText3},  {helloLangEN, helloText4},
  {helloLangES, helloText5},  {helloLangES, helloText6},  {helloLangES, helloText7},  {helloLangES, helloText8},  {helloLangES, helloText9},
  {helloLangFR, helloText10}, {helloLangFR, helloText11}, {helloLangFR, helloText12}, {helloLangFR, helloText13}, {helloLangFR, helloText14},
  {helloLangAR, helloText15}, {helloLangAR, helloText16}, {helloLangAR, helloText17}, {helloLangAR, helloText18}, {helloLangAR, helloText19},
  {helloLangAM, helloText20}, {helloLangAM, helloText21}, {helloLangAM, helloText22}, {helloLangAM, helloText23}, {helloLangAM, helloText24},
  {helloLangTL, helloText25}, {helloLangTL, helloText26}, {helloLangTL, helloText27}, {helloLangTL, helloText28}, {helloLangTL, helloText29}
};

const uint8_t helloPhraseCount = sizeof(helloEntries) / sizeof(helloEntries[0]);
const uint8_t helloLangCount = 6;
const uint8_t helloWordsPerLang = 5;
uint8_t helloLangIndex = 0;
uint8_t helloWordIndex = 0;
uint8_t helloIndex = 0;
char helloLangBuf[10] = "English";
char helloBuf[17] = "Hello";

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
    if (p > diceSides) { setDiceSides(p); return; }
  }
  setDiceSides(pgm_read_word(&dndPresets[0]));  // wrap to d2
}
void dicePrevPreset() {
  for (int8_t i = dndPresetCount - 1; i >= 0; i--) {
    uint16_t p = pgm_read_word(&dndPresets[i]);
    if (p < diceSides) { setDiceSides(p); return; }
  }
  setDiceSides(pgm_read_word(&dndPresets[dndPresetCount - 1]));  // wrap to d100
}

// Dice edit mode removed - always use arrows to select


const __FlashStringHelper* getCounterSourceLabel(CounterBoolSource s) {
  switch (s) {
    case CNT_SRC_PIR:        return F("PIR");
    case CNT_SRC_LDR_HIGH:   return F("LDR>");
    case CNT_SRC_SOUND_HIGH: return F("SND>");
    default:                 return F("PIR");
  }
}

int getCounterDefaultThreshold(CounterBoolSource s) {
  if (s == CNT_SRC_LDR_HIGH) return 250;
  if (s == CNT_SRC_SOUND_HIGH) return 100;
  return 0;
}

bool readCounterBoolState(int pirState, int lightValue, int soundValue) {
  if (counterSource == CNT_SRC_PIR) return pirState == HIGH;
  if (counterSource == CNT_SRC_LDR_HIGH) return lightValue > counterBoolThreshold;
  return soundValue > counterBoolThreshold;
}

// ----------------------

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
    lcd.print(F("LED Solid "));
    lcd.print(testBarCount);
  } else if (testEffect == TEST_SCANNER) {
    lcd.print(F("LED Scan "));
    lcd.print(testAnimRun ? F("On") : F("Off"));
  } else {
    lcd.print(F("LED Traffic "));
    lcd.print(testAnimRun ? F("On") : F("Off"));
  }

  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  if (testEffect == TEST_SOLID) {
    lcd.print(F("1-9 Toggle U/D"));
  } else {
    lcd.print(F("Play Run Fwd Rev"));
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

int getCounterTypedValue() {
  int v = atoi(counterBuf);
  return counterEntryNegative ? -v : v;
}

void showCounterLcd() {
  static int lastVal = 2147483647;
  static bool lastEntry = false;
  static bool lastNeg = false;
  static char lastBuf[7] = "";
  static int lastThr = -1;
  static CounterBoolSource lastSrc = (CounterBoolSource)255;
  if (counterValue == lastVal && counterNumEntry == lastEntry && counterEntryNegative == lastNeg &&
      strcmp(counterBuf, lastBuf) == 0 && counterBoolThreshold == lastThr && counterSource == lastSrc) return;
  lastVal = counterValue;
  lastEntry = counterNumEntry;
  lastNeg = counterEntryNegative;
  strncpy(lastBuf, counterBuf, sizeof(lastBuf));
  lastThr = counterBoolThreshold;
  lastSrc = counterSource;

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  lcd.print(F("Cnt: "));
  if (counterNumEntry) {
    if (counterEntryNegative) lcd.print(F("-"));
    lcd.print(counterBuf);
    lcd.print(F("_"));
  } else {
    lcd.print(counterValue);
  }

  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("EQ Src "));
  lcd.print(getCounterSourceLabel(counterSource));
  if (counterSource != CNT_SRC_PIR) {
    lcd.print(counterBoolThreshold);
  }
}

void loadHelloPhrase(uint8_t idx) {
  if (idx >= helloPhraseCount) idx = 0;
  helloIndex = idx;
  helloLangIndex = idx / helloWordsPerLang;
  helloWordIndex = idx % helloWordsPerLang;
  PGM_P langPtr = (PGM_P)pgm_read_word(&helloEntries[helloIndex].lang);
  PGM_P textPtr = (PGM_P)pgm_read_word(&helloEntries[helloIndex].text);
  strcpy_P(helloLangBuf, langPtr);
  strcpy_P(helloBuf, textPtr);
}

void loadHelloByLangWord(uint8_t langIdx, uint8_t wordIdx) {
  if (langIdx >= helloLangCount) langIdx = 0;
  if (wordIdx >= helloWordsPerLang) wordIdx = 0;
  uint8_t idx = (uint8_t)(langIdx * helloWordsPerLang + wordIdx);
  loadHelloPhrase(idx);
}

void showHelloLcd() {
  static uint8_t lastIdx = 255;
  if (lastIdx == helloIndex) return;
  lastIdx = helloIndex;

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));

  lcd.setCursor(0, 0);
  lcd.print(helloLangBuf);
  lcd.setCursor(11, 0);
  lcd.print(F("L"));
  lcd.print((int)helloLangIndex + 1);
  lcd.print(F("W"));
  lcd.print((int)helloWordIndex + 1);

  lcd.setCursor(0, 1);
  lcd.print(helloBuf);
}

void serialSendQuickReply(const char* text) {
  Serial.print(F("REPLY: "));
  Serial.println(text);
  strncpy(serialMsgLine2, text, 16);
  serialMsgLine2[16] = '\0';
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

// ---- Dice LCD ----
void showDiceLcd() {
  static DiceState lastDState  = (DiceState)255;
  static uint16_t  lastDResult = 65535;
  static uint16_t  lastDSides  = 65535;
  static bool      lastDNum    = false;

  bool changed = (diceState    != lastDState  ||
                  diceResult   != lastDResult ||
                  diceSides    != lastDSides);
  if (!changed) return;
  lastDState  = diceState;
  lastDResult = diceResult;
  lastDSides  = diceSides;

  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);

  if (diceState == DICE_ROLLING) {
    lcd.print(F("Rolling D"));
    lcd.print(diceSides);
    lcd.print(F("..."));
  } else if (diceState == DICE_RESULT) {
    lcd.print(F("D"));
    lcd.print(diceSides);
    lcd.setCursor(7, 0);
    lcd.print(F("=> "));
    lcd.print(diceResult);
    lcd.setCursor(0, 1);
    lcd.print(F("Play Reroll"));
  } else {
    // DICE_SELECT
    lcd.print(F("> D"));
    lcd.print(diceSides);
    lcd.setCursor(0, 1);
    lcd.print(F("UP/DN  Play"));
  }
}
// ------------------

void showIdleMenu() {
  int page = (millis() / 2500) % 8;
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
	  lcd.print(F("0 Counter"));
	  break;
	case 1:
	  lcd.setCursor(0, 0);
	  lcd.print(F("1 Room Light"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("2 Hallway"));
	  break;
	case 2:
	  lcd.setCursor(0, 0);
	  lcd.print(F("3 Streetlight"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("4 Eco Mode"));
	  break;
	case 3:
	  lcd.setCursor(0, 0);
	  lcd.print(F("5 Smart Home"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("6 Warning Light"));
	  break;
	case 4:
	  lcd.setCursor(0, 0);
	  lcd.print(F("7 Temperature"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("8 Sound Bar"));
	  break;
	case 5:
	  lcd.setCursor(0, 0);
	  lcd.print(F("9 UV Index"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("Play Dice"));
	  break;
	case 6:
	  lcd.setCursor(0, 0);
	  lcd.print(F("Vol+ Manual Test"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("Vol- Serial Msg"));
	  break;
	case 7:
	  lcd.setCursor(0, 0);
	  lcd.print(F("Fwd Counter"));
	  lcd.setCursor(0, 1);
	  lcd.print(F("Rev Hello"));
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

  if (currentMode == MODE_MANUAL_TEST) {
	showManualTestLcd();
	return;
  }

  if (currentMode == MODE_SERIAL_MSG) {
	showSerialMsgLcd();
	return;
  }

  if (currentMode == MODE_COUNTER) {
	showCounterLcd();
	return;
  }

  if (currentMode == MODE_HELLO) {
	showHelloLcd();
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
	  lastPirStateMode3 = pirState;
	  lastStreetStateMode3 = streetState;
	}

	if (pirState == HIGH && lightValue < brightValue) {
	  // Motion detected in dark - turn on
	  streetState = STREET_ON;
	  allOn();
	} else if (streetState == STREET_COLLAPSING) {
	  // Already collapsing - let it finish regardless of light level
	  unsigned long elapsed = millis() - streetCollapseStartTime;
	  if (elapsed >= 10000) {
		streetState = STREET_OFF;
		allOff();
	  } else {
		int step = (int)(elapsed / 2000);
		collapseToMiddle(step);
	  }
	} else if (pirState == LOW && streetState == STREET_ON) {
	  // Motion stopped and we were on - start collapse
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
		wakeUpState = WAKE_RISING;
		wakeUpStartTime = millis();
	  } else {
		allOff();
	  }
	} else if (wakeUpState == WAKE_RISING) {
	  // Rising animation (10 seconds) - continue regardless of light level
	  unsigned long elapsed = millis() - wakeUpStartTime;
	  if (elapsed >= 10000) {
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
		wakeUpState = WAKE_BOUNCING;
		wakeUpStartTime = millis();
	  } else {
		unsigned long elapsed = millis() - wakeUpStartTime;
		if (elapsed >= 10000) {
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
		wakeUpState = WAKE_ON;
		allOn();
	  } else {
		unsigned long bounceDuration = ledsToAdd * 500;  // 500ms per LED
		unsigned long elapsed = millis() - wakeUpStartTime;
		if (elapsed >= bounceDuration) {
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
	lastPirMode6 = pirState;
	lastLightMode6 = lightValue;
	lastWarnState = warnState;
  }

	// Update motion timer
	if (pirState == HIGH) {
	  warnLastMotionTime = millis();
	  if (warnState == WARN_IDLE || warnState == WARN_COUNTDOWN) {
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
		warnState = WARN_COUNTDOWN;
	  } else if (warnState == WARN_COUNTDOWN && millis() - warnLastMotionTime >= 20000) {
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

  // Mode 11: Manual LED Test
  if (currentMode == MODE_MANUAL_TEST) {
	if (testEffect == TEST_SOLID || !testAnimRun) {
	  for (int i = 0; i < 9; i++) {
		bool on = ((testLedMask >> i) & 0x01) != 0;
		digitalWrite(ledPins[i], on ? HIGH : LOW);
	  }
	} else if (testEffect == TEST_SCANNER && millis() - testStepAt >= 120) {
	  testStepAt = millis();
	  allOff();
	  digitalWrite(ledPins[testKnightPos], HIGH);
	  if (testKnightPos == 8) testKnightDir = -1;
	  else if (testKnightPos == 0) testKnightDir = 1;
	  testKnightPos = (uint8_t)((int)testKnightPos + testKnightDir);
	} else if (testEffect == TEST_TRAFFIC && millis() - testStepAt >= 500) {
	  testStepAt = millis();
	  allOff();
	  uint8_t phase = (uint8_t)(testKnightPos & 0x03);
	  if (phase == 0) {
		digitalWrite(ledPins[6], HIGH); digitalWrite(ledPins[7], HIGH); digitalWrite(ledPins[8], HIGH);
	  } else if (phase == 1) {
		digitalWrite(ledPins[3], HIGH); digitalWrite(ledPins[4], HIGH); digitalWrite(ledPins[5], HIGH);
		digitalWrite(ledPins[6], HIGH); digitalWrite(ledPins[7], HIGH); digitalWrite(ledPins[8], HIGH);
	  } else if (phase == 2) {
		digitalWrite(ledPins[0], HIGH); digitalWrite(ledPins[1], HIGH); digitalWrite(ledPins[2], HIGH);
	  } else {
		digitalWrite(ledPins[3], HIGH); digitalWrite(ledPins[4], HIGH); digitalWrite(ledPins[5], HIGH);
	  }
	  testKnightPos = (uint8_t)((testKnightPos + 1) & 0x03);
	}
	return;
  }

  // Mode 13: Counter auto-edge count from selected bool source
  if (currentMode == MODE_COUNTER) {
	bool nowState = readCounterBoolState(pirState, lightValue, soundValue);
	if (nowState && !counterLastState) {
	  counterValue++;
	}
	counterLastState = nowState;
	digitalWrite(ledPins[0], nowState ? HIGH : LOW);
	for (int i = 1; i < 9; i++) digitalWrite(ledPins[i], LOW);
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
  testLedMask   = 0;
  testBarCount  = 0;
  testEffect    = TEST_SOLID;
  testAnimRun   = false;
  testStepAt    = 0;
  testKnightPos = 0;
  testKnightDir = 1;
  diceCritFlashUntil = 0;
  diceCritFlashPhase = 0;
  counterNumEntry = false;
  counterEntryNegative = false;
  counterLen = 0;
  counterBuf[0] = '\0';
  counterLastState = false;
  helloIndex = 0;
  loadHelloPhrase(0);
  serialMsgLine1[0] = '\0';
  serialMsgLine2[0] = '\0';
  strncpy(serialMsgLine1, "Serial Msg Mode", 16); serialMsgLine1[16] = '\0';
  strncpy(serialMsgLine2, "1Y 2N 3M 4R", 16); serialMsgLine2[16] = '\0';
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
	  uint8_t prevIrCode = lastIrCode;
	  unsigned long prevIrTime = lastIrTime;
	  bool sameCode = (cmd == lastIrCode) && (millis() - lastIrTime < 350);
	  if (!sameCode) {
		lastIrCode = cmd;
		lastIrTime = millis();

		// Any button press wakes LCD if it was off
		if (!lcdOn && cmd != CMD_POWER) {
		  lcdOn = true;
		  lcd.backlight();
		  if (inSettings) {
			if (currentMode == MODE_DICE) showDiceLcd();
			else if (currentMode == MODE_COUNTER) showCounterLcd();
			else if (currentMode == MODE_SERIAL_MSG) showSerialMsgLcd();
			else if (currentMode == MODE_HELLO) showHelloLcd();
		  } else {
			lastDisplayedMode = -1;  // force redraw without changing mode
		  }
		  Serial.println(F("LCD wake"));
		  return;
		}

		if (cmd == CMD_EQ) {
		  // EQ/settings removed on UNO build
		  return;
		} else if (currentMode == MODE_HELLO && !inSettings && cmd == CMD_UP) {
		  loadHelloByLangWord(helloLangIndex, (uint8_t)((helloWordIndex + 1) % helloWordsPerLang));
		  showHelloLcd();
		} else if (currentMode == MODE_HELLO && !inSettings && cmd == CMD_DOWN) {
		  loadHelloByLangWord(helloLangIndex, (uint8_t)((helloWordIndex + helloWordsPerLang - 1) % helloWordsPerLang));
		  showHelloLcd();
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_UP) {
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; }
		  setDiceSides(diceSides + 1);
		  showDiceLcd();
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_DOWN) {
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; }
		  setDiceSides(diceSides - 1);
		  showDiceLcd();
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_FORWARD) {
		  // FORWARD disabled - use UP arrow instead
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; }
		  showDiceLcd();
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_REVERSE) {
		  // REVERSE disabled - use DOWN arrow instead
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; }
		  showDiceLcd();
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_0) {
		  // 0 button disabled - just use arrows to select
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; }
		  showDiceLcd();
		} else if (currentMode == MODE_DICE && !inSettings && (cmd >= CMD_1 && cmd <= CMD_9)) {
		  // Digit commands ignored - use arrows instead
		  if (diceState != DICE_SELECT) { diceState = DICE_SELECT; }
		  showDiceLcd();
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_PLAY) {
		  // Roll the dice
		  diceState     = DICE_ROLLING;
		  diceRollStart = millis();
		  showDiceLcd();
		} else if (currentMode == MODE_DICE && !inSettings && cmd == CMD_FUNC) {
		  // FUNC is menu - exit to idle
		  if (!lcdOn) { lcdOn = true; lcd.backlight(); }
		  holdDisplay = false;
		  ledsOff = false;
		  setMode(MODE_IDLE);
		  Serial.println(F("FUNC -> Menu"));
		} else if (!inSettings && cmd == CMD_FUNC) {
		  if (!lcdOn) { lcdOn = true; lcd.backlight(); }
		  holdDisplay = false;
		  ledsOff = false;
		  setMode(MODE_IDLE);
		  Serial.println(F("FUNC -> Menu"));
		} else if (currentMode == MODE_COUNTER && !inSettings && cmd == CMD_UP) {
		  counterValue++;
		  counterNumEntry = false;
		  counterEntryNegative = false;
		  showCounterLcd();
		} else if (currentMode == MODE_COUNTER && !inSettings && cmd == CMD_DOWN) {
		  if (!counterNumEntry || counterLen == 0) {
			counterValue--;
			counterNumEntry = false;
			counterEntryNegative = false;
		  } else {
			counterEntryNegative = !counterEntryNegative;
		  }
		  showCounterLcd();
		} else if (currentMode == MODE_COUNTER && !inSettings && cmd == CMD_VOLDN) {
		  counterValue = 0;
		  counterNumEntry = false;
		  counterEntryNegative = false;
		  counterLen = 0;
		  counterBuf[0] = '\0';
		  showCounterLcd();
		} else if (currentMode == MODE_COUNTER && !inSettings && (cmd == CMD_0 || (cmd >= CMD_1 && cmd <= CMD_9))) {
		  if (!counterNumEntry) { counterNumEntry = true; counterLen = 0; counterBuf[0] = '\0'; }
		  if (counterLen < sizeof(counterBuf) - 1) {
			char digit = '0';
			switch (cmd) {
			  case CMD_0: digit='0'; break; case CMD_1: digit='1'; break; case CMD_2: digit='2'; break;
			  case CMD_3: digit='3'; break; case CMD_4: digit='4'; break; case CMD_5: digit='5'; break;
			  case CMD_6: digit='6'; break; case CMD_7: digit='7'; break; case CMD_8: digit='8'; break;
			  case CMD_9: digit='9'; break;
			}
			counterBuf[counterLen++] = digit;
			counterBuf[counterLen] = '\0';
			showCounterLcd();
		  }
		} else if (currentMode == MODE_COUNTER && !inSettings && cmd == CMD_PLAY) {
		  if (counterNumEntry && counterLen > 0) {
			counterValue = getCounterTypedValue();
			counterNumEntry = false;
			counterEntryNegative = false;
			counterLen = 0;
			counterBuf[0] = '\0';
			showCounterLcd();
		  }
		} else if (currentMode == MODE_COUNTER && !inSettings && cmd == CMD_FORWARD) {
		  if (counterNumEntry && counterLen > 0) {
			counterValue += getCounterTypedValue();
			counterNumEntry = false;
			counterEntryNegative = false;
			counterLen = 0;
			counterBuf[0] = '\0';
			showCounterLcd();
		  }
		} else if (currentMode == MODE_COUNTER && !inSettings && cmd == CMD_REVERSE) {
		  if (counterNumEntry && counterLen > 0) {
			counterValue -= getCounterTypedValue();
			counterNumEntry = false;
			counterEntryNegative = false;
			counterLen = 0;
			counterBuf[0] = '\0';
			showCounterLcd();
		  }
		} else if (!inSettings && cmd == CMD_0) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_COUNTER);
			Serial.println(F("0 -> Counter"));
		  } else if (currentMode == MODE_DICE) {
			// 0 is digit entry in Dice mode
		  }
		} else if (cmd == CMD_POWER) {
		  if (prevIrCode == CMD_9 && (millis() - prevIrTime) < 900) {
			Serial.println(F("Remote reset"));
			delay(40);
			asm volatile ("jmp 0");
		  }
		  lcdOn = !lcdOn;
		  if (lcdOn) {
			lcd.backlight();
			if (inSettings) {
			  if (currentMode == MODE_DICE) showDiceLcd();
			  else if (currentMode == MODE_COUNTER) showCounterLcd();
			  else if (currentMode == MODE_SERIAL_MSG) showSerialMsgLcd();
			  else if (currentMode == MODE_HELLO) showHelloLcd();
			} else {
			  lastDisplayedMode = -1;  // force redraw of current mode
			}
		  } else {
			lcd.noBacklight();
			lcd.clear();
		  }
		} else if (!inSettings && cmd == CMD_FORWARD) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_SERIAL_MSG);
			Serial.println(F("FWD -> Serial Msg"));
		  } else if (currentMode == MODE_TEMPERATURE) {
			// Cycle unit pair forward: C/F -> K/C -> K/R -> R/F -> C/F
			tempUnitPair = (TempUnitPair)(((int)tempUnitPair + 1) % 4);
			lastDisplayedTempC = -999;  // force LCD refresh
			lastDisplayedPair  = (TempUnitPair)(-1);
			Serial.print(F("Temp pair >> : "));
			Serial.println(getTempPairLabel(tempUnitPair));
		  } else if (currentMode == MODE_MANUAL_TEST) {
			testEffect = (TestEffect)(((int)testEffect + 1) % 3);
			if (testEffect == TEST_TRAFFIC) testKnightPos = 0;
			showManualTestLcd();
			Serial.println(F("LED FX next"));
		  } else if (currentMode == MODE_HELLO) {
			loadHelloByLangWord((uint8_t)((helloLangIndex + 1) % helloLangCount), helloWordIndex);
			showHelloLcd();
		  }
		} else if (!inSettings && cmd == CMD_REVERSE) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_HELLO);
			Serial.println(F("REV -> Hello"));
		  } else if (currentMode == MODE_TEMPERATURE) {
			// Cycle unit pair backward: C/F -> R/F -> K/R -> K/C -> C/F
			tempUnitPair = (TempUnitPair)(((int)tempUnitPair + 3) % 4);
			lastDisplayedTempC = -999;  // force LCD refresh
			lastDisplayedPair  = (TempUnitPair)(-1);
			Serial.print(F("Temp pair << : "));
			Serial.println(getTempPairLabel(tempUnitPair));
		  } else if (currentMode == MODE_MANUAL_TEST) {
			testEffect = (TestEffect)(((int)testEffect + 2) % 3);
			if (testEffect == TEST_TRAFFIC) testKnightPos = 0;
			showManualTestLcd();
			Serial.println(F("LED FX prev"));
		  } else if (currentMode == MODE_HELLO) {
			loadHelloByLangWord((uint8_t)((helloLangIndex + helloLangCount - 1) % helloLangCount), helloWordIndex);
			showHelloLcd();
		  }
		} else if (!inSettings && isMappedCmd(cmd) && currentMode == MODE_IDLE) {
		  ProgramMode selectedMode = getModeFromCmd(cmd);
		  setMode(selectedMode);
		  Serial.print(F("Mode: "));
		  Serial.println(getModeLabel(currentMode));
		} else if (!inSettings && cmd == CMD_STREPT) {
		  holdDisplay = !holdDisplay;
		  if (!holdDisplay) lastDisplayedMode = -1;
		  Serial.println(holdDisplay ? F("Hold ON") : F("Hold OFF"));
		} else if (currentMode == MODE_MANUAL_TEST && !inSettings && cmd == CMD_PLAY) {
		  testAnimRun = !testAnimRun;
		  if (testAnimRun) testStepAt = millis();
		  showManualTestLcd();
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
		} else if (currentMode == MODE_MANUAL_TEST && !inSettings && cmd == CMD_FORWARD) {
		  testEffect = (TestEffect)((testEffect + 1) % 3);
		  testAnimRun = false;
		  testKnightPos = 0;
		  testKnightDir = 1;
		  showManualTestLcd();
		} else if (currentMode == MODE_MANUAL_TEST && !inSettings && cmd == CMD_REVERSE) {
		  testEffect = (TestEffect)((testEffect + 2) % 3);
		  testAnimRun = false;
		  testKnightPos = 0;
		  testKnightDir = 1;
		  showManualTestLcd();
		} else if (currentMode == MODE_SERIAL_MSG && !inSettings && cmd == CMD_1) {
		  serialSendQuickReply("YES");
		  showSerialMsgLcd();
		} else if (currentMode == MODE_SERIAL_MSG && !inSettings && cmd == CMD_2) {
		  serialSendQuickReply("NO");
		  showSerialMsgLcd();
		} else if (currentMode == MODE_SERIAL_MSG && !inSettings && cmd == CMD_3) {
		  serialSendQuickReply("MAYBE");
		  showSerialMsgLcd();
		} else if (currentMode == MODE_SERIAL_MSG && !inSettings && cmd == CMD_4) {
		  serialSendQuickReply("REPEAT");
		  showSerialMsgLcd();
		} else if (currentMode == MODE_SERIAL_MSG && !inSettings && cmd == CMD_PLAY) {
		  serialMsgLine1[0] = '\0';
		  serialMsgLine2[0] = '\0';
		  strncpy(serialMsgLine1, "Serial Msg Mode", 16); serialMsgLine1[16] = '\0';
		  strncpy(serialMsgLine2, "1Y 2N 3M 4R", 16); serialMsgLine2[16] = '\0';
		  serialMsgLen = 0;
		  showSerialMsgLcd();
		} else if (currentMode == MODE_SERIAL_MSG && !inSettings && cmd == CMD_FORWARD) {
		  setMode(MODE_HELLO);
		} else if (currentMode == MODE_SERIAL_MSG && !inSettings && cmd == CMD_REVERSE) {
		  setMode(MODE_HELLO);
		} else if (!inSettings && cmd == CMD_PLAY) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_DICE);
			Serial.println(F("PLAY -> Dice"));
		  } else {
			ledsOff = false;
			holdDisplay = false;
			setMode(currentMode);
		  }
		} else if (!inSettings && cmd == CMD_VOLUP) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_MANUAL_TEST);
		  } else if (currentMode == MODE_SOUND_BAR) {
			soundCeiling = min(soundCeiling + 25, 1023);
			Serial.print(F("SndCeil: ")); Serial.println(soundCeiling);
		  }
		} else if (!inSettings && cmd == CMD_VOLDN) {
		  if (currentMode == MODE_IDLE) {
			setMode(MODE_SERIAL_MSG);
		  } else if (currentMode == MODE_SOUND_BAR) {
			soundCeiling = max(soundCeiling - 25, 25);
			Serial.print(F("SndCeil: ")); Serial.println(soundCeiling);
		  }
		} else if (inSettings && cmd == CMD_FUNC) {
		  if (!lcdOn) { lcdOn = true; lcd.backlight(); }
		  holdDisplay = false;
		  ledsOff = false;
		  inSettings = false;
		  setMode(MODE_IDLE);
		  Serial.println(F("FUNC -> Menu"));
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
	  if (currentMode == MODE_DICE) {
		showDiceLcd();
	  } else if (currentMode == MODE_COUNTER) {
		showCounterLcd();
	  } else if (currentMode == MODE_SERIAL_MSG) {
		showSerialMsgLcd();
	  } else if (currentMode == MODE_HELLO) {
		showHelloLcd();
	  }
	}

  // --- SEND TEMP + LDR TO MEGA (non-blocking, fixed-buffer) ---
  unsigned long nowMs = millis();
  if ((unsigned long)(nowMs - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs += TELEMETRY_INTERVAL_MS;
    if ((unsigned long)(nowMs - lastTelemetryMs) >= TELEMETRY_INTERVAL_MS) {
      lastTelemetryMs = nowMs;
    }

    float tempOut = smoothedTempC;
    if (!isfinite(tempOut)) tempOut = 0.0f;

    dtostrf(tempOut, 0, 1, telemetryTempBuf);  // exactly one decimal
    int n = snprintf(telemetryLineBuf, sizeof(telemetryLineBuf), "TEMP=%s,LDR=%d\n", telemetryTempBuf, smoothedLightValue);
    if (n > 0 && n < (int)sizeof(telemetryLineBuf)) {
      Serial.write((const uint8_t*)telemetryLineBuf, (size_t)n);
    }
  }

  delay(150);
}
