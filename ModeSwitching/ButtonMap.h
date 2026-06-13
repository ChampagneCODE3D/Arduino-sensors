#pragma once
#include <Arduino.h>

// IR remote address (verified via RawCodeTest logger)
#define IR_ADDR     0xBF00

// Command codes (verified via RawCodeTest logger)
#define CMD_POWER   0x00
#define CMD_FORWARD 0x06
#define CMD_REVERSE 0x04
#define CMD_1       0x10
#define CMD_2       0x11
#define CMD_3       0x12
#define CMD_4       0x14
#define CMD_5       0x15
#define CMD_6       0x16

enum ProgramMode {
    MODE_IDLE                = 0,
    MODE_SMART_ROOM_LIGHT    = 1,
    MODE_HALLWAY_LIGHT       = 2,
    MODE_STREETLIGHT         = 3,
    MODE_ENERGY_SAVING_ROOM  = 4,
    MODE_SMART_HOME_LIGHTING = 5,
    MODE_NIGHT_WARNING       = 6
};



inline const char* getModeLabel(ProgramMode mode) {
	switch (mode) {
		case MODE_SMART_ROOM_LIGHT:    return "Room Light";
		case MODE_HALLWAY_LIGHT:       return "Hallway";
		case MODE_STREETLIGHT:         return "Streetlight";
		case MODE_ENERGY_SAVING_ROOM:  return "Energy Save";
		case MODE_SMART_HOME_LIGHTING: return "Wake-Up Light";
		case MODE_NIGHT_WARNING:       return "Night Warn";
		default:                       return "Idle";
	}
}

inline ProgramMode getModeFromCmd(uint8_t cmd) {
	switch (cmd) {
		case CMD_1: return MODE_SMART_ROOM_LIGHT;
		case CMD_2: return MODE_HALLWAY_LIGHT;
		case CMD_3: return MODE_STREETLIGHT;
		case CMD_4: return MODE_ENERGY_SAVING_ROOM;
		case CMD_5: return MODE_SMART_HOME_LIGHTING;
		case CMD_6: return MODE_NIGHT_WARNING;
		default:    return MODE_IDLE;
	}
}

inline const char* getModeDescription(ProgramMode mode) {
	switch (mode) {
		case MODE_SMART_ROOM_LIGHT:    return "PIR+LDR dim bar";
		case MODE_HALLWAY_LIGHT:       return "Fade hallway";
		case MODE_STREETLIGHT:         return "Fade street";
		case MODE_ENERGY_SAVING_ROOM:  return "Save energy";
		case MODE_SMART_HOME_LIGHTING: return "Progressive";
		case MODE_NIGHT_WARNING:       return "Warning fade";
		default:                       return "Select 1-6";
	}
}

inline const char* getModeCornerLabel(ProgramMode mode) {
  switch (mode) {
	case MODE_SMART_ROOM_LIGHT:    return "Room";
	case MODE_HALLWAY_LIGHT:       return "Hall";
	case MODE_STREETLIGHT:         return "Street";
	case MODE_ENERGY_SAVING_ROOM:  return "Save";
	case MODE_SMART_HOME_LIGHTING: return "Wake";
	case MODE_NIGHT_WARNING:       return "Warn";
	default:                       return "";
  }
}

inline bool isMappedCmd(uint8_t cmd) {
	return (cmd == CMD_1 || cmd == CMD_2 || cmd == CMD_3 ||
			cmd == CMD_4 || cmd == CMD_5 || cmd == CMD_6);
}
