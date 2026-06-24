/*
 * FULL OPTIMIZED VERSION
 * TINKERCAD LCD (0x20) + IR REMOTE (21-button)
 * Modes 1–8 with PIR, LDR, LM35, LED bar, LCD, IR control
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

// LCD @ 0x20 for Tinkercad
LiquidCrystal_I2C lcd(0x20, 16, 2);

// IR receiver on D2
const int IR_PIN = 2;
IRrecv irrecv(IR_PIN);
decode_results results;

// IR codes for 21-button remote (NEC format)
#define IR_1       0xFF30CF
#define IR_2       0xFF18E7
#define IR_3       0xFF7A85
#define IR_4       0xFF10EF
#define IR_5       0xFF38C7
#define IR_6       0xFF5AA5
#define IR_7       0xFF42BD
#define IR_8       0xFF4AB5
#define IR_9       0xFF52AD
#define IR_0       0xFF6897

#define IR_VOLUP   0xFF629D
#define IR_VOLDOWN 0xFFA857
#define IR_OK      0xFF02FD
#define IR_PLAY    0xFF22DD

// Pins
const int btnMode = 3;      // Manual mode button
const int pirPin  = 4;
const int ldrPin  = A0;
const int tempPin = A2;

// LED bar (D3 is last red LED)
const uint8_t ledPins[9] = {5,6,7,8,9,10,11,12,3};

// Modes
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

Mode currentMode = MODE_1_ROOM_LIGHT;

// State
int brightness = 0;
bool motionDetected = false;
unsigned long lastMotionTime = 0;
unsigned long wakeStartTime = 0;
unsigned long lastHelloSwitch = 0;
unsigned long lastModeChange = 0;

float currentTemp = 0;
float tempSmoothed = 0;

bool helloFast = false;

// Constants
const int LDR_DARK = 100;
const int LDR_BRIGHT = 800;
const unsigned long MOTION_TIMEOUT = 15000;
const unsigned long WAKE_DURATION = 10000;

// Hello mode
const char* greetings[] = {"Hello", "Hola", "Bonjour", "Hallo", "Ciao", "Ni Hao"};
const char* languages[] = {"EN", "ES", "FR", "DE", "IT", "ZH"};
int helloIndex = 0;

// ================= SETUP =================
void setup() {
  Serial.begin(9600);

  lcd.init();
  delay(100);
  lcd.backlight();
  lcd.print("Initializing...");

  pinMode(btnMode, INPUT_PULLUP);
  pinMode(pirPin, INPUT);

  for (int i = 0; i < 9; i++) {
	pinMode(ledPins[i], OUTPUT);
	digitalWrite(ledPins[i], LOW);
  }

  irrecv.enableIRIn();

  delay(800);
  lcd.clear();
  displayModeInfo();

  Serial.println("System ready - IR receiver active");
}

// ================= LOOP =================
void loop() {
  handleIR();
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

// ================= IR HANDLING =================
void handleIR() {
  if (!irrecv.decode(&results)) return;

  unsigned long code = results.value;

  // Debug output
  Serial.print("IR Code: 0x");
  Serial.println(code, HEX);

  // Mode selection (buttons 1-8)
  if (code == IR_1) { setMode(0); Serial.println("Mode 1"); }
  else if (code == IR_2) { setMode(1); Serial.println("Mode 2"); }
  else if (code == IR_3) { setMode(2); Serial.println("Mode 3"); }
  else if (code == IR_4) { setMode(3); Serial.println("Mode 4"); }
  else if (code == IR_5) { setMode(4); Serial.println("Mode 5"); }
  else if (code == IR_6) { setMode(5); Serial.println("Mode 6"); }
  else if (code == IR_7) { setMode(6); Serial.println("Mode 7"); }
  else if (code == IR_8) { setMode(7); Serial.println("Mode 8"); }

  // Brightness control
  else if (code == IR_VOLUP) {
	brightness = min(brightness + 1, 9);
	Serial.print("Brightness UP: ");
	Serial.println(brightness);
  }
  else if (code == IR_VOLDOWN) {
	brightness = max(brightness - 1, 0);
	Serial.print("Brightness DOWN: ");
	Serial.println(brightness);
  }

  // Sunrise reset
  else if (code == IR_OK) {
	wakeStartTime = millis();
	Serial.println("Sunrise reset");
  }

  // Hello mode pulse toggle
  else if (code == IR_PLAY) {
	helloFast = !helloFast;
	Serial.print("Hello pulse: ");
	Serial.println(helloFast ? "FAST" : "SLOW");
  }

  irrecv.resume();
}

// ================= BUTTON HANDLING =================
void handleButtons() {
  if (digitalRead(btnMode) == LOW && millis() - lastModeChange > 300) {
	lastModeChange = millis();
	delay(50);
	setMode((currentMode + 1) % 8);
	Serial.println("Mode button pressed");
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

  displayBrightnessBar();
}

void mode2() {
  if (motionDetected) brightness = 9;
  else {
	unsigned long e = millis() - lastMotionTime;
	if (e > MOTION_TIMEOUT) brightness = 0;
	else brightness = 9 - (e * 9 / MOTION_TIMEOUT);
  }
  displayBrightnessBar();
}

void mode3() {
  int light = analogRead(ldrPin);
  brightness = (light < LDR_DARK + 100) ? 8 : 0;
  displayBrightnessBar();
}

void mode4() {
  int light = analogRead(ldrPin);
  int base = map(light, LDR_DARK, LDR_BRIGHT, 0, 5);
  if (motionDetected) base = map(light, LDR_DARK, LDR_BRIGHT, 4, 9);
  if (!motionDetected && millis() - lastMotionTime > MOTION_TIMEOUT) base = 0;
  brightness = constrain(base, 0, 9);
  displayBrightnessBar();
}

void mode5() {
  unsigned long e = millis() - wakeStartTime;
  if (e < WAKE_DURATION) brightness = (e * 9) / WAKE_DURATION;
  else brightness = 9;
  displayBrightnessBar();
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
  } else brightness = motionDetected ? 4 : 0;

  displayBrightnessBar();
}

void mode7() {
  brightness = constrain((tempSmoothed / 50.0) * 9, 0, 9);
  displayBrightnessBar();
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

  displayBrightnessBar();
}

// ================= DISPLAY =================
void displayBrightnessBar() {
  for (int i = 0; i < 9; i++)
	digitalWrite(ledPins[i], i < brightness);

  lcd.setCursor(0, 0);
  lcd.print("Brightness:");
  lcd.print(brightness);
  lcd.print("   ");

  lcd.setCursor(0, 1);
  for (int i = 0; i < 16; i++)
	lcd.write(i < brightness ? 255 : ' ');
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
  lcd.print("IR Ready");
}
