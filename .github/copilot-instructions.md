# Copilot Instructions

## Project Guidelines
- Arduino Uno multi-mode lighting controller project includes 6 modes with PIR, LDR, IR remote (0xBF00), LCD (0x27), and 9 LEDs (D5-D7 green, D8-D10 yellow, D11/D12/D3 red).
- Mode 1: smart room with a 15-second occupancy period.
- Mode 2: hallway mode with a 12-second fade and bright cutoff at 800.
- Mode 3: streetlight mode with dark activation and a 10-second collapse.
- Mode 4 (Energy Saving Room) should use adaptive lighting based on ambient light level: ≥500 = 1 green LED, 250-499 = 2 green LEDs, <250 = 3 green LEDs. LEDs should respond continuously to light changes during the 15-second occupancy period, not just at initial trigger.
- Removed SD/SPI to stabilize RAM.
- Implement per-mode timers with a consistent "Occupied/Vacant in Xs" UI.
- Future enhancements may include temperature, sound, and UV sensor modes as simple light bar meters.