#include "settings.h"

void settingsInitDefault() {
	settings.motorSpeedStepUp = 2;
	settings.motorSpeedStepDown = 5;
	settings.motorStartSpeed = 15;
	settings.motorMaxSpeed = 128;

	settings.encoderForwardLimitPosition = 100;
	settings.encoderReverseLimitPosition = -100;
}
