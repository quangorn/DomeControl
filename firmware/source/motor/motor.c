#include "motor.h"
#include "source/common/definitions.h"
#include "source/common/utils.h"
#include <avr/io.h>

//TODO: move config to eeprom
#define MOTOR_SPEED_STEP_UP 2
#define MOTOR_SPEED_STEP_DOWN 5
#define MOTOR_START_SPEED 15
#define MOTOR_MAX_SPEED 128

uint8_t motorTargetSpeed = MOTOR_START_SPEED;
bool motorDirection = false;

//совпадает ли текущее направление вращения мотора с установленным
bool motorIsDirectionRight() {
	return !motorDirection == !(OUTPORT(MOTOR_DIR_PORT) & (1 << MOTOR_DIR_PIN)); //convert to bool
}

void motorSetDirection() {
	if (motorDirection) {
		OUTPORT(MOTOR_DIR_PORT) |= (1 << MOTOR_DIR_PIN);
	} else {
		OUTPORT(MOTOR_DIR_PORT) &= ~(1 << MOTOR_DIR_PIN);
	}
}

void motorInit() {
	//set pins to output mode
	DDRPORT(MOTOR_STEP_PORT) |= 1 << MOTOR_STEP_PIN;
	DDRPORT(MOTOR_DIR_PORT) |= 1 << MOTOR_DIR_PIN;

	//set none-inverting mode and fast PWM Mode
	TCCR1A |= (1 << WGM11) | (1 << WGM10);

	//set prescaler to 1024
	TCCR1B |= (1 << CS00) | (1 << CS02);

	OCR1B = MOTOR_START_SPEED;
}

void motorStart(bool direction) {
	motorTargetSpeed = MOTOR_MAX_SPEED;
	motorDirection = direction;
	if (OCR1B <= MOTOR_START_SPEED) {
		motorSetDirection();
	}
}

void motorStop() {
	motorTargetSpeed = MOTOR_START_SPEED;
}

bool motorIsStarted() {
	return motorTargetSpeed > MOTOR_START_SPEED;
}

void motorProceed() {
	uint8_t targetSpeed = motorTargetSpeed;
	if (!motorIsDirectionRight()) {
		targetSpeed = MOTOR_START_SPEED;
	}
	if (OCR1B < targetSpeed) {
		if (OCR1B <= MOTOR_START_SPEED) {
			//enable PWM Mode
			TCCR1A |= 1 << COM1B1;
		}
		if (targetSpeed - OCR1B < MOTOR_SPEED_STEP_UP) {
			OCR1B = targetSpeed;
		} else {
			OCR1B += MOTOR_SPEED_STEP_UP;
		}
	} else if (OCR1B > targetSpeed) {
		if (OCR1B - targetSpeed < MOTOR_SPEED_STEP_DOWN) {
			OCR1B = targetSpeed;
		} else {
			OCR1B -= MOTOR_SPEED_STEP_DOWN;
		}
		if (OCR1B <= MOTOR_START_SPEED) {
			if (motorIsDirectionRight()) {
				//disable PWM Mode
				TCCR1A &= ~(1 << COM1B1);
				//disable direction relay if motor is stopped
				motorDirection = false;
				motorSetDirection();
			} else {
				motorSetDirection();
			}
		}
	}
}

void motorToggle(bool direction) {
	if (motorIsStarted()) {
		motorStop();
	} else {
		motorStart(direction);
	}
}
