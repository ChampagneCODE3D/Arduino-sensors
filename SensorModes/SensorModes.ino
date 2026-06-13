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

int ledPins[9] = {5, 6, 7, 8, 9, 10, 11, 12, 3};  // Green: D5-D7, Yellow: D8-D10, Red: D11, D12, D3
const int pirPin = 4;
const int ldrPin = A0;
const int irPin = 2;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------------
// STATE
// -------------------------

ProgramMode currentMode = MODE_IDLE;
int darkValue = 77;
int brightValue = 800;
int smoothedLightValue = 0;
int lastBarLevel = -1;
int lastRawLightValue = -1;
int lastDisplayedLight = -1;
int lastDisplayedMode = -1;
int lastDisplayedPresence = -1;
int lastMenuPage = -1;
unsigned long lastIrTime = 0;
uint8_t lastIrCode = 0xFF;
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
int lastDisplayedUV = -1;  // For Mode 9 LCD refresh
bool lcdOn = true;         // LCD backlight/display toggle
bool inSettings = false;   // EQ settings menu active
int settingsParam = 0;     // 0 = brightValue, 1 = darkValue
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
  count = constrain(count, 0, 9);
  for (int i = 0; i < 9; i++) {
	digitalWrite(ledPins[i], (i < count) ? HIGH : LOW);
  }
}

void greenOnly() {
  allOff();
  digitalWrite(ledPins[0], HIGH);  // Pin 5 = Green
  digitalWrite(ledPins[1], HIGH);  // Pin 6 = Green
  digitalWrite(ledPins[2], HIGH);  // Pin 7 = Green
}

// Collapse inward: step 0 = all 9 on, increasing step turns off
// LEDs from both ends moving toward center (index 4).
void collapseToMiddle(int step) {
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

// Map 15-35 deg C to 0-9 LEDs across human comfort range (green -> yellow -> red)
void updateTempLedBar(float tempC) {
  int count = (int)((tempC - 15.0f) / 20.0f * 9.0f);
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

  // Line 1: e.g. "25.3 deg C  77.5 deg F"
  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  lcd.print(v1, 1); lcd.write(0xDF); lcd.print(u1);
  lcd.print(F("  "));
  lcd.print(v2, 1); lcd.write(0xDF); lcd.print(u2);

  // Line 2: e.g. "Temp  C/F"
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("Temp  "));
  lcd.print(getTempPairLabel(tempUnitPair));
}

