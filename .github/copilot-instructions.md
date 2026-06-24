# Copilot Instructions

## Project Guidelines
- Arduino Uno multi-mode lighting controller project includes 6 modes with PIR, LDR, IR remote (0xBF00), LCD (0x27), and 9 LEDs (D5-D7 green, D8-D10 yellow, D11/D12/D3 red).
- Mode 1: smart room with a 15-second occupancy period.
- Mode 2: hallway mode with a 12-second fade and bright cutoff at 800.
- Mode 3: streetlight mode with dark activation and a 10-second collapse.
- Mode 4 (Energy Saving Room) should use adaptive lighting based on ambient light level: ≥500 = 1 green LED, 250-499 = 2 green LEDs, <250 = 3 green LEDs. LEDs should respond continuously to light changes during the 15-second occupancy period, not just at initial trigger.
- Removed SD/SPI to stabilize RAM.
- Implement per-mode timers with a consistent "Occupied/Vacant in Xs" UI.
- Include EQ in the menu crawl/help pages so settings access is visible in the on-device UI/help menu. The idle menu must explicitly show how to enter settings with EQ so users can discover it from the on-device crawl menu.
- Future enhancements may include temperature, sound, and UV sensor modes as simple light bar meters.
- UI rule for this project: do not show three items on a single LCD crawl/menu page; keep menu pages to two items max. Keep mode screens uncluttered; avoid bulky shorthand like 'F/R' or crowded status lines, especially in LED mode.
- User prefers simpler, less crowded interfaces; current Serial Message typing flow is considered confusing and should be streamlined.

## Control Behavior Preferences
- ST-REPT must always act as display hold (multimeter-style) and should never be repurposed as menu navigation.
- From Idle, use FORWARD to enter Serial Message mode (instead of REVERSE).

## Modular Multi Node Smart Environment System (MSCWOW)
- DFRduino Uno R3 clone: main node running PIR+LDR lighting controller (existing SensorModes sketch), Keyestudio LCD.
- Arduino Uno R4 WiFi: secondary node for servo, RGB LED strip, buzzer, optional web interface, and mobile/POS parameter control.
- Arduino Uno Q (4GB RAM, 32GB storage, Linux SBC): programming/testing station, also runs AI object recognition web app via webcam and serves as the parking authority node that updates rules and communicates through Uno R4 to Mega.
- Arduino Mega: gate station authority for real-time access control.
- Communication: SD card first, then WiFi/Bluetooth if time allows.
- Optional: SIYEENOVE 4 DOF robot arm (already tested via web app and SBC mode), iPixel LED display, Arduino Mega, micro:bit.
- Sensors available: GUVA UV, flame, alcohol, sound, IMU, temp/humidity, PIR, LDR (from SunFounder Elite Explorer Kit + Keyestudio kit).
- 3D printing: dashboard enclosure, sensor panel, LED strip mount, Uno Q case, robot arm base, iPixel stand.
- Project goal: build foundation of modular expandable system, not complete every module.

## Personal Considerations
- Jordan has ADHD and arthritis - uses Copilot for organization/wording, all design decisions are his own.