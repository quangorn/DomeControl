#include "limits.h"
#include "source/common/utils.h"
#include "source/common/definitions.h"
#include <avr/io.h>

void limitsInit() {
	//enable pull up for limit pins
	OUTPORT(LIMIT_FORWARD_PORT) |= 1 << LIMIT_FORWARD_PIN;
	OUTPORT(LIMIT_REVERSE_PORT) |= 1 << LIMIT_REVERSE_PIN;
	OUTPORT(LIMIT_PARK_POSITION_PORT) |= 1 << LIMIT_PARK_POSITION_PIN;
}

bool limitsIsForwardLimitReached() {
	return !(INPORT(LIMIT_FORWARD_PORT) & (1 << LIMIT_FORWARD_PIN));
}

bool limitsIsReverseLimitReached() {
	return !(INPORT(LIMIT_REVERSE_PORT) & (1 << LIMIT_REVERSE_PIN));
}

bool limitsIsOnParkPosition() {
	return !(INPORT(LIMIT_PARK_POSITION_PORT) & (1 << LIMIT_PARK_POSITION_PIN));
}
