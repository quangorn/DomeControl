#include "encoder.h"
#include "source/common/definitions.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

volatile bool encoderCountingEnabled = false;
volatile bool encoderDirection = DIRECTION_FORWARD;
volatile int16_t encoderValue = 0;

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

void encoderSetValue(int16_t value) {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		encoderValue = value;
	}
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
