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
enum WarnState { WARN_IDLE, WARN_INTRO, WARN_OUTRO };
WarnState warnState = WARN_IDLE;
int warnLedIndex = 0;
unsigned long warnStepTime = 0;
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
enum WakeUpState { WAKE_OFF, WAKE_RISING };
WakeUpState wakeUpState = WAKE_OFF;
unsigned long wakeUpStartTime = 0;
int lastPirStateMode3 = -1;  // For Mode 3 change detection
int lastStreetStateMode3 = -1;  // For Mode 3 change detection
unsigned long streetLightLastMotionTime = 0;  // For Mode 3 UI countdown (always runs)

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

void showIdleMenu() {
  int page = (millis() / 2500) % 3;
  if (page == lastMenuPage) {
	return;
  }

  lastMenuPage = page;
  lcd.clear();

  switch (page) {
	case 0:
	  lcd.setCursor(0, 0);
	  lcd.print("1 Room Light");
	  lcd.setCursor(0, 1);
	  lcd.print("2 Hallway");
	  break;
	case 1:
	  lcd.setCursor(0, 0);
	  lcd.print("3 Streetlight");
	  lcd.setCursor(0, 1);
	  lcd.print("4 Energy Save");
	  break;
	case 2:
	  lcd.setCursor(0, 0);
	  lcd.print("5 Wake-Up Light");
	  lcd.setCursor(0, 1);
	  lcd.print("6 Night Warn");
	  break;
  }
}



void showModeOnLcd(int lightValue, int pirState) {
  lcd.backlight();

  if (currentMode == MODE_IDLE) {
	showIdleMenu();
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
	} else if (currentMode == MODE_NIGHT_WARNING && (warnState == WARN_INTRO || warnState == WARN_OUTRO)) {
	  // Night warning animation is in progress
	  showCountdown = true;
	  secondsRemaining = 1; // Just show "1s" during animation
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

void applyMode(int pirState, int lightValue) {

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

	case MODE_SMART_HOME_LIGHTING:
	  // Progressive Wake-Up: LEDs gradually turn on over 10 seconds when occupied & dark
	  if (pirState == HIGH && roomDark(lightValue)) {
		if (wakeUpState == WAKE_OFF) {
		  wakeUpState = WAKE_RISING;
		  wakeUpStartTime = millis();
		}
		if (wakeUpState == WAKE_RISING) {
		  unsigned long elapsed = millis() - wakeUpStartTime;
		  if (elapsed >= 10000) {
			// Fully on after 10 seconds - keep state so we don't restart animation
			wakeUpState = WAKE_RISING;  // Stay in WAKE_RISING to prevent restart
			allOn();
		  } else {
			// Gradually turn on LEDs: 1 new LED every ~1.1 seconds
			int ledCount = (int)((elapsed * 9) / 10000);
			setLedCount(ledCount);
		  }
		}
	  } else {
		wakeUpState = WAKE_OFF;
		allOff();
	  }
	  break;

	case MODE_NIGHT_WARNING:
	  if (pirState == HIGH && lightValue < brightValue) {
		if (warnState == WARN_IDLE || warnState == WARN_OUTRO) {
		  allOff();
		  warnLedIndex = -1;
		  warnStepTime = millis();
		  warnState = WARN_INTRO;
		}
	  }

	  if (warnState == WARN_INTRO && millis() - warnStepTime >= 80) {
		warnStepTime = millis();
		warnLedIndex++;
		if (warnLedIndex <= 8) {
		  digitalWrite(ledPins[warnLedIndex], HIGH);
		}
		if (warnLedIndex >= 8) {
		  warnState = WARN_OUTRO;
		  warnLedIndex = 8;
		}
	  } else if (warnState == WARN_OUTRO && millis() - warnStepTime >= 300) {
		warnStepTime = millis();
		digitalWrite(ledPins[warnLedIndex], LOW);
		warnLedIndex--;
		if (warnLedIndex < 0) {
		  warnState = WARN_IDLE;
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

  // SD logging removed to save RAM

  // Always turn off LEDs when switching modes to prevent state leakage
  allOff();

  if (currentMode == MODE_IDLE) {
    lastMenuPage = -1;
    showIdleMenu();
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print(getModeLabel(currentMode));
  lcd.setCursor(0, 1);
  lcd.print(getModeDescription(currentMode));
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

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select 1-6");
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
  smoothedLightValue = (smoothedLightValue * 2 + lightValue) / 3;

  if (IrReceiver.decode()) {
	uint16_t addr = IrReceiver.decodedIRData.address;
	uint8_t  cmd  = IrReceiver.decodedIRData.command;
	IrReceiver.resume();

	if (addr == IR_ADDR) {
	  bool sameCode = (cmd == lastIrCode) && (millis() - lastIrTime < 350);
	  if (!sameCode) {
		lastIrCode = cmd;
		lastIrTime = millis();

		if (cmd == CMD_POWER) {
		  Serial.println(F("POWER pressed - switching to IDLE"));
		  setMode(MODE_IDLE);
		  Serial.println(F("Mode: Idle"));
		} else if (cmd == CMD_FORWARD) {
		  int next = (currentMode == MODE_IDLE) ? MODE_SMART_ROOM_LIGHT
				   : (((int)currentMode % 6) + 1);
		  setMode((ProgramMode)next);
		  Serial.print(F("Mode >> : "));
		  Serial.println(getModeLabel(currentMode));
		} else if (cmd == CMD_REVERSE) {
		  int prev = (currentMode == MODE_IDLE || currentMode == MODE_SMART_ROOM_LIGHT)
				   ? MODE_NIGHT_WARNING
				   : ((int)currentMode - 1);
		  setMode((ProgramMode)prev);
		  Serial.print(F("Mode << : "));
		  Serial.println(getModeLabel(currentMode));
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

  applyMode(pirState, smoothedLightValue);

  showModeOnLcd(smoothedLightValue, pirState);

  // Debug output removed to reduce serial spam - Mode 3 has its own targeted debug

  delay(150);
}
