#include "motor.h"
#include "source/common/definitions.h"
#include "source/common/utils.h"
#include "source/encoder/encoder.h"
#include "source/limits/limits.h"
#include "source/settings/settings.h"
#include <avr/io.h>

static uint8_t motorTargetSpeed;
static bool motorTargetDirection = DIRECTION_FORWARD;
static bool motorGoToEnabled = false;
static bool motorFindingCenter = false;
static int16_t motorTargetPosition = ENCODER_CENTER_POSITION;

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
	motorGoToEnabled = false;
	motorFindingCenter = false;
	motorTargetSpeed = settings.motorMaxSpeed;
	motorTargetDirection = direction;
	if (!motorIsMoving()) {
		motorSetDirection();
		encoderEnableCounting(motorTargetDirection);
	}
}

void motorStop() {
	motorGoToEnabled = false;
	motorFindingCenter = false;
	motorTargetSpeed = settings.motorStartSpeed;
}

bool motorIsStarted() {
	return motorTargetSpeed > settings.motorStartSpeed;
}

bool motorIsMoving() {
	return OCR1B > settings.motorStartSpeed;
}

void motorProceed() {
	int16_t encoderValue = encoderGetValue();
	int16_t forwardLimitPosition = encoderGetForwardLimitPosition();
	int16_t reverseLimitPosition = encoderGetReverseLimitPosition();
	if (motorGoToEnabled && !motorFindingCenter) {
		forwardLimitPosition = reverseLimitPosition = motorTargetPosition;
	}
	if ((motorTargetDirection == DIRECTION_FORWARD && encoderValue >= forwardLimitPosition) ||
		(motorTargetDirection == DIRECTION_REVERSE && encoderValue <= reverseLimitPosition) ||
		(motorFindingCenter && limitsIsOnCenter())) {
		motorStop();
	}
	uint8_t targetSpeed = motorTargetSpeed;
	if (!motorIsDirectionRight()) {
		targetSpeed = settings.motorStartSpeed;
	}
	if (OCR1B < targetSpeed) {
		if (!motorIsMoving()) {
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

void motorGoTo(int16_t position) {
	int16_t encoderValue = encoderGetValue();
	if (position > encoderValue) {
		motorStart(DIRECTION_FORWARD);
		int16_t forwardLimitPosition = encoderGetForwardLimitPosition();
		motorTargetPosition = position > forwardLimitPosition ? forwardLimitPosition : position;
	} else if (position < encoderValue) {
		motorStart(DIRECTION_REVERSE);
		int16_t reverseLimitPosition = encoderGetReverseLimitPosition();
		motorTargetPosition = position < reverseLimitPosition ? reverseLimitPosition : position;
	} else {
		return;
	}
	motorGoToEnabled = true;
}

void motorFindCenter() {
	motorGoTo(ENCODER_CENTER_POSITION);
	motorFindingCenter = true;
}
