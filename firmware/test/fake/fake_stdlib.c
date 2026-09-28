#include "stdlib.h"

/*
 * itoa() for the host (test_plan.md §5.1). printInt() only ever asks for base 10;
 * other bases are not implemented and fall back to decimal.
 */
char* itoa(int value, char* str, int base) {
	char* position = str;
	unsigned int magnitude;

	(void)base;

	if (value < 0) {
		*position++ = '-';
		magnitude = (unsigned int)(-(long)value);
	} else {
		magnitude = (unsigned int)value;
	}

	char digits[12];
	int count = 0;
	do {
		digits[count++] = (char)('0' + magnitude % 10u);
		magnitude /= 10u;
	} while (magnitude != 0);

	while (count > 0) {
		*position++ = digits[--count];
	}
	*position = '\0';
	return str;
}