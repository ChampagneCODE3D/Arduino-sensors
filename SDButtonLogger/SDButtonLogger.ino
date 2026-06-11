#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>

#define IR_RECEIVE_PIN 2
#define SD_CS_PIN 10
#define BUTTON_COUNT 21

LiquidCrystal_I2C lcd(0x27, 16, 2);
File logFile;

const char* buttonNames[BUTTON_COUNT] = {
  "POWER",
  "VOL+",
  "FUNC/STOP",
  "REVERSE",
  "PLAY/PAUSE",
  "FORWARD",
  "DOWN",
  "VOL-",
  "UP",
  "0",
  "EQ",
  "ST/REPT",
  "1",
  "2",
  "3",
  "4",
  "5",
  "6",
  "7",
  "8",
  "9"
};

uint32_t buttonCodes[BUTTON_COUNT] = {0};
int currentButtonIndex = 0;
unsigned long lastCaptureTime = 0;

void showPrompt() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press button:");
  lcd.setCursor(0, 1);
  lcd.print(buttonNames[currentButtonIndex]);
}

void showDone() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mapping done");
  lcd.setCursor(0, 1);
  lcd.print("Check serial");
}

void printFullMap() {
  Serial.println();
  Serial.println("----- Button Map -----");
  for (int i = 0; i < BUTTON_COUNT; i++) {
	Serial.print(buttonNames[i]);
	Serial.print(" = 0x");
	Serial.println(buttonCodes[i], HEX);
  }
  Serial.println("----------------------");
}

void saveCodeToSd(const char* name, uint32_t code) {
  logFile = SD.open("buttonmap.txt", FILE_WRITE);
  if (logFile) {
	logFile.print(name);
	logFile.print("=");
	logFile.print("0x");
	logFile.println(code, HEX);
	logFile.close();
  }
}

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Guided Mapper");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1200);

  if (!SD.begin(SD_CS_PIN)) {
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("SD failed");
	lcd.setCursor(0, 1);
	lcd.print("Check wiring");
	Serial.println("SD init failed");
	while (true) {
	  delay(1000);
	}
  }

  logFile = SD.open("buttonmap.txt", FILE_WRITE);
  if (logFile) {
	logFile.println("--- New Session ---");
	logFile.close();
  }

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  showPrompt();
}

void loop() {
  if (currentButtonIndex >= BUTTON_COUNT) {
	return;
  }

  if (IrReceiver.decode()) {
	if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
	  uint32_t code = IrReceiver.decodedIRData.decodedRawData;

	  if (millis() - lastCaptureTime > 350) {
		buttonCodes[currentButtonIndex] = code;

		Serial.print(buttonNames[currentButtonIndex]);
		Serial.print(" = 0x");
		Serial.println(code, HEX);

		saveCodeToSd(buttonNames[currentButtonIndex], code);

		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print(buttonNames[currentButtonIndex]);
		lcd.setCursor(0, 1);
		lcd.print("0x");
		lcd.print(code, HEX);

		currentButtonIndex++;
		lastCaptureTime = millis();
		delay(900);

		if (currentButtonIndex < BUTTON_COUNT) {
		  showPrompt();
		} else {
		  showDone();
		  printFullMap();
		}
	  }
	}

	IrReceiver.resume();
  }
}
