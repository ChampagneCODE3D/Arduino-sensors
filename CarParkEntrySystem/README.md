# Assignment 2: Arduino Car Park Entry System

Multi-board architecture for the next project.

## Board Roles

- **Arduino Mega (Gate Station)**
  - Real-time gate authority
  - Reads ultrasonic + button
  - Drives servo + traffic LEDs + LCD
  - Enforces safety and access conditions locally

- **Arduino Uno R4 WiFi (Ops / Mobile / POS)**
  - Operator/mobile command node
  - POS/payment simulation
  - Sends approved commands to Mega

- **Arduino Uno Q (Parking Authority)**
  - Policy/rule source
  - Capacity and override rules
  - Sends policy updates to Uno R4

## Assignment 2 Core Flow

1. Car detected by ultrasonic
2. LCD: `Welcome, Get Ticket`
3. Red LED ON
4. Button press issues ticket
5. Red OFF, Green ON, gate opens
6. LCD: `Proceed`
7. Delay, gate closes, reset state

## Bonus Plan

- Second gate (entry + exit)
- RFID override when lot is FULL
- POS-based gate authorization

## Communication Model

Mega remains actuator/safety authority.
Remote boards request actions, Mega validates and executes.

### Suggested serial messages

- `RULE,capacity,50`
- `RULE,override,0|1`
- `CMD,REQUEST_ENTRY`
- `CMD,OPEN_GATE`
- `EVENT,CAR_DETECTED`
- `EVENT,GATE_OPENED`
- `STATUS,available,37`

## Folder Layout

- `MegaGateStation/` - Assignment-required logic
- `UnoR4WiFiOps/` - mobile/POS gateway
- `UnoQAuthority/` - policy authority
- `TODO.md` - implementation plan
- `GITMORE.md` - branch/release/commit workflow
