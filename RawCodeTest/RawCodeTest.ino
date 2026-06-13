#include <IRremote.hpp>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int irPin = 2;
const int sdCSPin = 10;
int pressCount = 0;
bool sdOk = false;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("IR Logger"));
  IrReceiver.begin(irPin, DISABLE_LED_FEEDBACK);

  sdOk = SD.begin(sdCSPin);
  if (!sdOk) {
	Serial.println(F("SD FAILED - Serial only"));
	lcd.setCursor(0, 1);
	lcd.print(F("SD FAIL-SerialOK"));
  } else {
	if (SD.exists("irlog.txt")) SD.remove("irlog.txt");
	File f = SD.open("irlog.txt", FILE_WRITE);
	if (f) { f.println(F("=== IR Raw Code Log ===")); f.close(); }
	Serial.println(F("SD ready - press buttons"));
	lcd.setCursor(0, 1);
	lcd.print(F("SD OK-PressBtn"));
  }
}

void loop() {
  if (IrReceiver.decode()) {
	uint8_t  cmd  = IrReceiver.decodedIRData.command;
	uint16_t addr = IrReceiver.decodedIRData.address;
	uint32_t raw  = IrReceiver.decodedIRData.decodedRawData;
	bool isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
	IrReceiver.resume();

	if (isRepeat) return;

	pressCount++;

	char line[40];
	sprintf(line, "%02d A:%04X C:%02X R:%08lX", pressCount, addr, cmd, raw);

	Serial.println(line);

	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print(F("#"));
	lcd.print(pressCount);
	lcd.print(F(" A:0x"));
	lcd.print(addr, HEX);
	lcd.setCursor(0, 1);
	lcd.print(F("CMD:0x"));
	lcd.print(cmd, HEX);

	if (sdOk) {
	  File f = SD.open("irlog.txt", FILE_WRITE);
	  if (f) { f.println(line); f.close(); }
	}
  }
}
