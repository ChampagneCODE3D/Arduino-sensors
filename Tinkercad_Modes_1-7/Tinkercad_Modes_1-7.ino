/*
 * Arduino Sensor Modes 1-8 (Tinkercad Compatible)
 * Serial Control: 1-8 for modes, +/- for brightness, r for sunrise, h for hello speed
 * LCD @ 0x20, LED bar on pins 5-12 and 3
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD initialization
LiquidCrystal_I2C lcd(0x20, 16, 2);

// Pin definitions
const int btnMode = 3;
const int pirPin  = 4;
const int ldrPin  = A0;
const int tempPin = A2;

// LED bar pins
const uint8_t ledPins[9] = {5, 6, 7, 8, 9, 10, 11, 12, 3};

// Mode enum
enum Mode {
  MODE_1_ROOM_LIGHT,
  MODE_2_HALLWAY,
  MODE_3_STREETLIGHT,
  MODE_4_ECO_LIGHT,
  MODE_5_SMART_HOME,
  MODE_6_WARNING,
  MODE_7_TEMPERATURE,
  MODE_8_HELLO
};

// State
Mode currentMode = MODE_1_ROOM_LIGHT;
Mode lastDisplayedMode = MODE_1_ROOM_LIGHT;
int brightness = 0;
int lastDisplayedBrightness = -1;
bool motionDetected = false;
unsigned long lastMotionTime = 0;
unsigned long wakeStartTime = 0;
unsigned long lastHelloSwitch = 0;

float currentTemp = 0;
float tempSmoothed = 0;
bool helloFast = false;

// Constants
const int LDR_DARK = 100;
const int LDR_BRIGHT = 800;
const unsigned long MOTION_TIMEOUT = 15000;
const unsigned long WAKE_DURATION = 10000;

// Hello strings
const char* greetings[] = {"Hello", "Hola", "Bonjour", "Hallo", "Ciao", "Ni Hao"};
const char* languages[] = {"EN", "ES", "FR", "DE", "IT", "ZH"};
int helloIndex = 0;

// ================= SETUP =================
void setup() {
  Serial.begin(9600);

  // LCD init
  lcd.init();
  delay(100);
  lcd.backlight();
  lcd.print("Init...");

  // Pin setup
  pinMode(btnMode, INPUT_PULLUP);
  pinMode(pirPin, INPUT);

  for (int i = 0; i < 9; i++) {
	pinMode(ledPins[i], OUTPUT);
	digitalWrite(ledPins[i], LOW);
  }

  delay(800);
  lcd.clear();
  displayModeInfo();

  Serial.println("=== Arduino Sensor Modes 1-8 ===");
  Serial.println("Serial Control:");
  Serial.println("  1-8: select mode");
  Serial.println("  +: brightness up");
  Serial.println("  -: brightness down");
  Serial.println("  r: reset sunrise");
  Serial.println("  h: hello speed");
}

// ================= LOOP =================
void loop() {
  handleSerial();
  handleButtons();
  updateSensors();

  switch (currentMode) {
	case MODE_1_ROOM_LIGHT:   mode1(); break;
	case MODE_2_HALLWAY:      mode2(); break;
	case MODE_3_STREETLIGHT:  mode3(); break;
	case MODE_4_ECO_LIGHT:    mode4(); break;
	case MODE_5_SMART_HOME:   mode5(); break;
	case MODE_6_WARNING:      mode6(); break;
	case MODE_7_TEMPERATURE:  mode7(); break;
	case MODE_8_HELLO:        mode8(); break;
  }

  delay(50);
}

// ================= SERIAL CONTROL =================
void handleSerial() {
  if (!Serial.available()) return;

  char c = Serial.read();

  // Mode selection 1-8
  if (c >= '1' && c <= '8') {
	setMode(c - '1');
	Serial.print("Mode -> ");
	Serial.println((int)currentMode + 1);
	return;
  }

  // Brightness
  if (c == '+') {
	brightness = min(brightness + 1, 9);
	Serial.print("Brightness -> ");
	Serial.println(brightness);
  }
  if (c == '-') {
	brightness = max(brightness - 1, 0);
	Serial.print("Brightness -> ");
	Serial.println(brightness);
  }

  // Sunrise reset
  if (c == 'r') {
	wakeStartTime = millis();
	Serial.println("Sunrise reset");
  }

  // Hello speed
  if (c == 'h') {
	helloFast = !helloFast;
	Serial.print("Hello speed -> ");
	Serial.println(helloFast ? "FAST" : "SLOW");
  }
}

// ================= BUTTON HANDLING =================
void handleButtons() {
  if (digitalRead(btnMode) == LOW) {
	delay(200);
	setMode((currentMode + 1) % 8);
	Serial.print("Mode -> ");
	Serial.println((int)currentMode + 1);
  }
}

// ================= SENSOR UPDATE =================
void updateSensors() {
  motionDetected = digitalRead(pirPin);
  if (motionDetected) lastMotionTime = millis();

  int raw = analogRead(tempPin);
  currentTemp = (raw * 5.0 / 1024.0) * 100.0;
  tempSmoothed = tempSmoothed * 0.8 + currentTemp * 0.2;
}

// ================= MODE FUNCTIONS =================
void mode1() {
  int light = analogRead(ldrPin);
  brightness = map(light, LDR_DARK, LDR_BRIGHT, 0, 9);
  brightness = constrain(brightness, 0, 9);

  if (!motionDetected && millis() - lastMotionTime > MOTION_TIMEOUT)
	brightness = 0;

  displayLDR(light);
  updateLEDs();
}

void mode2() {
  int light = analogRead(ldrPin);
  if (motionDetected) {
	brightness = 9;
  } else {
	unsigned long e = millis() - lastMotionTime;
	if (e > MOTION_TIMEOUT) brightness = 0;
	else brightness = 9 - (e * 9 / MOTION_TIMEOUT);
  }
  displayBrightnessBar(light);
}

void mode3() {
  int light = analogRead(ldrPin);
  brightness = (light < LDR_DARK + 100) ? 8 : 0;

  displayLDR(light);
  updateLEDs();
}

void mode4() {
  int light = analogRead(ldrPin);
  int base = map(light, LDR_DARK, LDR_BRIGHT, 0, 5);

  if (motionDetected) base = map(light, LDR_DARK, LDR_BRIGHT, 4, 9);
  if (!motionDetected && millis() - lastMotionTime > MOTION_TIMEOUT) base = 0;

  brightness = constrain(base, 0, 9);

  displayLDR(light);
  updateLEDs();
}

void mode5() {
  unsigned long e = millis() - wakeStartTime;
  if (e < WAKE_DURATION) brightness = (e * 9) / WAKE_DURATION;
  else brightness = 9;

  displayLDR(0);
  updateLEDs();
}

void mode6() {
  int light = analogRead(ldrPin);
  bool dark = light < LDR_DARK + 100;

  if (motionDetected && dark) {
	static unsigned long lastBlink = 0;
	if (millis() - lastBlink > 500) {
	  lastBlink = millis();
	  brightness = (brightness == 9) ? 2 : 9;
	}
  } else {
	brightness = motionDetected ? 4 : 0;
  }

  displayLDR(light);
  updateLEDs();
}

void mode7() {
  brightness = constrain((tempSmoothed / 50.0) * 9, 0, 9);

  displayLDR(0);
  updateLEDs();
}

void mode8() {
  unsigned long now = millis();

  if (now - lastHelloSwitch > 3000) {
	lastHelloSwitch = now;
	helloIndex = (helloIndex + 1) % 6;
  }

  int speed = helloFast ? 150 : 300;
  int phase = (now / speed) % 10;
  brightness = (phase < 5) ? (phase + 4) : (9 - phase);

  displayLDR(0);
  updateLEDs();

  lcd.setCursor(0, 0);
  lcd.print("Hello ");
  lcd.print(languages[helloIndex]);
  lcd.print("   ");
}

// ================= DISPLAY =================
void displayLDR(int rawLDR) {
  lcd.setCursor(0, 1);
  lcd.print("LDR: ");
  if (rawLDR < 100) lcd.print("0");
  if (rawLDR < 1000) lcd.print(" ");
  lcd.print(rawLDR);
  lcd.print("  ");
}

void updateLEDs() {
  for (int i = 0; i < 9; i++)
	digitalWrite(ledPins[i], i < brightness);
}

void setMode(int m) {
  currentMode = (Mode)m;
  lcd.clear();
  displayModeInfo();
}

void displayModeInfo() {
  lcd.setCursor(0, 0);
  lcd.print("Mode ");
  lcd.print((int)currentMode + 1);
  lcd.print(": ");

  switch (currentMode) {
	case MODE_1_ROOM_LIGHT:  lcd.print("Room"); break;
	case MODE_2_HALLWAY:     lcd.print("Hall"); break;
	case MODE_3_STREETLIGHT: lcd.print("Street"); break;
	case MODE_4_ECO_LIGHT:   lcd.print("Eco"); break;
	case MODE_5_SMART_HOME:  lcd.print("Wake"); break;
	case MODE_6_WARNING:     lcd.print("Warn"); break;
	case MODE_7_TEMPERATURE: lcd.print("Temp"); break;
	case MODE_8_HELLO:       lcd.print("Hello"); break;
  }

  lcd.setCursor(0, 1);
  for (int i = 0; i < 9; i++)
	lcd.write(i < brightness ? 255 : ' ');
}
