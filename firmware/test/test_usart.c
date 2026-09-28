#include "runner.h"

#include "fake/fake_avr.h"

#include "source/common/definitions.h"

/*
 * The USART suite needs the module's static send state (usartSendIsActive, the buffer
 * positions), so it compiles usart.c into this translation unit instead of linking it
 * (test_plan.md §5.3). Nothing in usart.c is changed for the test.
 */
#include "source/usart/usart.c"

#include <string.h>

static char txStream[512];
static size_t txLength;

static void txReset(void) {
	txLength = 0;
	txStream[0] = '\0';
}

/*
 * usart.c keeps its state in static variables that a real MCU initialises once at
 * reset. This process runs several tests, so the suite resets them explicitly; the
 * production module is not changed for the test.
 */
static void usartResetModule(void) {
	usartSendPos = 0;
	usartSendBufferEndPos = 0;
	usartSendIsActive = false;
	usartReceivePos = 0;
	usartCommandReceived = false;
}

static void prepareUsart(void) {
	fakeAvrReset();
	usartResetModule();
	usartInit();
	txReset();
}

/*
 * Drain the transmitter the way the AVR would: UDR holds the byte currently on the
 * wire, and USART_TXC_vect() advances to the next one until the module clears
 * usartSendIsActive (test_plan.md §5.2).
 */
static void txPumpAll(void) {
	while (usartSendIsActive) {
		TEST_ASSERT(txLength + 1 < sizeof(txStream));
		txStream[txLength++] = (char)UDR;
		USART_TXC_vect();
	}
	txStream[txLength] = '\0';
}

static void rxFeed(const char* bytes) {
	for (const char* byte = bytes; *byte; byte++) {
		UDR = (uint8_t)*byte;
		USART_RXC_vect();
	}
}

static void initConfiguresTheUsart(void) {
	prepareUsart();

	TEST_ASSERT_EQ(12, USART_PRESCALER); /* 12 MHz / 8 / 115200 - 1, double speed */
	TEST_ASSERT_EQ(USART_PRESCALER >> 8, UBRRH);
	TEST_ASSERT_EQ(USART_PRESCALER & 0xFF, UBRRL);
	TEST_ASSERT(UCSRA & (1 << U2X));
	TEST_ASSERT_EQ((1 << TXEN) | (1 << RXEN) | (1 << RXCIE) | (1 << TXCIE), UCSRB);
	TEST_ASSERT_EQ((1 << URSEL) | (1 << UCSZ1) | (1 << UCSZ0), UCSRC);
}

static void printlnAppendsCarriageReturnLineFeed(void) {
	prepareUsart();

	usartPrintln("OK");
	TEST_ASSERT(usartSendIsActive);
	txPumpAll();
	TEST_ASSERT(!usartSendIsActive);
	TEST_ASSERT_STR_EQ("OK\r\n", txStream);
}

static void printKeepsBytesInOrder(void) {
	prepareUsart();

	usartPrint("GT-42");
	txPumpAll();

	TEST_ASSERT_STR_EQ("GT-42", txStream);
}

static void receivedCommandIsReturnedExactlyOnce(void) {
	prepareUsart();

	TEST_ASSERT_NULL(usartGetReceivedCommand());

	/* The receive buffer keeps the terminating '#': the ISR stores it and puts the
	   NUL after it (usart.c:83-87), so the command string is the whole frame. */
	rxFeed("GT-42#");
	TEST_ASSERT_STR_EQ("GT-42#", usartGetReceivedCommand());
	TEST_ASSERT_NULL(usartGetReceivedCommand());
}

static void feedFrameWithBody(size_t length, const char* expected) {
	char body[MAX_COMMAND_LENGTH + 1];

	for (size_t i = 0; i < length; i++) {
		body[i] = (char)('a' + (int)(i % 26));
	}
	body[length] = '\0';

	prepareUsart();
	rxFeed(body);
	rxFeed("#");

	const char* command = usartGetReceivedCommand();
	TEST_ASSERT_NOT_NULL(command);
	TEST_ASSERT_STR_EQ(expected, command);
}

