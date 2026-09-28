#include "runner.h"

#include "fake/fake_avr.h"

#include "source/common/definitions.h"
#include "source/encoder/encoder.h"
#include "source/limits/limits.h"
#include "source/motor/motor.h"
#include "source/settings/settings.h"

static void resetWorld(void) {
	fakeAvrReset();
	settingsInitDefault();
	encoderInit();
	limitsInit();
	motorStop(); /* clears the goTo/findCentre flags left by a previous test */
	motorInit(); /* sets the start speed and the Timer1 mode */
	encoderDisableCounting();
	encoderSetValue(0);
}

static bool pwmEnabled(void) {
	return (TCCR1A & (1 << COM1B1)) != 0;
}

static bool directionRelay(void) {
	return (PORTB & (1 << MOTOR_DIR_PIN)) != 0;
}

static void proceedTimes(int count) {
	for (int i = 0; i < count; i++) {
		motorProceed();
	}
}

/*
 * Simulate an encoder that follows the motor: one step per motorProceed() in the
 * current relay direction. Returns the position reached.
 */
static int16_t followMotor(int maxSteps) {
	for (int i = 0; i < maxSteps && motorIsStarted(); i++) {
		int16_t value = encoderGetValue();
		value = (int16_t)(directionRelay() ? value - 1 : value + 1);
		encoderSetValue(value);
		motorProceed();
	}
	return encoderGetValue();
}

static void initSetsTimerModeAndPins(void) {
	resetWorld();

	TEST_ASSERT_EQ(settings.motorStartSpeed, OCR1B);
	TEST_ASSERT(!motorIsMoving());
	TEST_ASSERT(!motorIsStarted());
	TEST_ASSERT(!pwmEnabled());
	TEST_ASSERT(!directionRelay());

	TEST_ASSERT(DDRB & (1 << MOTOR_STEP_PIN));
	TEST_ASSERT(DDRB & (1 << MOTOR_DIR_PIN));
	TEST_ASSERT_EQ((1 << WGM11) | (1 << WGM10), TCCR1A & ((1 << WGM11) | (1 << WGM10)));
	TEST_ASSERT_EQ((1 << CS00) | (1 << CS02), TCCR1B & ((1 << CS00) | (1 << CS02)));
}

static void startArmsTheEncoder(void) {
	resetWorld();
	motorStart(DIRECTION_FORWARD);

	INT0_vect();
	TEST_ASSERT_EQ(1, encoderGetValue());

	resetWorld();
	motorStart(DIRECTION_REVERSE);
	TEST_ASSERT(directionRelay());
	INT0_vect();
	TEST_ASSERT_EQ(-1, encoderGetValue());
}

static void startAcceleratesToMaxIn57Steps(void) {
	resetWorld();
	motorStart(DIRECTION_FORWARD);

	TEST_ASSERT(motorIsStarted());  /* target is the top speed */
	TEST_ASSERT(!motorIsMoving());  /* but OCR1B is still at the start speed */

	motorProceed();
	TEST_ASSERT(pwmEnabled());
	TEST_ASSERT_EQ(17, OCR1B);

	motorProceed();
	TEST_ASSERT_EQ(19, OCR1B);

	proceedTimes(55); /* 57 proceeds in total from the start speed to the top speed */
	TEST_ASSERT_EQ(128, OCR1B);
	TEST_ASSERT(motorIsMoving());

	motorProceed(); /* must not overshoot the top speed */
	TEST_ASSERT_EQ(128, OCR1B);
}

static void movingTracksThePwmRegisterNotTheTarget(void) {
	resetWorld();
	motorStart(DIRECTION_FORWARD);
	proceedTimes(1);
	TEST_ASSERT(motorIsMoving());

	motorStop();
	TEST_ASSERT(!motorIsStarted());
	TEST_ASSERT(motorIsMoving()); /* OCR1B is still high: the ramp has not run yet */
}

static void stopDeceleratesByFiveAndClearsPwm(void) {
	resetWorld();
	motorStart(DIRECTION_FORWARD);
	proceedTimes(57);
	TEST_ASSERT_EQ(128, OCR1B);

	motorStop();
	motorProceed();
	TEST_ASSERT_EQ(123, OCR1B);
	TEST_ASSERT(motorIsMoving());
	TEST_ASSERT(pwmEnabled());

	proceedTimes(21); /* 123 -> 18 */
	TEST_ASSERT_EQ(18, OCR1B);

	motorProceed(); /* 18 -> 15: the ramp clamps at the start speed */
	TEST_ASSERT_EQ(15, OCR1B);
	TEST_ASSERT(!motorIsMoving());
	TEST_ASSERT(!pwmEnabled());
	TEST_ASSERT(!directionRelay());

	encoderSetValue(7);
	INT0_vect();
	TEST_ASSERT_EQ(7, encoderGetValue()); /* counting is disabled with the motor */
}

