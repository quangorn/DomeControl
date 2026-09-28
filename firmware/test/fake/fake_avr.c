#include "fake_avr.h"

/*
 * The fake AVR register file (test_plan.md §5.2). These are the ~25 identifiers the
 * firmware touches besides the pins, as plain RAM: writing them records intent, and
 * nothing here emulates the peripheral.
 */

uint8_t PORTB, DDRB, PINB;
uint8_t PORTC, DDRC, PINC;
uint8_t PORTD, DDRD, PIND;

uint8_t TCCR1A, TCCR1B, OCR1B;

uint8_t MCUCR, GICR;

uint8_t UBRRH, UBRRL, UCSRA, UCSRB, UCSRC, UDR;

void fakeAvrReset(void) {
	PORTB = 0;
	DDRB = 0;
	PINB = 0xFF;
	PORTC = 0;
	DDRC = 0;
	PINC = 0xFF;
	PORTD = 0;
	DDRD = 0;
	PIND = 0xFF;

	TCCR1A = 0;
	TCCR1B = 0;
	OCR1B = 0;

	MCUCR = 0;
	GICR = 0;

	UBRRH = 0;
	UBRRL = 0;
	UCSRA = 0;
	UCSRB = 0;
	UCSRC = 0;
	UDR = 0;
}