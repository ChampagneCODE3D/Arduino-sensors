#pragma once

#include <Arduino.h>

enum ProgramMode {
  MODE_IDLE = 0,
  MODE_SMART_ROOM_LIGHT = 1,
  MODE_HALLWAY_LIGHT = 2,
  MODE_STREETLIGHT = 3,
  MODE_ENERGY_SAVING_ROOM = 4,
  MODE_SMART_HOME_LIGHTING = 5,
  MODE_NIGHT_WARNING = 6
};

struct ButtonAssignment {
  const char* buttonName;
  uint32_t irCode;
  ProgramMode mode;
  const char* modeLabel;
};

const uint32_t BTN_POWER = 0xFF00BF00;
const uint32_t BTN_VOL_UP = 0xFE01BF00;
const uint32_t BTN_FUNC_STOP = 0xFD02BF00;
const uint32_t BTN_REVERSE = 0xFB04BF00;
const uint32_t BTN_PLAY_PAUSE = 0xFA05BF00;
const uint32_t BTN_FORWARD = 0xF906BF00;
const uint32_t BTN_DOWN = 0xF708BF00;
const uint32_t BTN_VOL_DOWN = 0xF609BF00;
const uint32_t BTN_UP = 0xF50ABF00;
const uint32_t BTN_0 = 0xF30CBF00;
const uint32_t BTN_EQ = 0xF20DBF00;
const uint32_t BTN_ST_REPT = 0xF10EBF00;
const uint32_t BTN_1 = 0xEF10BF00;
const uint32_t BTN_2 = 0xEE11BF00;
const uint32_t BTN_3 = 0xED12BF00;
const uint32_t BTN_4 = 0xEB14BF00;
const uint32_t BTN_5 = 0xEA15BF00;
const uint32_t BTN_6 = 0xE916BF00;
const uint32_t BTN_7 = 0xE718BF00;
const uint32_t BTN_8 = 0xE619BF00;
const uint32_t BTN_9 = 0xE51ABF00;

const ButtonAssignment BUTTON_ASSIGNMENTS[] = {
  {"POWER", BTN_POWER, MODE_IDLE, "System Off"},
  {"VOL+", BTN_VOL_UP, MODE_SMART_ROOM_LIGHT, "Smart Room Light"},
  {"FUNC/STOP", BTN_FUNC_STOP, MODE_HALLWAY_LIGHT, "Hallway Light"},
  {"REVERSE", BTN_REVERSE, MODE_STREETLIGHT, "Streetlight"},
  {"PLAY/PAUSE", BTN_PLAY_PAUSE, MODE_ENERGY_SAVING_ROOM, "Energy Saving Room"},
  {"FORWARD", BTN_FORWARD, MODE_SMART_HOME_LIGHTING, "Smart Home Lighting"},
  {"DOWN", BTN_DOWN, MODE_NIGHT_WARNING, "Night Warning"}
};

const size_t BUTTON_ASSIGNMENT_COUNT = sizeof(BUTTON_ASSIGNMENTS) / sizeof(BUTTON_ASSIGNMENTS[0]);

inline const char* getModeLabel(ProgramMode mode) {
  switch (mode) {
	case MODE_SMART_ROOM_LIGHT:
	  return "Smart Room Light";
	case MODE_HALLWAY_LIGHT:
	  return "Hallway Light";
	case MODE_STREETLIGHT:
	  return "Streetlight";
	case MODE_ENERGY_SAVING_ROOM:
	  return "Energy Saving Room";
	case MODE_SMART_HOME_LIGHTING:
	  return "Smart Home Lighting";
	case MODE_NIGHT_WARNING:
	  return "Night Warning";
	default:
	  return "System Off";
  }
}

inline ProgramMode getModeFromCode(uint32_t code) {
  for (size_t i = 0; i < BUTTON_ASSIGNMENT_COUNT; i++) {
	if (BUTTON_ASSIGNMENTS[i].irCode != 0 && BUTTON_ASSIGNMENTS[i].irCode == code) {
	  return BUTTON_ASSIGNMENTS[i].mode;
	}
  }

  return MODE_IDLE;
}

inline bool isMappedCode(uint32_t code) {
  for (size_t i = 0; i < BUTTON_ASSIGNMENT_COUNT; i++) {
	if (BUTTON_ASSIGNMENTS[i].irCode != 0 && BUTTON_ASSIGNMENTS[i].irCode == code) {
	  return true;
	}
  }

  return false;
}
