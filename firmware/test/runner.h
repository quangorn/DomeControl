#ifndef DOMECONTROL_TEST_RUNNER_H
#define DOMECONTROL_TEST_RUNNER_H

/*
 * Mini test runner (test_plan.md §12, §14.1): one executable per suite, no
 * dependencies. A suite lists its cases with RUN_TEST() and ends with runnerFinish(),
 * whose return value becomes the process exit code that ctest reads.
 */

#include <stdio.h>
#include <string.h>

static int runnerChecks = 0;
static int runnerFailures = 0;

#define TEST_ASSERT(condition)                                            \
	do {                                                                  \
		runnerChecks++;                                                   \
		if (!(condition)) {                                               \
			runnerFailures++;                                             \
			printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		}                                                                 \
	} while (0)

#define TEST_ASSERT_EQ(expected, actual)                                      \
	do {                                                                      \
		long long runnerExpected_ = (long long)(expected);                    \
		long long runnerActual_ = (long long)(actual);                        \
		runnerChecks++;                                                       \
		if (runnerExpected_ != runnerActual_) {                               \
			runnerFailures++;                                                 \
			printf("  FAIL %s:%d: %s == %lld, expected %lld\n", __FILE__,     \
			       __LINE__, #actual, runnerActual_, runnerExpected_);        \
		}                                                                     \
	} while (0)

#define TEST_ASSERT_STR_EQ(expected, actual)                                  \
	do {                                                                      \
		const char* runnerExpected_ = (expected);                             \
		const char* runnerActual_ = (actual);                                 \
		runnerChecks++;                                                       \
		if (runnerActual_ == NULL || strcmp(runnerExpected_, runnerActual_) != 0) { \
			runnerFailures++;                                                 \
			printf("  FAIL %s:%d: %s == \"%s\", expected \"%s\"\n", __FILE__, \
			       __LINE__, #actual,                                         \
			       runnerActual_ == NULL ? "(null)" : runnerActual_,          \
			       runnerExpected_);                                          \
		}                                                                     \
	} while (0)

#define TEST_ASSERT_NULL(pointer)                                     \
	do {                                                              \
		runnerChecks++;                                               \
		if ((pointer) != NULL) {                                      \
			runnerFailures++;                                         \
			printf("  FAIL %s:%d: %s is not NULL\n", __FILE__,        \
			       __LINE__, #pointer);                               \
		}                                                             \
	} while (0)

#define TEST_ASSERT_NOT_NULL(pointer)                                 \
	do {                                                              \
		runnerChecks++;                                               \
		if ((pointer) == NULL) {                                      \
			runnerFailures++;                                         \
			printf("  FAIL %s:%d: %s is NULL\n", __FILE__, __LINE__,  \
			       #pointer);                                         \
		}                                                             \
	} while (0)

#define RUN_TEST(test)        \
	do {                      \
		printf("- %s\n", #test); \
		test();               \
	} while (0)

static inline int runnerFinish(const char* suite) {
	printf("%s: %d checks, %d failures\n", suite, runnerChecks, runnerFailures);
	return runnerFailures == 0 ? 0 : 1;
}

#endif /* DOMECONTROL_TEST_RUNNER_H */