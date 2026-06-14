# Howl's Moving Arduino

**A multi-board sensor controller, operator interface, and eventual self-propelled platform.**  
Currently running on Arduino Uno R3 (DFRduino clone) with Arduino CLI + Visual Studio Community 2026.  
Part of a larger robotics and home automation project — see **Hardware Inventory** and **Project Roadmap** below.

## Author

Jordan (ChampagneCODE3D)  
Diploma in MET (Mechanical Engineering Technology) — SAIT  
Background: Industrial robotics, PLC/ladder logic, HMI/SCADA, sensor integration  
AI Declaration: Developed collaboratively with GitHub Copilot. All design decisions, hardware choices, and feature requirements are original human work.

---

## Project Log

### June 2026 — Full Button Map, Lux Display, LED Kill, Background States

Pushed hard this session — everything from completing the 21-button IR map to rethinking how the LED kill should work.

**What got built:**
- All 21 remote buttons now mapped and functional — nothing left as no-op
  - `PLAY` resets the current mode (clears stuck timers, PIR state)
  - `VOL+` / `VOL-` adjust sound ceiling in Mode 8, or LDR threshold everywhere else
  - `FUNC` kills LED output without leaving the mode — `[X]` shown in corner
  - `FUNC` inside settings shows a help screen for the settings controls
- `ledsOff` gate moved to output functions (`setLedCount`, `greenOnly`, `collapseToMiddle`) instead of early-returning from `applyMode` — state machines, timers, and animations now keep running in the background when LEDs are killed. Sunrise/sunset, warning scans, hallway fades all continue invisibly and snap back correctly on re-enable
- Lux display replaces raw LDR value — GL5528 + 10kΩ pull-down formula, real engineering units on the LCD
- Temp display redesigned: value + degree symbol left-aligned, full scale name right (`Celsius`, `Fahrenheit`, `Kelvin`, `Rankine`) — no cryptic abbreviations
- Temp LED bar rescaled: 0–30°C range so 19°C sits at ~6 LEDs into yellow, not a single green LED
- `[H]` hold indicator and `[X]` LED kill indicator share the corner label slot — clean, consistent

**What got fought:**
- `FUNC` re-enable was slow because `applyMode` runs on the next loop tick (150ms delay). Fixed by calling `applyMode` immediately on re-enable from the IR handler
- Lux formula initially had the voltage divider backwards — resistor is pull-down not pull-up, so R_ldr = 10k×(1023-raw)/raw not the other way. Caught before upload by checking the circuit
- `allOff()` needed to still fire when FUNC first kills LEDs, but be gated during normal mode runs — solved by keeping `allOff()` ungated and gating only the output helpers that modes call during normal operation

**Where it's going:**
TODO.md has the full list. Next priorities are finishing hardware testing of SettingsInput and starting the first multi-sensor combo mode (Comfort Index).

### June 2026 — Sensor Modes & UI Complete

Started this phase wanting to add temperature, sound and UV sensing on top of the existing 6-mode lighting controller, and ended up building a full operator interface in the process.

**What got built:**
- 3 new sensor modes (Modes 7–9): temperature with 4 unit pairs, sound level bar, UV index stub
- EQ settings menu for live threshold tuning without reflashing — type values directly with the IR number keys
- ST/REPT display hold, works like a meter hold with `[H]` indicator on screen
- Power button toggles LCD on/off and now correctly stays in the current mode on restore
- `0` always returns to menu and clears hold state so you can never get stuck
- Menu restructured to 2 items per page on a 16x2 LCD — much more readable
- Mode names updated: Eco Light (economy + ecological), Smart Home, Warning Light

**What got fought along the way:**
- Reversed LM35 destroyed the original bare sensor — replaced with a PCB module that enforces polarity. Lesson: always use the breakout board on analog temp sensors.
- SD card logging looked easy until it ate ~900 bytes of RAM and caused random resets. Removed completely. The Uno simply doesn't have the headroom. Logging deferred to NodeMCU WiFi phase.
- LED and LCD bars were out of sync on the sound mode because two separate `analogRead()` calls on the same pin return different values. Fixed with a single shared sample per loop.
- The EQ button was silently ignored inside settings because the IR handler was accidentally nested inside an `else` block that didn't run when `inSettings` was true. Classic logic inversion bug — spotted via Serial Monitor output showing the command arriving but nothing happening.
- Arduino IDE is functional but clunky for anything beyond a single-file sketch. Switched to Visual Studio Community 2026 with Arduino CLI for all editing and builds. Arduino IDE kept open on the side purely for its Serial Monitor.

