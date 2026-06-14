# Project TODO — Arduino Sensors

Private planning document. Do not make repo public while this file exists.

---

## In Progress

- [ ] Finish testing SettingsInput on hardware (EQ, number entry, VOL+/-, FUNC)
- [ ] Confirm LDR part number (likely GL5528) for general hardware docs
- [ ] Commit and push ledsOff background state machine fix
- [ ] Merge SettingsInput into SensorModes once validated
- [ ] Add 2/4 page indicator to temp mode LCD
- [ ] Add manual Fwd/Rev menu page navigation (replace millis() auto-scroll)
- [ ] Context-sensitive EQ settings per mode (Mode 3 dark-required bool, Mode 8 sound ceiling)
- [ ] EEPROM save/load for brightValue, darkValue, soundCeiling on EQ exit
- [ ] UV sensor daylight calibration (Mode 9) — needs outdoor sunlight test
- [ ] Fix menu crawl: update labels (Eco Light not Energy Save), add EQ/FUNC/ST/REPT page

---

## New Modes — Multi-Sensor

- [ ] **Comfort Index** (Temp + LDR) — single 0-9 LED score, too hot + bright = red
- [ ] **Security Alert** (PIR + Sound) — motion alone = yellow, motion + loud = full red
- [ ] **Sleep Check** (Temp + LDR + Sound) — all three below threshold = green good
- [ ] **Presence + Noise** (PIR + Sound) — walking by vs actively in room
- [ ] **Storm Indicator** (UV + LDR) — UV dropping + LDR dimming = overcast warning

---

## LCD Features

- [ ] **Pre-stored text crawl** — number keys pick a message, scrolls across LCD
- [ ] **LCD pixel art editor** — 16x2 block chars as 32-pixel canvas, Fwd/Rev cursor, Up/Down draw
- [ ] **T9 text entry** — multi-tap number keys to spell custom messages

---

## Games / Utilities

- [ ] **Jump game** — scrolling obstacle, custom LCD sprites, Play=jump, EEPROM high score
- [ ] **Simon Says** — LED pattern memory game, IR remote to repeat sequence
- [ ] **Reaction timer** — LED flash trigger, Play=response, ms score on LCD
- [ ] **Countdown timer** — set with number keys, LEDs tick down
- [ ] **Stopwatch** — Play=start/stop, 0=reset, LCD display
- [ ] **Metronome** — BPM via Up/Down, LED flash on beat
- [ ] **Morse code** — pre-stored phrases, LED flash pattern output
- [ ] **Dice roller** — Play=roll, LEDs show binary, LCD shows number
- [ ] **Binary counter** — LEDs count up/down in binary

---

## Phase 2 — Mega Port

- [ ] Repin all assignments from Uno to Mega
- [ ] Add DHT11/22 — replace LM35, adds humidity
- [ ] Add relay — temperature-triggered fan on D13 equiv
- [ ] Add HC-SR04 ultrasonic — proximity/obstacle
- [ ] Add stepper motor control
- [ ] Port all sensor modes and settings

---

## Phase 3 — NodeMCU WiFi Logger

- [ ] Flash NodeMCU with WiFi serial receiver sketch
- [ ] Wire Uno TX → NodeMCU RX (10k+20k voltage divider)
- [ ] Uno sends CSV sensor data over Serial
- [ ] NodeMCU serves live dashboard in browser
- [ ] Log temp, sound, light level over time

---

## Phase 4 — Robot Chassis

- [ ] Flash BTT SKR Mini E3 v3 with Klipper or Marlin
- [ ] Recover original Ender motherboard via BOOT0 pin + DFU
- [ ] Design chassis (3D print from Ender 3, laser cut, or scratch)
- [ ] Nano + NodeMCU chauffeur module — WiFi remote drive
- [ ] Integrate Mega sensor suite onto chassis
- [ ] Battery pack + buck converter for portable power

---

## Notes

- RAM budget: 66% used (1354/2048) on SettingsInput — danger zone at 80%+
- Flash: 74% used — ~8KB remaining
- All new modes estimated ~120 bytes RAM total — fits comfortably
- Jump game sprites go in PROGMEM (flash) not RAM — custom chars in LCD CGRAM
