#ifndef DOMECONTROL_FAKE_AVR_H
#define DOMECONTROL_FAKE_AVR_H

/*
 * Test-facing API of the fake AVR layer (test_plan.md §5.1).
 *
 * Everything else the tests need is the register set from fake/avr/io.h, which the
 * production sources already use through OUTPORT/INPORT/DDRPORT or directly.
 */

#include <stdint.h>

/* The register file and its bit positions. */
#include <avr/io.h>

/* Interrupt vectors exported by the firmware under test. */
void INT0_vect(void);
void USART_RXC_vect(void);
void USART_TXC_vect(void);

/*
 * Zero every fake register and let PINB/PINC/PIND read 0xFF, i.e. "active-low inputs
 * released, pull-ups holding them high". Module state in static variables is not
 * touched: a suite resets that through the module's own init/stop functions.
 */
void fakeAvrReset(void);

/* Active-low input helpers: press pulls the pin low, release lets the pull-up win. */
static inline void fakePinPress(uint8_t* pin, uint8_t bit) {
	*pin &= (uint8_t)~(1u << bit);
}

static inline void fakePinRelease(uint8_t* pin, uint8_t bit) {
	*pin |= (uint8_t)(1u << bit);
}

#endif /* DOMECONTROL_FAKE_AVR_H */