**Where it's going:**
The Uno is basically feature-complete now. Hardware inventory check revealed a Mega 2560, two NodeMCU ESP8266 boards (on order), a BTT SKR Mini E3 stepper controller, and an Ender 3 v2 as a potential chassis donor. The long-term goal is a multi-board self-propelled platform — sensors and lighting on the Mega, WiFi logging via NodeMCU, motion via the SKR Mini. Think Howl's Moving Castle, Arduino edition.

---

## Development Environment

This project uses **Visual Studio Community 2026** with the Arduino extension rather than the Arduino IDE.

**Why Visual Studio over Arduino IDE:**
- Full IntelliSense, symbol navigation, and code search
- Better file and project management for multi-file sketches
- Git integration built in
- Arduino IDE is still used for Serial Monitor — VS does not have a native serial terminal

**Build and upload is handled via Arduino CLI from the VS integrated PowerShell terminal:**

```powershell
# Compile
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile --fqbn arduino:avr:uno SensorModes

# Upload
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' upload -p COM8 --fqbn arduino:avr:uno SensorModes

# Check memory usage (bytes line in output)
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile --fqbn arduino:avr:uno SensorModes 2>&1 | Select-String "bytes"
```

**Serial Monitor:** Open Arduino IDE separately and use its Serial Monitor (9600 baud) for live sensor debugging. Arduino CLI has no built-in serial terminal.

---

## Active Sketches

| File | Branch | Purpose |
|------|--------|---------|
| `SensorModes/SensorModes.ino` | `feature/temp-mode` | 9-mode sensor controller — stable experimental |
| `SensorModes/ButtonMap.h` | `feature/temp-mode` | IR command map and mode label helpers |
| `SettingsInput/SettingsInput.ino` | `feature/settings-numentry` | Settings menu with direct number entry via IR keypad |
| `SettingsInput/ButtonMap.h` | `feature/settings-numentry` | Updated ButtonMap for SettingsInput |
| `ModeSwitching/ModeSwitching.ino` | `master` | Stable 6-mode lighting baseline — do not modify |

---

## Hardware

### Current Build (Uno)
- Arduino Uno R3 clone (DFRduino)
- Keyestudio 16x2 I2C LCD (0x27)
- PIR motion sensor (D4)
- LDR light sensor (A0)
- IR receiver + NEC remote (D2)
- 9x LED bar (D5–D12, D3) — Green: D5–D7, Yellow: D8–D10, Red: D11, D12, D3
- LM35 temperature sensor on PCB module (A2)
- DFRobot sound/mic sensor DFR0034 (A1)
- GUVA-S12SD UV sensor (A3) — Mode 9 (pending daylight calibration)

### Full Hardware Inventory
| Board | Status | Planned Role |
|-------|--------|--------------|
| Arduino Uno R3 (DFRduino) | Active | Sensor controller — current project |
| Arduino Mega 2560 + shield | Ready | Expanded I/O — port from Uno when done |
| Arduino Uno R4 WiFi | Boxed | Future — onboard LED matrix + WiFi/BT |
| Arduino Uno R4 (non-WiFi) | Available | Spare / comparison |
| NodeMCU ESP8266 + OLED ×2 | On order | WiFi logger + robot chauffeur controller |
| BTT SKR Mini E3 (never flashed) | Ready | Stepper motion controller for robot chassis |
| Ender 3 v2 frame + steppers | Available | Robot chassis donor / repurpose |
| Original Ender 3 motherboard | SD failed | Recoverable via BOOT0 pin + DFU flash |

### Peripherals Available
- DHT11 or DHT22 temp/humidity sensor (3-wire, D13 on Uno)
- Stepper motor driver (ULN2003 or similar — pending ID)
- Blue cube relay (bare component — requires NPN transistor + flyback diode)
- Servo expansion shield (on Mega — separate power rail for actuators)
- HC-SR04 ultrasonic sensor (Mega kit — obstacle avoidance)

---

## Modes (SensorModes branch)

