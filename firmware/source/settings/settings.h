#ifndef DOMECONTROL_SETTINGS_H
#define DOMECONTROL_SETTINGS_H

#include <stdint.h>

struct Settings {
	uint8_t motorSpeedStepUp;
	uint8_t motorSpeedStepDown;
	uint8_t motorStartSpeed;
	uint8_t motorMaxSpeed;

	int16_t encoderForwardLimitPosition;
	int16_t encoderReverseLimitPosition;
} settings;

void settingsInitDefault(); //TODO: remove after eeprom save implementation

#endif //DOMECONTROL_SETTINGS_H
