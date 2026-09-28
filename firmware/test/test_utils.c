#include "runner.h"

#include "source/common/utils.h"

#include <string.h>

static void checkCommandAcceptsExactAndPrefixedCommands(void) {
	TEST_ASSERT(checkCommand("GOF", "GOF"));
	TEST_ASSERT(checkCommand("GT", "GT-42"));
	TEST_ASSERT(checkCommand("GT", "GT+5"));
}

static void checkCommandRejectsMismatches(void) {
	TEST_ASSERT(!checkCommand("GT", "XGT"));
	TEST_ASSERT(!checkCommand("GOF", "GOR"));
	TEST_ASSERT(!checkCommand("GT", ""));
	TEST_ASSERT(!checkCommand("GT", "G"));
}

static void checkCommandIsCaseSensitive(void) {
	TEST_ASSERT(!checkCommand("gof", "GOF"));
	TEST_ASSERT(!checkCommand("GOF", "gof"));
}

static void checkCommandWithEmptyPrefixMatchesAnything(void) {
	/* Characterisation of utils.c:4-6: an empty expected command matches any command,
	   because strncmp() then compares zero bytes. Nothing can produce it today (no
	   CMD_ string is empty), and the L0 guard keeps it that way. */
	TEST_ASSERT(checkCommand("", "GOF"));
	TEST_ASSERT(checkCommand("", ""));
}

static void parseIntReadsTheGotoPayload(void) {
	TEST_ASSERT_EQ(0, parseInt("0"));
	TEST_ASSERT_EQ(-42, parseInt("-42"));
	TEST_ASSERT_EQ(5, parseInt("+5"));
	TEST_ASSERT_EQ(5, parseInt(" 5"));
	TEST_ASSERT_EQ(0, parseInt("abc"));
	TEST_ASSERT_EQ(12, parseInt("12abc"));
	TEST_ASSERT_EQ(32767, parseInt("32767"));
	TEST_ASSERT_EQ(-32768, parseInt("-32768"));
}

static void printIntWritesExactStrings(void) {
	char buffer[64];

	printInt(0, buffer);
	TEST_ASSERT_STR_EQ("0", buffer);
	printInt(-32768, buffer);
	TEST_ASSERT_STR_EQ("-32768", buffer);
	printInt(32767, buffer);
	TEST_ASSERT_STR_EQ("32767", buffer);
}

static void printIntRoundTripsEveryValue(void) {
	char buffer[64];
	int mismatches = 0;
	int longest = 0;

	for (int value = -32768; value <= 32767; value++) {
		printInt((int16_t)value, buffer);
		int length = (int)strlen(buffer);
		if (length > longest) {
			longest = length;
		}
		if (length > 6 || parseInt(buffer) != value) {
			mismatches++;
		}
	}
	TEST_ASSERT_EQ(0, mismatches);
	TEST_ASSERT_EQ(6, longest); /* -32768 is the longest value: 6 characters */
}

static void printIntStaysInsideItsBuffer(void) {
	static const int16_t values[] = { -32768, -1, 0, 1, 32767 };
	char buffer[64];

	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		memset(buffer, 0x7F, sizeof(buffer));
		printInt(values[i], buffer);
		size_t length = strlen(buffer);
		TEST_ASSERT(length <= 6);
		TEST_ASSERT_EQ(0, buffer[length]);
		for (size_t j = length + 1; j < sizeof(buffer); j++) {
			TEST_ASSERT(buffer[j] == 0x7F);
		}
	}
}

int main(void) {
	RUN_TEST(checkCommandAcceptsExactAndPrefixedCommands);
	RUN_TEST(checkCommandRejectsMismatches);
	RUN_TEST(checkCommandIsCaseSensitive);
	RUN_TEST(checkCommandWithEmptyPrefixMatchesAnything);
	RUN_TEST(parseIntReadsTheGotoPayload);
	RUN_TEST(printIntWritesExactStrings);
	RUN_TEST(printIntRoundTripsEveryValue);
	RUN_TEST(printIntStaysInsideItsBuffer);
	return runnerFinish("utils");
}