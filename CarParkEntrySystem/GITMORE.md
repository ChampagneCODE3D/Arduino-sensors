# GITMORE — Car Park Entry System Workflow

## Repository strategy

Use a dedicated repository for Assignment 2.

Suggested repo name:
- `Arduino-CarPark-Entry-System`

## Branch strategy

- `main` — stable/demo-ready
- `feature/mega-core-flow`
- `feature/capacity-full-logic`
- `feature/second-gate`
- `feature/uno-r4-ops`
- `feature/uno-q-authority`
- `feature/rfid-override`

## Commit style

Use short, specific commits:

- `feat(mega): implement ultrasonic car detection state`
- `feat(ui): lcd welcome/proceed message flow`
- `feat(gate): servo open close cycle with timeout`
- `feat(capacity): full lot block + lcd status`
- `feat(comms): add serial message protocol`

## Demo tags

Tag stable milestones:

- `v0.1-required-flow`
- `v0.2-capacity`
- `v0.3-multi-board`
- `v1.0-assignment-submit`

## Pull request checklist

- [ ] Build compiles for target board
- [ ] Required hardware path validated
- [ ] No blocking regressions
- [ ] README and TODO updated
- [ ] Demo/test notes included
