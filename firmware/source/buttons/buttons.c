#include "buttons.h"
#include "source/common/utils.h"
#include "source/common/definitions.h"
#include <avr/io.h>

static bool buttonForwardState = false;
static bool buttonReverseState = false;

void buttonsInit() {
	//enable pull up for buttons pins
	OUTPORT(BUTTON_FORWARD_PORT) |= 1 << BUTTON_FORWARD_PIN;
	OUTPORT(BUTTON_REVERSE_PORT) |= 1 << BUTTON_REVERSE_PIN;
}

bool buttonsIsForwardPressed() {
	bool oldState = buttonForwardState;
	buttonForwardState = !(INPORT(BUTTON_FORWARD_PORT) & (1 << BUTTON_FORWARD_PIN));
	return !oldState && buttonForwardState;
}

bool buttonsIsReversePressed() {
	bool oldState = buttonReverseState;
	buttonReverseState = !(INPORT(BUTTON_REVERSE_PORT) & (1 << BUTTON_REVERSE_PIN));
	return !oldState && buttonReverseState;
}
