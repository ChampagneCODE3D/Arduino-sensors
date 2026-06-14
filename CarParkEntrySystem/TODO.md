# TODO — Car Park Entry System (Assignment 2)

## Phase 1 — Required Assignment Scope (Mega only)

- [ ] Wire ultrasonic sensor and validate car detection distance threshold
- [ ] Wire push button with stable debounce handling
- [ ] Wire traffic light module (or red/green LEDs)
- [ ] Wire servo and validate open/close angles and timing
- [ ] Wire LCD and implement required messages
- [ ] Implement state machine for required flow
- [ ] Validate full required demo sequence end-to-end
- [ ] Record demonstration video with all required actions visible

## Phase 2 — Bonus on Mega

- [ ] Add parking capacity counter
- [ ] Show FULL / AVAILABLE status on LCD
- [ ] Prevent gate opening when FULL
- [ ] Add second gate support (entry + exit)

## Phase 3 — Multi-board expansion

- [ ] Bring up Uno R4 WiFi as operations/POS node
- [ ] Bring up Uno Q as policy authority node
- [ ] Define serial protocol between boards
- [ ] Implement rules push: Uno Q -> Uno R4 -> Mega
- [ ] Implement command path: Uno R4 -> Mega
- [ ] Keep Mega as final actuator safety authority

## Phase 4 — Optional polish

- [ ] Add RFID override flow for security/tow access
- [ ] Add event log output stream for validation
- [ ] Add watchdog/fail-safe behavior for comms drops