static void reversingWhileMovingSwitchesRelayAfterRampDown(void) {
	resetWorld();
	motorStart(DIRECTION_FORWARD);
	proceedTimes(57);
	TEST_ASSERT_EQ(128, OCR1B);
	TEST_ASSERT(!directionRelay());

	encoderSetValue(10);
	motorStart(DIRECTION_REVERSE);

	TEST_ASSERT_EQ(128, OCR1B);       /* the ramp is untouched until motorProceed() */
	TEST_ASSERT(!directionRelay());   /* the relay must not flip while the motor moves */

	/* Characterisation of motor.c:47-56 (test_plan.md §10.7): the encoder keeps
	   counting in the old direction until the ramp reaches the bottom. */
	INT0_vect();
	TEST_ASSERT_EQ(11, encoderGetValue());

	proceedTimes(1);
	TEST_ASSERT_EQ(123, OCR1B);
	TEST_ASSERT(!directionRelay());

	proceedTimes(21); /* 123 -> 18 */
	TEST_ASSERT_EQ(18, OCR1B);
	TEST_ASSERT(!directionRelay());

	motorProceed(); /* 18 -> 15: the relay flips and the encoder is re-armed */
	TEST_ASSERT_EQ(15, OCR1B);
	TEST_ASSERT(directionRelay());

	encoderSetValue(10);
	INT0_vect();
	TEST_ASSERT_EQ(9, encoderGetValue()); /* now counting in the new direction */

	motorProceed(); /* and the ramp climbs again towards the same target speed */
	TEST_ASSERT_EQ(17, OCR1B);
}

static void toggleStartsAndStops(void) {
	resetWorld();

	motorToggle(DIRECTION_FORWARD);
	TEST_ASSERT(motorIsStarted());

	motorToggle(DIRECTION_FORWARD);
	TEST_ASSERT(!motorIsStarted());
	TEST_ASSERT(!motorIsMoving());

	motorToggle(DIRECTION_FORWARD);
	TEST_ASSERT(motorIsStarted());
}

static void goToCurrentPositionDoesNotStart(void) {
	resetWorld();
	encoderSetValue(42);

	motorGoTo(42);
	TEST_ASSERT(!motorIsStarted());
	TEST_ASSERT_EQ(15, OCR1B);

	motorProceed();
	TEST_ASSERT_EQ(15, OCR1B);
	TEST_ASSERT(!pwmEnabled());
}

static void goToStopsAtItsTargetNotAtTheLimit(void) {
	resetWorld();
	encoderSetValue(0);
	motorGoTo(50);

	TEST_ASSERT(motorIsStarted());
	int16_t position = followMotor(200);
	TEST_ASSERT_EQ(50, position);
	TEST_ASSERT(!motorIsStarted());

	/* The contrast: a plain start has no target and runs up to the limit itself. */
	resetWorld();
	encoderSetValue(0);
	motorStart(DIRECTION_FORWARD);
	position = followMotor(200);
	TEST_ASSERT_EQ(100, position);
	TEST_ASSERT(!motorIsStarted());
}

static void goToClampsTheForwardTarget(void) {
	resetWorld();
	encoderSetValue(0);
	motorGoTo(500); /* clamped to the forward limit, 100 */

	int16_t position = followMotor(200);
	TEST_ASSERT_EQ(100, position);
	TEST_ASSERT(!motorIsStarted());
}

static void goToClampsTheReverseTarget(void) {
	resetWorld();
	encoderSetValue(0);
	motorGoTo(-500); /* clamped to the reverse limit, -100 */

	int16_t position = followMotor(200);
	TEST_ASSERT_EQ(-100, position);
	TEST_ASSERT(!motorIsStarted());
}

static void goToReversesFromMotion(void) {
	resetWorld();
	encoderSetValue(0);
	motorGoTo(50);
	TEST_ASSERT_EQ(50, followMotor(200));
	TEST_ASSERT(!directionRelay());

	motorGoTo(10);
	TEST_ASSERT(motorIsStarted());
	TEST_ASSERT_EQ(10, followMotor(400));
	TEST_ASSERT(!motorIsStarted());
	TEST_ASSERT(directionRelay());
}

