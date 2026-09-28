#include "runner.h"

#include "fake/fake_avr.h"

#include "source/buttons/buttons.h"
#include "source/common/definitions.h"

static void resetWorld(void) {
	fakeAvrReset();
	buttonsInit();
	/* Sync the edge detectors with the released inputs set by fakeAvrReset(). */
	buttonsIsForwardPressed();
	buttonsIsReversePressed();
}

static void initEnablesPullUps(void) {
	fakeAvrReset();
	buttonsInit();

	TEST_ASSERT(PORTD & (1 << BUTTON_FORWARD_PIN));
	TEST_ASSERT(PORTD & (1 << BUTTON_REVERSE_PIN));
}

static void pressIsReportedOnTheTransitionOnly(void) {
	resetWorld();

	TEST_ASSERT(!buttonsIsForwardPressed());
	TEST_ASSERT(!buttonsIsForwardPressed());

	fakePinPress(&PIND, BUTTON_FORWARD_PIN);
	TEST_ASSERT(buttonsIsForwardPressed());
	TEST_ASSERT(!buttonsIsForwardPressed()); /* held down: no repeat */

	fakePinRelease(&PIND, BUTTON_FORWARD_PIN);
	TEST_ASSERT(!buttonsIsForwardPressed());

	fakePinPress(&PIND, BUTTON_FORWARD_PIN);
	TEST_ASSERT(buttonsIsForwardPressed());
}

static void pressedButtonReadsLow(void) {
	resetWorld();

	fakePinPress(&PIND, BUTTON_FORWARD_PIN);
	TEST_ASSERT(!(PIND & (1 << BUTTON_FORWARD_PIN)));

	fakePinRelease(&PIND, BUTTON_FORWARD_PIN);
	TEST_ASSERT(PIND & (1 << BUTTON_FORWARD_PIN));
}

static void buttonsAreIndependent(void) {
	resetWorld();

	fakePinPress(&PIND, BUTTON_FORWARD_PIN);
	TEST_ASSERT(buttonsIsForwardPressed());
	TEST_ASSERT(!buttonsIsReversePressed());

	fakePinPress(&PIND, BUTTON_REVERSE_PIN);
	TEST_ASSERT(!buttonsIsForwardPressed()); /* still held, already consumed */
	TEST_ASSERT(buttonsIsReversePressed());

	fakePinRelease(&PIND, BUTTON_FORWARD_PIN);
	fakePinRelease(&PIND, BUTTON_REVERSE_PIN);
	TEST_ASSERT(!buttonsIsForwardPressed());
	TEST_ASSERT(!buttonsIsReversePressed());
}

int main(void) {
	RUN_TEST(initEnablesPullUps);
	RUN_TEST(pressIsReportedOnTheTransitionOnly);
	RUN_TEST(pressedButtonReadsLow);
	RUN_TEST(buttonsAreIndependent);
	return runnerFinish("buttons");
}