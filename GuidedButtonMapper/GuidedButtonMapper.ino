#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>
#include <EEPROM.h>

#define IR_RECEIVE_PIN 2
#define BUTTON_COUNT 21

const byte EEPROM_SIGNATURE = 0x5A;
const int EEPROM_SIGNATURE_ADDR = 0;
const int EEPROM_INDEX_ADDR = 1;
const int EEPROM_CODES_ADDR = 4;

// Change to 0x3F if your LCD uses that address instead of 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

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

void printFullMap() {
  Serial.println();
  Serial.println("----- Button Map -----");
  for (int i = 0; i < BUTTON_COUNT; i++) {
	Serial.print(buttonNames[i]);
	Serial.print(" = 0x");
	Serial.println(buttonCodes[i], HEX);
  }
  Serial.println("----------------------");
  Serial.println("Send P to print again");
  Serial.println("Send R to reset map");
}

void saveCode(int index, uint32_t code) {
  EEPROM.put(EEPROM_CODES_ADDR + (index * (int)sizeof(uint32_t)), code);
}

void saveProgress() {
  EEPROM.update(EEPROM_SIGNATURE_ADDR, EEPROM_SIGNATURE);
  EEPROM.update(EEPROM_INDEX_ADDR, (byte)currentButtonIndex);
}

void clearSavedMap() {
  currentButtonIndex = 0;
  for (int i = 0; i < BUTTON_COUNT; i++) {
	buttonCodes[i] = 0;
	saveCode(i, 0);
  }
  saveProgress();
}

void loadSavedMap() {
  if (EEPROM.read(EEPROM_SIGNATURE_ADDR) != EEPROM_SIGNATURE) {
	clearSavedMap();
	return;
  }

  currentButtonIndex = EEPROM.read(EEPROM_INDEX_ADDR);
  if (currentButtonIndex < 0 || currentButtonIndex > BUTTON_COUNT) {
	clearSavedMap();
	return;
  }

  for (int i = 0; i < currentButtonIndex; i++) {
	EEPROM.get(EEPROM_CODES_ADDR + (i * (int)sizeof(uint32_t)), buttonCodes[i]);
  }
}

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
  lcd.print("Serial: P / R");
}

void handleSerialCommands() {
  if (!Serial.available()) {
	return;
  }

  char command = toupper(Serial.read());
  if (command == 'P') {
	printFullMap();
  } else if (command == 'R') {
	clearSavedMap();
	Serial.println("Map cleared");
	showPrompt();
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

  loadSavedMap();
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  if (currentButtonIndex >= BUTTON_COUNT) {
	showDone();
	printFullMap();
  } else {
	showPrompt();
  }
}

void loop() {
  handleSerialCommands();

  if (currentButtonIndex >= BUTTON_COUNT) {
	return;
  }

  if (IrReceiver.decode()) {
	if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
	  uint32_t code = IrReceiver.decodedIRData.decodedRawData;

	  if (millis() - lastCaptureTime > 350) {
		buttonCodes[currentButtonIndex] = code;
		saveCode(currentButtonIndex, code);

		Serial.print(buttonNames[currentButtonIndex]);
		Serial.print(" = 0x");
		Serial.println(code, HEX);

		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print(buttonNames[currentButtonIndex]);
		lcd.setCursor(0, 1);
		lcd.print("0x");
		lcd.print(code, HEX);

		currentButtonIndex++;
		saveProgress();
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
