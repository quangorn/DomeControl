#include "runner.h"

#include "fake/fake_avr.h"

#include "source/common/definitions.h"
#include "source/encoder/encoder.h"
#include "source/limits/limits.h"
#include "source/settings/settings.h"

static void resetWorld(void) {
	fakeAvrReset();
	settingsInitDefault();
	encoderInit();
	limitsInit();
	encoderDisableCounting();
	encoderSetValue(0);
	/* Sync the edge detector with the released inputs set by fakeAvrReset(). */
	limitsProceed();
}

static void initEnablesPullUps(void) {
	fakeAvrReset();
	limitsInit();

	TEST_ASSERT(PORTC & (1 << LIMIT_FORWARD_PIN));
	TEST_ASSERT(PORTC & (1 << LIMIT_REVERSE_PIN));
	TEST_ASSERT(PORTC & (1 << LIMIT_CENTER_PIN));
}

static void limitInputsAreActiveLow(void) {
	resetWorld();

	fakePinPress(&PINC, LIMIT_FORWARD_PIN);
	TEST_ASSERT(limitsIsForwardLimitReached());
	fakePinRelease(&PINC, LIMIT_FORWARD_PIN);
	TEST_ASSERT(!limitsIsForwardLimitReached());

	fakePinPress(&PINC, LIMIT_REVERSE_PIN);
	TEST_ASSERT(limitsIsReverseLimitReached());
	fakePinRelease(&PINC, LIMIT_REVERSE_PIN);
	TEST_ASSERT(!limitsIsReverseLimitReached());

	fakePinPress(&PINC, LIMIT_CENTER_PIN);
	TEST_ASSERT(limitsIsOnCenter());
	fakePinRelease(&PINC, LIMIT_CENTER_PIN);
	TEST_ASSERT(!limitsIsOnCenter());
}

static void forwardLimitAnchorsOnTheRisingEdgeOnly(void) {
	resetWorld();
	encoderSetValue(50);

	fakePinPress(&PINC, LIMIT_FORWARD_PIN);
	limitsProceed();
	TEST_ASSERT_EQ(100, encoderGetValue());

	/* While the switch is held, a changed encoder value is not re-anchored: this is
	   edge, not level, detection, and it is what lets the dome count "through" the
	   limit switch. */
	encoderSetValue(70);
	limitsProceed();
	TEST_ASSERT_EQ(70, encoderGetValue());

	fakePinRelease(&PINC, LIMIT_FORWARD_PIN);
	limitsProceed();
	encoderSetValue(80);

	fakePinPress(&PINC, LIMIT_FORWARD_PIN);
	limitsProceed();
	TEST_ASSERT_EQ(100, encoderGetValue());
}

static void reverseLimitAnchorsOnTheRisingEdgeOnly(void) {
	resetWorld();
	encoderSetValue(50);

	fakePinPress(&PINC, LIMIT_REVERSE_PIN);
	limitsProceed();
	TEST_ASSERT_EQ(-100, encoderGetValue());

	encoderSetValue(-70);
	limitsProceed();
	TEST_ASSERT_EQ(-70, encoderGetValue());

	fakePinRelease(&PINC, LIMIT_REVERSE_PIN);
	limitsProceed();
	encoderSetValue(-80);

	fakePinPress(&PINC, LIMIT_REVERSE_PIN);
	limitsProceed();
	TEST_ASSERT_EQ(-100, encoderGetValue());
}

static void centerLimitAnchorsOnTheRisingEdgeOnly(void) {
	resetWorld();
	encoderSetValue(50);

	fakePinPress(&PINC, LIMIT_CENTER_PIN);
	limitsProceed();
	TEST_ASSERT_EQ(0, encoderGetValue());

	encoderSetValue(30);
	limitsProceed();
	TEST_ASSERT_EQ(30, encoderGetValue());

	fakePinRelease(&PINC, LIMIT_CENTER_PIN);
	limitsProceed();
	encoderSetValue(20);

	fakePinPress(&PINC, LIMIT_CENTER_PIN);
	limitsProceed();
	TEST_ASSERT_EQ(0, encoderGetValue());
}

static void sensorsAreIndependent(void) {
	resetWorld();
	encoderSetValue(50);

	fakePinPress(&PINC, LIMIT_CENTER_PIN);
	limitsProceed();

	TEST_ASSERT_EQ(0, encoderGetValue());
	TEST_ASSERT(limitsIsOnCenter());
	TEST_ASSERT(!limitsIsForwardLimitReached());
	TEST_ASSERT(!limitsIsReverseLimitReached());
}

static void countingContinuesFromTheAnchor(void) {
	resetWorld();
	encoderSetValue(50);
	encoderEnableCounting(DIRECTION_FORWARD);

	fakePinPress(&PINC, LIMIT_FORWARD_PIN);
	limitsProceed();
	TEST_ASSERT_EQ(100, encoderGetValue());

	INT0_vect();
	TEST_ASSERT_EQ(101, encoderGetValue());
}

int main(void) {
	RUN_TEST(initEnablesPullUps);
	RUN_TEST(limitInputsAreActiveLow);
	RUN_TEST(forwardLimitAnchorsOnTheRisingEdgeOnly);
	RUN_TEST(reverseLimitAnchorsOnTheRisingEdgeOnly);
	RUN_TEST(centerLimitAnchorsOnTheRisingEdgeOnly);
	RUN_TEST(sensorsAreIndependent);
	RUN_TEST(countingContinuesFromTheAnchor);
	return runnerFinish("limits");
}