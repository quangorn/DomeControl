#include "runner.h"

#include "fake/fake_avr.h"

#include "source/common/definitions.h"
#include "source/encoder/encoder.h"
#include "source/settings/settings.h"

static void resetWorld(void) {
	fakeAvrReset();
	settingsInitDefault();
	encoderInit();
	encoderDisableCounting();
	encoderSetValue(0);
}

static void initConfiguresInt0ForARisingEdge(void) {
	fakeAvrReset();
	encoderInit();

	TEST_ASSERT(MCUCR & (1 << ISC00));
	TEST_ASSERT(MCUCR & (1 << ISC01));
	TEST_ASSERT(GICR & (1 << INT0));
}

static void disabledCountingIgnoresPulses(void) {
	resetWorld();
	encoderSetValue(7);
	encoderDisableCounting();

	INT0_vect();
	INT0_vect();

	TEST_ASSERT_EQ(7, encoderGetValue());
}

static void forwardPulsesIncrement(void) {
	resetWorld();
	encoderEnableCounting(DIRECTION_FORWARD);

	INT0_vect();
	INT0_vect();
	INT0_vect();

	TEST_ASSERT_EQ(3, encoderGetValue());
}

static void reversePulsesDecrement(void) {
	resetWorld();
	encoderEnableCounting(DIRECTION_REVERSE);

	INT0_vect();
	INT0_vect();

	TEST_ASSERT_EQ(-2, encoderGetValue());
}

static void directionCanBeChangedWhileCounting(void) {
	resetWorld();
	encoderEnableCounting(DIRECTION_FORWARD);
	INT0_vect();

	encoderEnableCounting(DIRECTION_REVERSE);
	INT0_vect();
	INT0_vect();

	TEST_ASSERT_EQ(-1, encoderGetValue());
}

static void disableStopsCounting(void) {
	resetWorld();
	encoderEnableCounting(DIRECTION_FORWARD);
	INT0_vect();

	encoderDisableCounting();
	INT0_vect();
	INT0_vect();

	TEST_ASSERT_EQ(1, encoderGetValue());
}

static void limitPositionsComeFromSettings(void) {
	resetWorld();

	TEST_ASSERT_EQ(100, encoderGetForwardLimitPosition());
	TEST_ASSERT_EQ(-100, encoderGetReverseLimitPosition());
	TEST_ASSERT_EQ(0, encoderGetCenterPosition());

	settings.encoderForwardLimitPosition = 42;
	settings.encoderReverseLimitPosition = -42;
	TEST_ASSERT_EQ(42, encoderGetForwardLimitPosition());
	TEST_ASSERT_EQ(-42, encoderGetReverseLimitPosition());
}

static void anchoringSettersJumpToTheirPositions(void) {
	resetWorld();
	encoderSetValue(13);

	encoderSetForwardLimitPosition();
	TEST_ASSERT_EQ(100, encoderGetValue());

	encoderSetValue(13);
	encoderSetReverseLimitPosition();
	TEST_ASSERT_EQ(-100, encoderGetValue());

	encoderSetValue(13);
	encoderSetCenterPosition();
	TEST_ASSERT_EQ(0, encoderGetValue());
}

static void int16BoundsAreNotClamped(void) {
	resetWorld();

	encoderSetValue(32767);
	TEST_ASSERT_EQ(32767, encoderGetValue());

	encoderSetValue(-32768);
	TEST_ASSERT_EQ(-32768, encoderGetValue());

	/* A pulse at the boundary overflows int16_t: undefined behaviour, recorded as a
	   limitation of the module (test_plan.md §13), not asserted as behaviour. */
}

int main(void) {
	RUN_TEST(initConfiguresInt0ForARisingEdge);
	RUN_TEST(disabledCountingIgnoresPulses);
	RUN_TEST(forwardPulsesIncrement);
	RUN_TEST(reversePulsesDecrement);
	RUN_TEST(directionCanBeChangedWhileCounting);
	RUN_TEST(disableStopsCounting);
	RUN_TEST(limitPositionsComeFromSettings);
	RUN_TEST(anchoringSettersJumpToTheirPositions);
	RUN_TEST(int16BoundsAreNotClamped);
	return runnerFinish("encoder");
}