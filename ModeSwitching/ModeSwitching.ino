#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>
#include "ButtonMap.h"

// -------------------------
// PIN SETUP
// -------------------------

int ledPins[9] = {5, 6, 7, 8, 9, 10, 11, 12, 13};
const int pirPin = 4;
const int ldrPin = A0;
const int irPin = 2;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------------
// STATE
// -------------------------

ProgramMode currentMode = MODE_IDLE;
int darkValue = 77;
int brightValue = 1002;
int smoothedLightValue = 0;
int lastBarLevel = -1;
int lastRawLightValue = -1;
int lastDisplayedLight = -1;
int lastDisplayedMode = -1;
int lastDisplayedPresence = -1;
unsigned long lastMotionTime = 0;
unsigned long holdTime = 3000;

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

const char* getModeDescription(ProgramMode mode) {
  for (size_t i = 0; i < sizeof(MODE_DEFINITIONS) / sizeof(MODE_DEFINITIONS[0]); i++) {
	if (MODE_DEFINITIONS[i].mode == mode) {
	  return MODE_DEFINITIONS[i].modeDescription;
	}
  }
  return "";
}

void showModeOnLcd(int lightValue, int personPresent) {
  if (lightValue != lastDisplayedLight || (int)currentMode != lastDisplayedMode) {
	lcd.setCursor(0, 0);
	lcd.print("LDR:");
	lcd.print("      ");
	lcd.setCursor(4, 0);
	lcd.print(lightValue);
	lastDisplayedLight = lightValue;
	lastDisplayedMode = (int)currentMode;
  }

  if (personPresent != lastDisplayedPresence) {
	lcd.setCursor(0, 1);
	lcd.print("                ");
	lcd.setCursor(0, 1);
	if (personPresent) {
	  lcd.print("Room: Occupied");
	} else {
	  lcd.print("Room: Vacant");
	}
	lastDisplayedPresence = personPresent;
  }
}

void applyMode(int pirState, int lightValue, int personPresent) {
  switch (currentMode) {
	case MODE_IDLE:
	  allOff();
	  break;

	case MODE_SMART_ROOM_LIGHT:
	  if (personPresent) {
		updateLightBar(lightValue);
	  } else {
		allOff();
	  }
	  break;

	case MODE_HALLWAY_LIGHT:
	  if (personPresent) {
		updateLightBar(lightValue);
	  } else {
		allOff();
	  }
	  break;

	case MODE_STREETLIGHT:
	  if (personPresent) {
		updateLightBar(lightValue);
	  } else {
		allOff();
	  }
	  break;

	case MODE_ENERGY_SAVING_ROOM:
	  if (personPresent) {
		updateLightBar(lightValue);
	  } else {
		allOff();
	  }
	  break;

	case MODE_SMART_HOME_LIGHTING:
	  if (personPresent) {
		updateLightBar(lightValue);
	  } else {
		allOff();
	  }
	  break;

	case MODE_NIGHT_WARNING:
	  if (pirState == HIGH) {
		updateLightBar(lightValue);
	  } else {
		allOff();
	  }
	  break;
  }
}

void setMode(ProgramMode mode) {
  currentMode = mode;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select 1-6");
  lcd.setCursor(0, 1);
  lcd.print(getModeLabel(currentMode));
  delay(500);
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

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select 1-6");
  lcd.setCursor(0, 1);
  lcd.print("Press button");

  IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK);
  setMode(MODE_IDLE);
}

// -------------------------
// MAIN LOOP
// -------------------------

void loop() {
  int pirState = digitalRead(pirPin);
  int lightValue = analogRead(ldrPin);
  smoothedLightValue = (smoothedLightValue * 2 + lightValue) / 3;

  if (pirState == HIGH) {
	lastMotionTime = millis();
  }

  int personPresent = (millis() - lastMotionTime < holdTime) ? 1 : 0;

  if (IrReceiver.decode()) {
	if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
	  uint32_t code = IrReceiver.decodedIRData.decodedRawData;
	  ProgramMode selectedMode = getModeFromCode(code);

	  if (selectedMode == MODE_IDLE) {
		setMode(MODE_IDLE);
		Serial.println("Mode set: Idle");
	  } else {
		setMode(selectedMode);
		Serial.print("Mode set: ");
		Serial.println(getModeLabel(currentMode));
	  }
	}

	IrReceiver.resume();
  }

  applyMode(pirState, smoothedLightValue, personPresent);
  showModeOnLcd(smoothedLightValue, personPresent);

  Serial.print("Mode: ");
  Serial.print(getModeLabel(currentMode));
  Serial.print(" PIR: ");
  Serial.print(pirState);
  Serial.print(" LDR: ");
  Serial.print(lightValue);
  Serial.print(" Smooth: ");
  Serial.print(smoothedLightValue);
  Serial.print(" Present: ");
  Serial.println(personPresent);

  delay(150);
}