| Button | Mode | Sensor | Behavior |
|--------|------|--------|----------|
| 1 | Room Light | PIR + LDR | 15s occupancy, LED bar dims with light |
| 2 | Hallway | PIR + LDR | 12s timeout, fade animation, bright override |
| 3 | Streetlight | PIR + LDR | Motion in dark only, 10s countdown, sustained darkness required |
| 4 | Eco Light | PIR + LDR | Adaptive 1–3 green LEDs by ambient light — Economy + Ecological |
| 5 | Smart Home | PIR + LDR | Sunrise/hold/sunset cycle, gentle wake-up, sensory-friendly |
| 6 | Warning Light | PIR + LDR | Adaptive scanning warning, 10s occupied / 20s countdown |
| 7 | Temperature | LM35 (A2) | 4 unit pairs: C/F, K/C, K/R, R/F — Fwd/Rev cycles pairs |
| 8 | Mic Bar | DFR0034 (A1) | Level meter — synced LED + LCD block bar |
| 9 | UV Index | GUVA-S12SD (A3) | UV index bar (pending daylight calibration) |

---

## IR Remote Mapping

See `BUTTON_MAP.txt` at repo root for full reference including settings menu controls.

Verified via RawCodeTest logger (NEC protocol, address 0xBF00):

| Button  | CMD hex | Function |
|---------|---------|----------|
| Power   | 0x00 | LCD on/off toggle — stays in current mode |
| Fwd     | 0x06 | Next mode / temp unit >> |
| Rev     | 0x04 | Prev mode / temp unit << |
| EQ      | 0x0D | Settings menu / confirm number entry |
| ST/REPT | 0x0E | Freeze/unfreeze display ([H] shown when held) |
| Up      | 0x0A | Threshold +10 in settings |
| Down    | 0x08 | Threshold -10 in settings |
| 0       | 0x0C | Return to menu (clears hold if active) |
| 1–9     | 0x10–0x1A | Select mode / type digits in settings |

---

## Calibration Notes

### LDR (A0)
- Dark threshold: `darkValue = 77`
- Bright threshold: `brightValue = 800`
- Tuned via EQ settings menu — values persist in RAM, EEPROM save planned.
- Tune in situ: enter EQ menu, use Up/Down to nudge ±10 or type value directly with number keys.

### LM35 Temperature Sensor (A2)
- Sensor: LM35 on Keyestudio PCB module with 3-pin connector (S/+/-)
- Formula: `analogRead(A2) * 0.48876f` (10mV/°C at 5V reference = 4.8876mV per ADC count)
- **Issue encountered:** Original bare LM35 was wired backwards and damaged — stuck at ~52°C output. Replaced with LM35 on PCB module which enforces correct polarity.
- Smoothing: exponential moving average `(prev * 2 + new) / 3` seeded from real reading on boot.
- LCD refresh: every 20 seconds to prevent flicker; immediate on unit pair change.
- Accuracy: reads ~1–2°C below reference thermometer — within LM35 spec (±0.5°C typical, ±1.5°C max). No offset applied.

### Sound Sensor (A1)
- Sensor: DFRobot DFR0034 analog microphone
- Raw ADC range observed: 0 (dead quiet) to ~417 (loud clap/shout), typical speech 70–110
- LED bar ceiling: 250 (clips to full bar at loud sounds, responsive for normal speech)
- LCD block bar ceiling: 250 (16 chars, synced to same sample as LED bar)
- **Issue encountered:** LED and LCD bars were out of sync when using separate analogRead calls. Fixed by taking a single sample per loop and passing it to both display functions.

### GUVA-S12SD UV Sensor (A3)
- Calibration pending — requires outdoor daylight exposure. Reads near zero indoors.

---

## Known Issues Resolved

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| SD card logging causing random resets | SD library needs 512-byte sector buffer — not enough RAM on Uno | Removed SD entirely; logging deferred to NodeMCU WiFi logger in Phase 3 |
| LM35 reading ~52°C constantly | Bare LM35 wired backwards, pin damaged | Replaced with PCB module that enforces polarity |
| LED and LCD bars out of sync | Two separate `analogRead()` calls returning different values | Single shared read per loop passed to both outputs |
| LCD flicker on temperature display | LCD rewriting every 150ms loop tick | Added 20-second refresh gate; only redraws on value change |
| Power button returning to menu | `setMode(MODE_IDLE)` called on LCD restore | Changed to `lastDisplayedMode = -1` to force redraw without mode change |
| Switch-case static variable bugs (Modes 3–6) | Arduino/GCC scope issue with `static` locals inside switch cases | Moved complex modes to early-return branches before the switch |
| Stale occupancy state across modes | Single global occupancy flag shared by all modes | Replaced with per-mode timers and state machines reset on `setMode()` |
| Menu crowded on 16x2 LCD | Too many items per page | Reduced to 2 items per page, 6 pages total |

