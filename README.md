# Arduino Sensors Project

Arduino Uno sensor and mode-switching project built in Visual Studio with Arduino CLI support.

## Overview

This workspace contains several Arduino sketches for PIR, LDR, IR remote, LCD, and LED-based sensor demos. The main working sketch is the mode-switching build, which lets the IR remote select different sensor behaviors.

## Current Working Modes

The remote is mapped to these modes:

- **1 - Room Light**: gradual LED bar based on LDR brightness and motion presence
- **2 - Hallway**: motion-triggered on/off style lighting
- **3 - Streetlight**: warning-style lighting behavior for dark conditions
- **4 - Energy Save**: occupancy-based light control
- **5 - Home Light**: brightness and presence-based lighting control
- **6 - Night Warn**: motion-in-dark warning mode with fading lights
- **Power**: return to idle

## Hardware Used

- Arduino Uno
- I2C LCD
- PIR motion sensor
- LDR light sensor
- IR receiver + remote
- LED bar / light outputs

## Main Sketch

The main sketch currently used for the assignment work is:

- `ModeSwitching/ModeSwitching.ino`

Supporting mapping file:

- `ModeSwitching/ButtonMap.h`

Other utility sketches in the repo include:

- `RemoteButtonMapper/RemoteButtonMapper.ino`
- `GuidedButtonMapper/GuidedButtonMapper.ino`
- `SDButtonLogger/SDButtonLogger.ino`
- `ArduinoSensorsLdrLcd.ino`

## LCD Behavior

The LCD shows:

- a rolling idle menu when no mode is selected
- the current LDR reading in active modes
- room occupancy status as **Room: Occupied** or **Room: Vacant**

## LED Behavior

The light bar responds to the LDR reading and mode logic. The dark threshold and bright threshold can be tuned in the sketch.

Current tuning values in the mode sketch:

- `darkValue = 77`
- `brightValue = 930`

## How to Build

This project is set up to work with Arduino CLI.

Example compile command:

```powershell
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile --fqbn arduino:avr:uno --build-path .arduino-build .\ModeSwitching
```

Example upload command:

```powershell
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' upload -p COM8 --fqbn arduino:avr:uno --input-dir .arduino-build .\ModeSwitching
```

## Notes

- `A4` and `A5` are reserved for I2C LCD wiring on the Uno.
- `D13` may behave oddly for an external LED, so it may need extra care.
- The IR remote mapping is stored in `ButtonMap.h`.

## Assignment Focus

The current work is centered on these assignment questions:

1. Smart room light system using PIR + LDR
2. Automatic hallway light system using motion + light sensing
3. Smart streetlight system using PIR + LDR
4. Energy-saving room lighting system
5. Smart home lighting system using occupancy and brightness
6. Night-time motion detection warning system

## Repository Status

The repo is tracked with Git locally. A GitHub remote has not been configured yet.
