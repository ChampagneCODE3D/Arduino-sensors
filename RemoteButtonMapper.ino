#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>

#define IR_RECEIVE_PIN 2

// Change to 0x3F if your LCD uses that address instead of 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

uint32_t lastCode = 0;

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remote Mapper");
  lcd.setCursor(0, 1);
  lcd.print("Press button...");

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
}

void loop() {
  if (IrReceiver.decode()) {
	if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
	  uint32_t code = IrReceiver.decodedIRData.decodedRawData;

	  if (code != lastCode) {
		lastCode = code;

		Serial.print("Button code: 0x");
		Serial.println(code, HEX);

		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Code:");
		lcd.setCursor(0, 1);
		lcd.print("0x");
		lcd.print(code, HEX);
	  }
	}

	IrReceiver.resume();
  }
}