---

## RAM Budget (Uno — 2KB total)

| Sketch | RAM used | Free | Flash |
|--------|----------|------|-------|
| SensorModes | 1254 bytes (61%) | 794 bytes | 66% |
| SettingsInput | 1283 bytes (62%) | 765 bytes | 69% |

**Practical limits on Uno:**
- Under 70% = stable
- 70–80% = risky, stack overflow possible
- SD card logging requires ~900 bytes minimum — not feasible on Uno
- EEPROM save adds ~0 RAM (hardware registers only)

---

## Project Roadmap

### Phase 1 — Uno Polish (current)
- [x] 9 sensor modes working
- [x] EQ settings menu (brightValue / darkValue) with Up/Down nudge
- [x] Direct number entry in settings via IR keypad (SettingsInput branch)
- [x] LCD power toggle stays in current mode
- [x] ST/REPT display hold with [H] indicator
- [x] 0 = menu, Power = LCD on/off
- [ ] EEPROM save/load so thresholds survive power off
- [ ] UV sensor daylight calibration (Mode 9)
- [ ] Merge SettingsInput to SensorModes when validated

### Phase 2 — Mega Expansion
- [ ] Port SensorModes sketch to Mega (repin assignments)
- [ ] Add DHT11/22 — replace LM35, adds humidity for free on one pin
- [ ] Add relay — temperature-triggered fan/actuator
- [ ] Add HC-SR04 ultrasonic — proximity / obstacle awareness
- [ ] Add stepper motor control
- [ ] EEPROM settings persist (same API on Mega)

### Phase 3 — NodeMCU WiFi Logger
> NodeMCU ESP8266 boards with onboard OLED (×2) — on order

- [ ] Flash NodeMCU with WiFi serial receiver sketch
- [ ] Uno TX → NodeMCU RX (5V→3.3V: 10kΩ + 20kΩ voltage divider)
- [ ] Uno sends CSV sensor lines over Serial
- [ ] NodeMCU stores to 4MB flash and/or serves live dashboard over WiFi
- [ ] Sensor history accessible in browser on phone, no PC required

### Phase 4 — Robot Chassis (Howl's Moving Castle concept)
> Inspired by the Howl's Moving Castle aesthetic — a self-propelled multi-board platform

**Architecture:**
```
BTT SKR Mini E3     →  4-axis stepper locomotion (TMC2209, silent StealthChop)
Arduino Mega        →  sensors, LED lighting modes, environment awareness
NodeMCU #1          →  WiFi web remote / autonomous nav logic
NodeMCU #2          →  live sensor telemetry dashboard
```

**Chauffeur module concept (Nano + NodeMCU piggyback):**
```
Phone / Browser  →  WiFi  →  NodeMCU  →  Serial  →  Arduino Nano  →  Motors / Servos
                                                                    ←  Sensor feedback
```
- Arduino Nano handles real-time motor and servo control (fast loop, no WiFi overhead)
- NodeMCU handles WiFi command reception and path logic
- Compact enough to piggyback on any chassis as a self-contained drive module
- 3.3V logic level shifter required on NodeMCU RX line from Nano TX

**Chassis donor:** Ender 3 v2 frame + steppers  
**Motion controller:** BTT SKR Mini E3 v3 (never flashed — ready for Klipper or Marlin)  
**Sensorless homing:** TMC2209 StallGuard — no limit switches needed  
**Old Ender motherboard:** SD card failed but recoverable via BOOT0 jumper + DFU mode over USB

**Still needed:**
- DC gear motors or additional steppers for drive wheels
- L298N or similar motor driver for drive wheels
- LiPo or 18650 battery pack
- Buck converter (battery → 5V logic rail)
- Chassis frame (3D print from Ender 3, laser cut, or scratch built)


## Author

Jordan (ChampagneCODE3D)  
Diploma in MET (Mechanical Engineering Technology) — SAIT  
Background: Industrial robotics, PLC/ladder logic, HMI/SCADA, sensor integration  
AI Declaration: Developed collaboratively with GitHub Copilot. All design decisions, hardware choices, and feature requirements are original human work.

---

## Active Sketch

