# Arduino Sensors Project

Multi-mode sensor controller built on Arduino Uno R3 (DFRduino clone) using Arduino CLI + Visual Studio.  
Part of a larger multi-board robotics and home automation platform — see **Hardware Inventory** and **Project Roadmap** below.

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