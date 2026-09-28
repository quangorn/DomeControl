#ifndef DOMECONTROL_FAKE_AVR_INTERRUPT_H
#define DOMECONTROL_FAKE_AVR_INTERRUPT_H

/*
 * Host stand-in for <avr/interrupt.h>: ISR(vect) becomes an ordinary function that
 * a test calls by hand, and sei()/cli() are no-ops because the host has no interrupt
 * state to change.
 */

#define ISR(vector) void vector(void)

static inline void sei(void) {}
static inline void cli(void) {}

#endif /* DOMECONTROL_FAKE_AVR_INTERRUPT_H */