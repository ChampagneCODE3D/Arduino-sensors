# Project TODO — Arduino Sensors

Private planning document. Do not make repo public while this file exists.

---

## In Progress

- [ ] Hardware-test arrow-key EQ settings on hardware (UP/DOWN/Fwd/Rev/FUNC defaults)
- [x] Remove typed number entry from settings — arrow-key only
- [x] Fix UP/DOWN direction inversion on Dark param
- [x] Fix PLAY lockup when pressed while in settings
- [x] Fix FUNC/STREPT/VOL/mode-select leaking into settings state
- [x] Add FUNC=reset-to-defaults inside settings with LCD confirmation
- [x] Fix LCD blank on wake/power when in settings
- [x] Expand idle crawl to 8 pages with settings navigation hints
- [ ] Confirm LDR part number (likely GL5528) for general hardware docs
- [ ] Commit and push current SensorModes updates (EQ fixes, Dice, Track, idle shortcuts)
- [ ] Merge SettingsInput into SensorModes once validated
- [ ] Add 2/4 page indicator to temp mode LCD
- [ ] Add manual Fwd/Rev menu page navigation (replace millis() auto-scroll)
- [x] Context-sensitive EQ behavior per mode (Sound mode EQ = sound ceiling; EQ blocked in non-applicable modes)
- [ ] EEPROM save/load for brightValue, darkValue, soundCeiling on EQ exit
- [ ] UV sensor daylight calibration (Mode 9) — needs outdoor sunlight test; update UV_RAW_MAX after
- [ ] Sync SettingsInput.ino with SensorModes EQ redesign (arrow-key only, defaults, guards)
- [ ] Add 2/4 page indicator to temp mode LCD

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
- [x] **Dice roller** — Play=roll, custom d2-d1000, presets, LCD + LED result map
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

## Phase 2.5 — Reference Tools (Mega or Uno R4 WiFi + OLED)

> Target hardware: Arduino Mega 2560 or Uno R4 WiFi paired with SSD1306 128×64 OLED (I2C).
> All lookup data stored in PROGMEM — negligible RAM cost, feasible even on Mega.
> Uno R3 explicitly excluded: insufficient Flash and RAM for element/formula tables.

### Periodic Table Lookup

- [ ] PROGMEM table: atomic number, symbol (2–3 chars), name (~10 chars), atomic mass, common oxidation states, most stable isotope count
- [ ] UP/DOWN scroll through elements 1–118 by atomic number
- [ ] FWD/REV cycle through data pages per element:
  - Page 1: Number · Symbol · Name
  - Page 2: Atomic mass · Category (metal/nonmetal/metalloid)
  - Page 3: Common charges (oxidation states)
  - Page 4: Notable isotopes (e.g., C-12, C-14)
- [ ] Number keys jump to element by atomic number (typed entry, same pattern as Dice mode)
- [ ] OLED display: element symbol large-font centre, details small-font below
- [ ] Serial output: full element data line for logging / copy-paste

### Formula Sheet Lookup

- [ ] PROGMEM formula library organised by category:
  - **Mechanics**: F=ma, KE=½mv², PE=mgh, v=u+at, s=ut+½at², p=mv
  - **Electricity**: V=IR, P=IV, P=V²/R, C=Q/V, E=½CV²
  - **Thermodynamics**: Q=mcΔT, PV=nRT, η=1−Tc/Th, ΔG=ΔH−TΔS
  - **Waves**: v=fλ, f=1/T, E=hf
  - **Chemistry**: pH=−log[H⁺], Kw=[H⁺][OH⁻], ΔH=ΣΔHf(products)−ΣΔHf(reactants)
  - **Geometry**: A=πr², V=(4/3)πr³, c²=a²+b², A=½bh
- [ ] FWD/REV cycle categories; UP/DOWN scroll formulas within category
- [ ] OLED: formula name line 1, expression line 2, variable key line 3–4
- [ ] PLAY prints formula + variable definitions to Serial
- [ ] Number keys select category directly (1=Mechanics, 2=Electricity … 6=Geometry)

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
