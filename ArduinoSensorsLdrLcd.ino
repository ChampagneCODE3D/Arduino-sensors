#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------------------------
// PIN SETUP
// -------------------------

// LED order: Green (5,6,7), Yellow (8,9,10), Red (11,12,13)
int ledPins[9] = {5, 6, 7, 8, 9, 10, 11, 12, 13};

const int pirPin = 4;
const int ldrPin = A0;

// Try 0x3F here if your LCD stays blank at 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Your real LDR calibration values
int darkValue = 77;      // measured in darkness
int brightValue = 1002;  // measured with light right on sensor

int smoothedLightValue = 0;
int lastDisplayedValue = -1;
int lastDisplayedPresence = -1;

// PIR hold timer (so it doesn't turn off instantly)
unsigned long lastMotionTime = 0;
unsigned long holdTime = 3000; // 3 seconds hold after motion

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
  lcd.print("LDR: ");
  lcd.setCursor(0, 1);
  lcd.print("Room: Vacant");
}

// -------------------------
// MAIN LOOP
// -------------------------
void loop() {

  int pirState = digitalRead(pirPin);
  int lightValue = analogRead(ldrPin);

  smoothedLightValue = (smoothedLightValue * 4 + lightValue) / 5;

  Serial.print("PIR: ");
  Serial.print(pirState);
  Serial.print("   LDR: ");
  Serial.print(lightValue);
  Serial.print("   Smooth: ");
  Serial.println(smoothedLightValue);

  if (abs(smoothedLightValue - lastDisplayedValue) >= 3) {
    lcd.setCursor(0, 0);
    lcd.print("LDR:            ");
    lcd.setCursor(5, 0);
    lcd.print(smoothedLightValue);
    lastDisplayedValue = smoothedLightValue;
  }

  // Reset timer when motion is detected
  if (pirState == HIGH) {
    lastMotionTime = millis();
  }

  int personPresent = (millis() - lastMotionTime < holdTime) ? 1 : 0;

  if (personPresent != lastDisplayedPresence) {
    lcd.setCursor(0, 1);
    if (personPresent == 1) {
      lcd.print("Room: Occupied  ");
    } else {
      lcd.print("Room: Vacant    ");
    }
    lastDisplayedPresence = personPresent;
  }

  // Keep LEDs on for holdTime after last motion
  if (personPresent == 1) {
    updateLightBar(smoothedLightValue);
  } else {
    allOff();
  }

  delay(200);
}

// -------------------------
// GRADUAL EQ-STYLE BAR GRAPH
// -------------------------
void updateLightBar(int lightValue) {

  // Map LDR reading to 0–9 levels
  int level = map(lightValue, darkValue, brightValue, 9, 0);
  level = constrain(level, 0, 9);

  // Light up LEDs based on level
  for (int i = 0; i < 9; i++) {
    if (i < level) {
      digitalWrite(ledPins[i], HIGH);
    } else {
      digitalWrite(ledPins[i], LOW);
    }
  }
}

// -------------------------
// TURN ALL LEDs OFF
// -------------------------
void allOff() {
  for (int i = 0; i < 9; i++) {
    digitalWrite(ledPins[i], LOW);
  }
}