static void findCenterFromNonZeroStopsOnTheCenterSwitch(void) {
	resetWorld();
	encoderSetValue(50);
	motorFindCenter();

	TEST_ASSERT(motorIsStarted());
	TEST_ASSERT(directionRelay());

	/* Drive the position down to 0 with the centre switch open: find centre does not
	   override the limits with its GoTo target, so the motor must keep running. */
	for (int i = 0; i < 200 && encoderGetValue() > 0; i++) {
		encoderSetValue((int16_t)(encoderGetValue() - 1));
		motorProceed();
	}
	TEST_ASSERT_EQ(0, encoderGetValue());
	TEST_ASSERT(motorIsStarted());

	fakePinPress(&PINC, LIMIT_CENTER_PIN);
	motorProceed();
	TEST_ASSERT(!motorIsStarted());

	proceedTimes(100);
	TEST_ASSERT_EQ(15, OCR1B);
	TEST_ASSERT(!motorIsMoving());
}

static void findCenterFromZeroStopsInOneIteration(void) {
	resetWorld();
	encoderSetValue(0);
	fakePinPress(&PINC, LIMIT_CENTER_PIN);
	motorFindCenter();

	TEST_ASSERT(!motorIsStarted());
	TEST_ASSERT_EQ(15, OCR1B);

	/* motorGoTo() returned early, so this iteration is the one that goes through
	   motorStop() and clears the find-centre flag. */
	motorProceed();
	TEST_ASSERT_EQ(15, OCR1B);
	TEST_ASSERT(!pwmEnabled());

	motorProceed();
	TEST_ASSERT_EQ(15, OCR1B);
	TEST_ASSERT(!directionRelay());
}

static uint32_t fuzzState = 1;

static uint32_t fuzzNext(void) {
	fuzzState = fuzzState * 1103515245u + 12345u;
	return fuzzState >> 16;
}

static void fuzzRandomCommandsKeepInvariants(void) {
	resetWorld();
	fuzzState = 20260928u;

	int violations = 0;
	for (int step = 0; step < 5000; step++) {
		switch (fuzzNext() % 6u) {
			case 0: motorStart(DIRECTION_FORWARD); break;
			case 1: motorStart(DIRECTION_REVERSE); break;
			case 2: motorStop(); break;
			case 3: motorToggle((fuzzNext() & 1u) ? DIRECTION_REVERSE : DIRECTION_FORWARD); break;
			case 4: motorGoTo((int16_t)((int)(fuzzNext() % 201u) - 100)); break;
			default: motorFindCenter(); break;
		}

		if (motorIsMoving()) {
			int16_t value = encoderGetValue();
			value = (int16_t)(directionRelay() ? value - 1 : value + 1);
			if (value < settings.encoderReverseLimitPosition) {
				value = settings.encoderReverseLimitPosition;
			}
			if (value > settings.encoderForwardLimitPosition) {
				value = settings.encoderForwardLimitPosition;
			}
			encoderSetValue(value);
		}
		motorProceed();

		if (OCR1B < settings.motorStartSpeed || OCR1B > settings.motorMaxSpeed) {
			violations++;
		}
		if (encoderGetValue() < -100 || encoderGetValue() > 100) {
			violations++;
		}
	}

	motorStop();
	proceedTimes(200);

	TEST_ASSERT_EQ(0, violations);
	TEST_ASSERT(!motorIsMoving());
	TEST_ASSERT_EQ(settings.motorStartSpeed, OCR1B);
	TEST_ASSERT(!pwmEnabled());
}

int main(void) {
	RUN_TEST(initSetsTimerModeAndPins);
	RUN_TEST(startArmsTheEncoder);
	RUN_TEST(startAcceleratesToMaxIn57Steps);
	RUN_TEST(movingTracksThePwmRegisterNotTheTarget);
	RUN_TEST(stopDeceleratesByFiveAndClearsPwm);
	RUN_TEST(reversingWhileMovingSwitchesRelayAfterRampDown);
	RUN_TEST(toggleStartsAndStops);
	RUN_TEST(goToCurrentPositionDoesNotStart);
	RUN_TEST(goToStopsAtItsTargetNotAtTheLimit);
	RUN_TEST(goToClampsTheForwardTarget);
	RUN_TEST(goToClampsTheReverseTarget);
	RUN_TEST(goToReversesFromMotion);
	RUN_TEST(findCenterFromNonZeroStopsOnTheCenterSwitch);
	RUN_TEST(findCenterFromZeroStopsInOneIteration);
	RUN_TEST(fuzzRandomCommandsKeepInvariants);
	return runnerFinish("motor");
}