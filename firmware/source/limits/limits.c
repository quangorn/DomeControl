#include "limits.h"
#include "source/common/utils.h"
#include "source/common/definitions.h"
#include "source/encoder/encoder.h"
#include <avr/io.h>

static bool limitForwardState = false;
static bool limitReverseState = false;
static bool limitCenterState = false;

void limitsInit() {
	//enable pull up for limit pins
	OUTPORT(LIMIT_FORWARD_PORT) |= 1 << LIMIT_FORWARD_PIN;
	OUTPORT(LIMIT_REVERSE_PORT) |= 1 << LIMIT_REVERSE_PIN;
	OUTPORT(LIMIT_CENTER_PORT) |= 1 << LIMIT_CENTER_PIN;
}

void limitsProceed() {
	bool newState = limitsIsForwardLimitReached();
	if (newState && !limitForwardState) {
		encoderSetForwardLimitPosition();
	}
	limitForwardState = newState;

	newState = limitsIsReverseLimitReached();
	if (newState && !limitReverseState) {
		encoderSetReverseLimitPosition();
	}
	limitReverseState = newState;

	newState = limitsIsOnCenter();
	if (newState && !limitCenterState) {
		encoderSetCenterPosition();
	}
	limitCenterState = newState;
}

bool limitsIsForwardLimitReached() {
	return !(INPORT(LIMIT_FORWARD_PORT) & (1 << LIMIT_FORWARD_PIN));
}

bool limitsIsReverseLimitReached() {
	return !(INPORT(LIMIT_REVERSE_PORT) & (1 << LIMIT_REVERSE_PIN));
}

bool limitsIsOnCenter() {
	return !(INPORT(LIMIT_CENTER_PORT) & (1 << LIMIT_CENTER_PIN));
}
