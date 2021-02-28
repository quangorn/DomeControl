#include "encoder.h"
#include "source/common/definitions.h"
#include "source/settings/settings.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

static volatile bool encoderCountingEnabled = false;
static volatile bool encoderDirection = DIRECTION_FORWARD;
static volatile int16_t encoderValue = ENCODER_CENTER_POSITION;

void encoderInit() {
	//enable pull up
	//OUTPORT(ENCODER_PORT) |= 1 << ENCODER_PIN;

	//set INT0 to trigger on rising edge
	MCUCR |= (1 << ISC00) | (1 << ISC01);
	//enable INT0
	GICR |= 1 << INT0;
}

void encoderEnableCounting(bool direction) {
	encoderDirection = direction;
	encoderCountingEnabled = true;
}

void encoderDisableCounting() {
	encoderCountingEnabled = false;
}

int16_t encoderGetValue() {
	int16_t value;
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		value = encoderValue;
	}
	return value;
}

int16_t encoderGetForwardLimitPosition() {
	return settings.encoderForwardLimitPosition;
}

int16_t encoderGetReverseLimitPosition() {
	return settings.encoderReverseLimitPosition;
}

int16_t encoderGetCenterPosition() {
	return ENCODER_CENTER_POSITION;
}

void encoderSetValue(int16_t value) {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		encoderValue = value;
	}
}

void encoderSetForwardLimitPosition() {
	encoderSetValue(encoderGetForwardLimitPosition());
}

void encoderSetReverseLimitPosition() {
	encoderSetValue(encoderGetReverseLimitPosition());
}

void encoderSetCenterPosition() {
	encoderSetValue(encoderGetCenterPosition());
}

//======= Interrupt vectors =======
ISR (INT0_vect) {
	if (encoderCountingEnabled) {
		if (encoderDirection == DIRECTION_FORWARD) {
			encoderValue++;
		} else {
			encoderValue--;
		}
	}
}
