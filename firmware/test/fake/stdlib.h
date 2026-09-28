#ifndef DOMECONTROL_FAKE_STDLIB_H
#define DOMECONTROL_FAKE_STDLIB_H

/*
 * avr-libc declares itoa() in <stdlib.h>; glibc does not, so firmware/source/common/
 * utils.c does not compile against the host headers (test_plan.md §5.1). Declare it
 * here, take everything else from the real header, and implement it in fake_stdlib.c.
 */

#include_next <stdlib.h>

char* itoa(int value, char* str, int base);

#endif /* DOMECONTROL_FAKE_STDLIB_H */