static void usableFrameBodyIs62Bytes(void) {
	/* 62 payload bytes + '#' + NUL fill the 64-byte buffer exactly. */
	char expected[64];

	for (size_t i = 0; i < 62; i++) {
		expected[i] = (char)('a' + (int)(i % 26));
	}
	expected[62] = '#';
	expected[63] = '\0';

	feedFrameWithBody(62, expected);
}

static void frameBodyOf63BytesBecomesEmpty(void) {
	/* test_plan.md §10.1: the '#' lands on the last buffer slot, usartReceivePos wraps
	   to 0 and buffer[0] = '\0' wipes the command. Usable length is 62, not 64. */
	feedFrameWithBody(63, "");
}

static void frameBodyOf64BytesOverwritesTheStart(void) {
	/* The '#' wraps to index 0 and the terminator to index 1: only "#" survives. */
	feedFrameWithBody(64, "#");
}

static void twoCommandsInOnePacketKeepTheSecond(void) {
	/* test_plan.md §10.2: there is no queue and no delimiter check, so the first
	   command is silently overwritten before main() can read it. */
	prepareUsart();

	rxFeed("ST#GEV#");
	TEST_ASSERT_STR_EQ("GEV#", usartGetReceivedCommand());
	TEST_ASSERT_NULL(usartGetReceivedCommand());
}

static void chunksUpTo63BytesStayInOrder(void) {
	char chunk[64];

	memset(chunk, 'x', 63);
	chunk[63] = '\0';

	prepareUsart();

	usartPrint(chunk);
	txPumpAll();
	usartPrint(chunk);
	txPumpAll();

	TEST_ASSERT_EQ(126, txLength);
	int mismatches = 0;
	for (size_t i = 0; i < txLength; i++) {
		if (txStream[i] != 'x') {
			mismatches++;
		}
	}
	TEST_ASSERT_EQ(0, mismatches);
}

static void sendPositionsWrapAround(void) {
	char body[64];
	char expected[64];

	for (int i = 0; i < 63; i++) {
		body[i] = (char)('A' + (i % 26));
	}
	body[63] = '\0';

	prepareUsart();

	usartPrint(body); /* 63 writes: exactly the usable payload of the send buffer */
	TEST_ASSERT_EQ(0, usartSendPos);
	TEST_ASSERT_EQ(63, usartSendBufferEndPos);

	USART_TXC_vect();
	TEST_ASSERT_EQ(1, usartSendPos);

	usartPrint("Z");
	TEST_ASSERT_EQ(0, usartSendBufferEndPos); /* endPos wrapped 63 -> 0 */

	/* body[0] reached UDR before the pump started, so the recorded stream is
	   body[1..62] followed by the byte written into the wrapped slot 63. */
	memcpy(expected, body + 1, 62);
	expected[62] = 'Z';
	expected[63] = '\0';

	txReset();
	txPumpAll();
	TEST_ASSERT_STR_EQ(expected, txStream);
}

int main(void) {
	RUN_TEST(initConfiguresTheUsart);
	RUN_TEST(printlnAppendsCarriageReturnLineFeed);
	RUN_TEST(printKeepsBytesInOrder);
	RUN_TEST(receivedCommandIsReturnedExactlyOnce);
	RUN_TEST(usableFrameBodyIs62Bytes);
	RUN_TEST(frameBodyOf63BytesBecomesEmpty);
	RUN_TEST(frameBodyOf64BytesOverwritesTheStart);
	RUN_TEST(twoCommandsInOnePacketKeepTheSecond);
	RUN_TEST(chunksUpTo63BytesStayInOrder);
	RUN_TEST(sendPositionsWrapAround);
	return runnerFinish("usart");
}