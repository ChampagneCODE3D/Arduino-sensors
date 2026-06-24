/*
 * MegaGateStation.ino
 * Assignment 2 - Core gate flow scaffold
 */

#include <Servo.h>
#include <LiquidCrystal_I2C.h>

// Placeholder pins - set during wiring
const int trigPin = 22;
const int echoPin = 23;
const int buttonPin = 24;
const int redLedPin = 25;
const int greenLedPin = 26;
const int gateServoPin = 27;

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo gateServo;

enum GateState {
  WAIT_CAR,
  WAIT_TICKET,
  GATE_OPEN,
  RESET_STATE
};

GateState gateState = WAIT_CAR;
unsigned long stateStartMs = 0;

void setLeds(bool redOn, bool greenOn) {
  digitalWrite(redLedPin, redOn ? HIGH : LOW);
  digitalWrite(greenLedPin, greenOn ? HIGH : LOW);
}

void setup() {
  Serial.begin(9600);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);

  gateServo.attach(gateServoPin);
  gateServo.write(0);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Car Park Ready");

  setLeds(false, false);
}

void loop() {
  // TODO: implement required assignment flow state machine
}
