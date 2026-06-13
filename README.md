# Arduino Sensors Project

Multi-mode sensor controller built on Arduino Uno R3 (DFRduino clone) using Arduino CLI + Visual Studio.

## Author

Jordan (ChampagneCODE3D)  
Diploma in MET (Mechanical Engineering Technology) — SAIT  
Background: Industrial robotics, PLC/ladder logic, HMI/SCADA, sensor integration  
AI Declaration: Developed collaboratively with GitHub Copilot. All design decisions, hardware choices, and feature requirements are original human work.

---

## Active Sketch

`SensorModes/SensorModes.ino` — 8-mode sensor controller (experimental branch)  
`SensorModes/ButtonMap.h` — IR remote command map and mode definitions  
`ModeSwitching/ModeSwitching.ino` — Stable 6-mode lighting baseline (master branch, do not modify)

---

## Hardware

- Arduino Uno R3 clone (DFRduino)
- Keyestudio 16x2 I2C LCD (0x27)
- PIR motion sensor (D4)
- LDR light sensor (A0)
- IR receiver + NEC remote (D2)
- 9x LED bar (D5–D12, D3) — Green: D5–D7, Yellow: D8–D10, Red: D11, D12, D3
- LM35 temperature sensor on PCB module (A2)
- DFRobot sound/mic sensor DFR0034 (A1)
- GUVA-S12SD UV sensor (A3) — Mode 9 (pending)

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
- SD card logging was removed to stabilize RAM (was causing bus conflicts on Uno).
- RAM usage: ~56% / Flash: ~62% on current build.
- Part of the larger Modular Multi Node Smart Environment System project (MSCWOW).