`SensorModes/SensorModes.ino` — 9-mode sensor controller (experimental branch)  
`SensorModes/ButtonMap.h` — IR remote command map and mode definitions  
`ModeSwitching/ModeSwitching.ino` — Stable 6-mode lighting baseline (master branch, do not modify)

---

## Hardware

### Current Build (Uno)
- Arduino Uno R3 clone (DFRduino)
- Keyestudio 16x2 I2C LCD (0x27)
- PIR motion sensor (D4)
- LDR light sensor (A0)
- IR receiver + NEC remote (D2)
- 9x LED bar (D5–D12, D3) — Green: D5–D7, Yellow: D8–D10, Red: D11, D12, D3
- LM35 temperature sensor on PCB module (A2)
- DFRobot sound/mic sensor DFR0034 (A1)
- GUVA-S12SD UV sensor (A3) — Mode 9 (pending daylight calibration)

### Full Hardware Inventory
| Board | Status | Planned Role |
|-------|--------|--------------|
| Arduino Uno R3 (DFRduino) | Active | Sensor controller — current project |
| Arduino Mega 2560 + shield | Ready | Expanded I/O — port from Uno when done |
| Arduino Uno R4 WiFi | Boxed | Future — onboard LED matrix + WiFi/BT |
| Arduino Uno R4 (non-WiFi) | Available | Spare / comparison |
| NodeMCU ESP8266 + OLED ×2 | On order | WiFi logger + robot chauffeur controller |
| BTT SKR Mini E3 (never flashed) | Ready | Stepper motion controller for robot chassis |
| Ender 3 v2 frame + steppers | Available | Robot chassis donor / repurpose |
| Original Ender 3 motherboard | SD failed | Recoverable via BOOT0 pin + DFU flash |

### Peripherals Available
- DHT11 or DHT22 temp/humidity sensor (3-wire, D13 on Uno)
- Stepper motor driver (ULN2003 or similar — pending ID)
- Blue cube relay (bare component — requires NPN transistor + flyback diode)
- Servo expansion shield (on Mega — separate power rail for actuators)
- HC-SR04 ultrasonic sensor (Mega kit — obstacle avoidance)

---

## Modes (SensorModes branch)

| Button | Mode | Sensor | Behavior |
|--------|------|--------|----------|
| 1 | Room Light | PIR + LDR | 15s occupancy, LED bar dims with light |
| 2 | Hallway | PIR + LDR | 12s timeout, fade animation, bright override |
| 3 | Streetlight | PIR + LDR | Motion in dark only, 10s countdown, sustained darkness required |
| 4 | Energy Save | PIR + LDR | Adaptive 1–3 green LEDs by ambient light level |
| 5 | Wake-Up | PIR + LDR | Sunrise/hold/sunset cycle, bounce-back on interruption |
| 6 | Night Warn | PIR + LDR | Adaptive scanning warning, 10s occupied / 20s countdown |
| 7 | Temperature | LM35 (A2) | 4 unit pairs: C/F, K/C, K/R, R/F — fw/rv cycles pairs |
| 8 | Sound Bar | DFR0034 (A1) | Level meter — synced LED + LCD block bar |
| 9 | UV Index | GUVA-S12SD (A3) | UV index bar (pending) |

---

## IR Remote Mapping

Verified via RawCodeTest logger (NEC protocol, address 0xBF00):

| Button | CMD hex |
|--------|---------|
| Power  | 0x00 |
| Fwd    | 0x06 |
| Rev    | 0x04 |
| 1      | 0x10 |
| 2      | 0x11 |
| 3      | 0x12 |
| 4      | 0x14 |
| 5      | 0x15 |
| 6      | 0x16 |
| 7      | 0x18 |
| 8      | 0x19 |
| 9      | 0x1A |

---

## Calibration Notes

### LDR (A0)
- Dark threshold: `darkValue = 77`
- Bright threshold: `brightValue = 800`
- Tuned manually by observing raw analogRead values in Serial Monitor at target light levels.

### LM35 Temperature Sensor (A2)
- Sensor: LM35 on Keyestudio PCB module with 3-pin connector (S/+/-)
- Formula: `analogRead(A2) * 0.48876f` (10mV/°C at 5V reference = 4.8876mV per ADC count)
- Note: Original bare LM35 was wired backwards and damaged (stuck at ~52°C output). Replaced with LM35 on PCB module.
- Smoothing: exponential moving average `(prev * 2 + new) / 3` seeded from real reading on boot.
- LCD refresh: every 20 seconds to prevent flicker; immediate on unit pair change.
- Accuracy: reads ~1–2°C below reference thermometer — within LM35 spec (±0.5°C typical, ±1.5°C max). No offset applied as reference devices (micro:bit, clock) may read warm themselves.