void showSettingsLcd() {
  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);
  if (settingsParam == 0) {
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

void showIdleMenu() {
  int page = (millis() / 2500) % 6;
  if (page == lastMenuPage) {
	return;
  }

  lastMenuPage = page;
  lcd.clear();

  switch (page) {
	case 0:
	  lcd.setCursor(0, 0);
	  lcd.print("0 Menu");
	  lcd.setCursor(0, 1);
	  lcd.print("1 Room Light");
	  break;
	case 1:
	  lcd.setCursor(0, 0);
	  lcd.print("2 Hallway");
	  lcd.setCursor(0, 1);
	  lcd.print("3 Streetlight");
	  break;
	case 2:
	  lcd.setCursor(0, 0);
	  lcd.print("4 Energy Save");
	  lcd.setCursor(0, 1);
	  lcd.print("5 Smart Home");
	  break;
	case 3:
	  lcd.setCursor(0, 0);
	  lcd.print("6 Warning Light");
	  lcd.setCursor(0, 1);
	  lcd.print("7 Temperature");
	  break;
	case 4:
	  lcd.setCursor(0, 0);
	  lcd.print("8 Sound Bar");
	  lcd.setCursor(0, 1);
	  lcd.print("9 UV Index");
	  break;
	case 5:
	  lcd.setCursor(0, 0);
	  lcd.print("Power ON/OFF");
	  lcd.setCursor(0, 1);
	  lcd.print("LCD screen");
	  break;
  }
}



void showModeOnLcd(int lightValue, int pirState, int soundValue) {
  lcd.backlight();

  if (currentMode == MODE_IDLE) {
	showIdleMenu();
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
	  lcd.print(getModeCornerLabel(currentMode));
	  int bars = map(raw, 0, 250, 0, 16);
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
	  lcd.print(F("Level:"));
	  lcd.print(raw);
	  lcd.setCursor(10, 0);
	  lcd.print(getModeCornerLabel(currentMode));
	  int bars = map(raw, 0, 1023, 0, 16);
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
	lcd.print("LDR:");
	lcd.print("      ");
	lcd.setCursor(4, 0);
	lcd.print(lightValue);
	lcd.setCursor(10, 0);
	lcd.print("      ");
	lcd.setCursor(10, 0);
	lcd.print(getModeCornerLabel(currentMode));
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
	lcd.print("                ");
	lcd.setCursor(0, 1);

	if (currentPresence) {
	  lcd.print("Occupied");
	} else if (showCountdown && secondsRemaining > 0) {
	  lcd.print("Vacant in ");
	  lcd.print(secondsRemaining);
	  lcd.print("s");
	} else {
	  lcd.print("Vacant");
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

  // Mode 9: Sound bar - peak-hold LED bar responding to mic level
  if (currentMode == MODE_SOUND_BAR) {
    int count = map(soundValue, 0, 250, 0, 9);
    count = constrain(count, 1, 9);
    setLedCount(count);
    return;
  }

  // Mode 9: UV index bar - GUVA-S12SD on A3
  if (currentMode == MODE_UV_INDEX) {
    int raw = analogRead(uvPin);
    int count = map(raw, 0, 1023, 0, 9);
    count = constrain(count, 0, 9);
    setLedCount(count);
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
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select 1-7");
  lcd.setCursor(0, 1);
  lcd.print("Press button");

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

  if (inSettings) {
    // IR handling still runs below; skip sensor/display updates
    // fall through to IR section at end of loop
  } else {
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
		  setMode(MODE_IDLE);
		  Serial.println(F("LCD wake"));
		  return;
		}

		if (cmd == CMD_EQ) {
		  inSettings = !inSettings;
		  if (inSettings) {
			settingsParam = 0;
			showSettingsLcd();
			Serial.println(F("Settings menu"));
		  } else {
			setMode(currentMode);
			Serial.println(F("Settings exit"));
		  }
		} else if (inSettings && cmd == CMD_UP) {
		  if (settingsParam == 0) { brightValue = min(brightValue + 10, 1023); }
		  else                    { darkValue   = max(darkValue   - 10, 0);    }
		  showSettingsLcd();
		  Serial.print(F("Bright:")); Serial.print(brightValue);
		  Serial.print(F(" Dark:")); Serial.println(darkValue);
		} else if (inSettings && cmd == CMD_DOWN) {
		  if (settingsParam == 0) { brightValue = max(brightValue - 10, 0);    }
		  else                    { darkValue   = min(darkValue   + 10, 1023); }
		  showSettingsLcd();
		  Serial.print(F("Bright:")); Serial.print(brightValue);
		  Serial.print(F(" Dark:")); Serial.println(darkValue);
		} else if (inSettings && cmd == CMD_FORWARD) {
		  settingsParam = 1; showSettingsLcd();
		} else if (inSettings && cmd == CMD_REVERSE) {
		  settingsParam = 0; showSettingsLcd();
		} else if (cmd == CMD_0) {
		  if (!lcdOn) { lcdOn = true; lcd.backlight(); }
		  setMode(MODE_IDLE);
		  Serial.println(F("0 - Menu"));
		} else if (cmd == CMD_POWER) {
		  lcdOn = !lcdOn;
		  if (lcdOn) {
			lcd.backlight();
			setMode(MODE_IDLE);
			Serial.println(F("LCD on - Menu"));
		  } else {
			lcd.noBacklight();
			lcd.clear();
			Serial.println(F("LCD off"));
		  }
		} else if (cmd == CMD_FORWARD) {
		  if (currentMode == MODE_TEMPERATURE) {
			// Cycle unit pair forward: C/F -> K/C -> K/R -> R/F -> C/F
			tempUnitPair = (TempUnitPair)(((int)tempUnitPair + 1) % 4);
			lastDisplayedTempC = -999;  // force LCD refresh
			lastDisplayedPair  = (TempUnitPair)(-1);
			Serial.print(F("Temp pair >> : "));
			Serial.println(getTempPairLabel(tempUnitPair));
		  } else {
			int next = (currentMode == MODE_IDLE) ? MODE_SMART_ROOM_LIGHT
					 : (((int)currentMode % 9) + 1);
			setMode((ProgramMode)next);
			Serial.print(F("Mode >> : "));
			Serial.println(getModeLabel(currentMode));
		  }
		} else if (cmd == CMD_REVERSE) {
		  if (currentMode == MODE_TEMPERATURE) {
			// Cycle unit pair backward: C/F -> R/F -> K/R -> K/C -> C/F
			tempUnitPair = (TempUnitPair)(((int)tempUnitPair + 3) % 4);
			lastDisplayedTempC = -999;  // force LCD refresh
			lastDisplayedPair  = (TempUnitPair)(-1);
			Serial.print(F("Temp pair << : "));
			Serial.println(getTempPairLabel(tempUnitPair));
		  } else {
			int prev = (currentMode == MODE_IDLE || currentMode == MODE_SMART_ROOM_LIGHT)
					 ? MODE_UV_INDEX
					 : ((int)currentMode - 1);
			setMode((ProgramMode)prev);
			Serial.print(F("Mode << : "));
			Serial.println(getModeLabel(currentMode));
		  }
		} else if (isMappedCmd(cmd)) {
		  ProgramMode selectedMode = getModeFromCmd(cmd);
		  setMode(selectedMode);
		  Serial.print(F("Mode: "));
		  Serial.println(getModeLabel(currentMode));
		} else {
		  Serial.print(F("Ignored CMD: 0x"));
		  Serial.println(cmd, HEX);
		}
	  }
		}
	  }
	} // end !inSettings

	if (!inSettings) {
	applyMode(pirState, smoothedLightValue, soundValue);

	if (lcdOn) {
	  showModeOnLcd(smoothedLightValue, pirState, soundValue);
	}
  }

  delay(150);
}
