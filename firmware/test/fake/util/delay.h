#ifndef DOMECONTROL_FAKE_UTIL_DELAY_H
#define DOMECONTROL_FAKE_UTIL_DELAY_H

/*
 * Host stand-in for <util/delay.h>. The 40 ms main-loop window cannot be reproduced
 * on the host (test_plan.md §5.2); the delay is discarded.
 */

#define _delay_ms(milliseconds) ((void)(milliseconds))
#define _delay_us(microseconds) ((void)(microseconds))

#endif /* DOMECONTROL_FAKE_UTIL_DELAY_H */