### Sound Sensor (A1)
- Sensor: DFRobot DFR0034 analog microphone
- Raw ADC range observed: 0 (dead quiet) to ~417 (loud clap/shout), typical speech 70–110
- LED bar ceiling: 250 (clips to full bar at loud sounds, responsive for normal speech)
- LCD block bar ceiling: 250 (16 chars, synced to same reading as LED bar)
- Both LED and LCD bars read from single analogRead per loop to stay in sync.

### GUVA-S12SD UV Sensor (A3)
- Calibration pending — Mode 9 not yet implemented.

---

## Build & Upload

```powershell
# Compile
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile --fqbn arduino:avr:uno SensorModes

# Upload
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' upload -p COM8 --fqbn arduino:avr:uno SensorModes
```

---

## Notes

- A4/A5 reserved for I2C LCD.
- SD card logging removed from Uno build to stabilize RAM (bus conflicts + insufficient RAM for 512-byte sector buffer).
- RAM: ~60% / Flash: ~66% on current build — no headroom for SD on Uno.
- Part of the larger Modular Multi Node Smart Environment System project (MSCWOW).

---

## Project Roadmap

### Phase 1 — Uno Polish (current)
- [x] 9 sensor modes working
- [x] EQ settings menu (brightValue / darkValue)
- [x] LCD power toggle, 0 = menu
- [ ] EEPROM save/load for settings (survives power off)
- [ ] Direct number entry in settings menu via IR keypad
- [ ] UV sensor daylight calibration (Mode 9)

### Phase 2 — Mega Expansion
- [ ] Port SensorModes sketch to Mega (repin assignments)
- [ ] Add DHT11/22 — replace LM35 with temp + humidity on one pin
- [ ] Add relay (D13 equivalent) — temperature-triggered fan/actuator
- [ ] Add HC-SR04 ultrasonic — proximity / obstacle awareness
- [ ] Add stepper motor control (freed pins from shift register or direct Mega pins)
- [ ] EEPROM settings persist (same code, same EEPROM API on Mega)

### Phase 3 — NodeMCU WiFi Logger
> NodeMCU ESP8266 boards with onboard OLED (×2) — on order

- [ ] Flash NodeMCU with WiFi serial receiver sketch
- [ ] Uno TX → NodeMCU RX (via 5V→3.3V voltage divider: 10kΩ + 20kΩ)
- [ ] Uno sends CSV sensor lines over Serial at each mode-display update
- [ ] NodeMCU stores to 4MB flash and/or serves live dashboard over WiFi
- [ ] Access sensor history in browser on phone, no PC required

### Phase 4 — Robot Chassis (Howl's Moving Castle concept)
> Inspired by the Howl's Moving Castle aesthetic — a self-propelled multi-board platform

**Architecture:**
```
BTT SKR Mini E3     →  4-axis stepper locomotion (TMC2209, silent StealthChop)
Arduino Mega        →  sensors, LED lighting modes, environment awareness
NodeMCU #1          →  WiFi web remote / autonomous nav logic
NodeMCU #2          →  live sensor telemetry dashboard
```

**Chauffeur module concept (Nano + NodeMCU stack):**
```
Phone / Browser  →  WiFi  →  NodeMCU  →  Serial  →  Arduino Nano  →  Motors / Servos
                                                                    ←  Sensor feedback
```
- Arduino Nano handles real-time motor and servo control (fast loop, no WiFi overhead)
- NodeMCU handles WiFi command reception and path logic
- Small enough to fit on any chassis — piggybacked as a self-contained drive module
- 3.3V logic level shifter required on NodeMCU RX line from Nano TX

**Chassis donor:** Ender 3 v2 frame + steppers (repurposed)  
**Motion controller:** BTT SKR Mini E3 v3 (never flashed — ready for Klipper or Marlin)  
**Sensorless homing:** TMC2209 StallGuard — no limit switches needed for leg/axis homing

**Still needed:**
- DC gear motors or additional steppers for drive wheels
- L298N or similar motor driver for drive wheels
- LiPo or 18650 battery pack for portable power
- Buck converter (battery voltage → 5V logic)
- Chassis frame (3D printed from Ender 3, laser cut, or scratch built)

---