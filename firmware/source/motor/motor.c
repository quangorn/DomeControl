#include "motor.h"
#include "source/common/definitions.h"
#include "source/common/utils.h"
#include "source/encoder/encoder.h"
#include "source/settings/settings.h"
#include <avr/io.h>

static uint8_t motorTargetSpeed;
static bool motorTargetDirection = DIRECTION_FORWARD;

bool motorGetDirection() {
	return (bool)(OUTPORT(MOTOR_DIR_PORT) & (1 << MOTOR_DIR_PIN));
}

//совпадает ли текущее направление вращения мотора с установленным
bool motorIsDirectionRight() {
	return motorTargetDirection == motorGetDirection();
}

void motorSetDirection() {
	if (motorTargetDirection) {
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

	motorTargetSpeed = settings.motorStartSpeed;
	OCR1B = motorTargetSpeed;
}

void motorStart(bool direction) {
	motorTargetSpeed = settings.motorMaxSpeed;
	motorTargetDirection = direction;
	if (OCR1B <= settings.motorStartSpeed) {
		motorSetDirection();
		encoderEnableCounting(motorTargetDirection);
	}
}

void motorStop() {
	motorTargetSpeed = settings.motorStartSpeed;
}

bool motorIsStarted() {
	return motorTargetSpeed > settings.motorStartSpeed;
}

void motorProceed() {
	bool currentDirection = motorGetDirection();
	int16_t encoderValue = encoderGetValue();
	if ((currentDirection == DIRECTION_FORWARD && encoderValue >= encoderGetForwardLimitPosition()) ||
		(currentDirection == DIRECTION_REVERSE && encoderValue <= encoderGetReverseLimitPosition())) {
		motorStop();
	}
	uint8_t targetSpeed = motorTargetSpeed;
	if (!motorIsDirectionRight()) {
		targetSpeed = settings.motorStartSpeed;
	}
	if (OCR1B < targetSpeed) {
		if (OCR1B <= settings.motorStartSpeed) {
			//enable PWM Mode
			TCCR1A |= 1 << COM1B1;
		}
		if (targetSpeed - OCR1B < settings.motorSpeedStepUp) {
			OCR1B = targetSpeed;
		} else {
			OCR1B += settings.motorSpeedStepUp;
		}
	} else {
		if (OCR1B - targetSpeed < settings.motorSpeedStepDown) {
			OCR1B = targetSpeed;
		} else {
			OCR1B -= settings.motorSpeedStepDown;
		}
		if (OCR1B <= settings.motorStartSpeed) {
			if (motorIsDirectionRight()) {
				//disable PWM Mode
				TCCR1A &= ~(1 << COM1B1);
				//disable direction relay if motor is stopped
				motorTargetDirection = DIRECTION_FORWARD;
				motorSetDirection();
				encoderDisableCounting();
			} else {
				motorSetDirection();
				encoderEnableCounting(motorTargetDirection);
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
