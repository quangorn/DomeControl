#ifndef DOMECONTROL_FAKE_AVR_IO_H
#define DOMECONTROL_FAKE_AVR_IO_H

/*
 * Host stand-in for <avr/io.h> (test_plan.md §5.1).
 *
 * The firmware is compiled unchanged: this header is found first because the test
 * build adds firmware/test/fake to the include path. Registers become ordinary RAM
 * variables (defined in fake_avr.c), so a test observes intent -- which bit was
 * written -- and never the AVR peripheral itself.
 *
 * Bit positions follow the ATmega8 datasheet. motor.c writes the Timer0 prescaler
 * names CS00/CS02 into TCCR1B; on the ATmega8 they are numerically equal to
 * CS10/CS12 (test_plan.md §10.9), so both spellings are provided.
 */

#include <stdint.h>

/* I/O ports */
extern uint8_t PORTB, DDRB, PINB;
extern uint8_t PORTC, DDRC, PINC;
extern uint8_t PORTD, DDRD, PIND;

/* Timer1 (OC1B is the motor step output) */
extern uint8_t TCCR1A, TCCR1B, OCR1B;

/* External interrupt INT0 (encoder) */
extern uint8_t MCUCR, GICR;

/* USART */
extern uint8_t UBRRH, UBRRL, UCSRA, UCSRB, UCSRC, UDR;

/* Timer1 bits */
#define WGM10 0
#define WGM11 1
#define COM1B1 5

/* Prescaler bits as named by motor.c (Timer0 names, Timer1 positions) */
#define CS00 0
#define CS02 2

/* MCUCR and GICR bits */
#define ISC00 0
#define ISC01 1
#define INT0 6

/* UCSRA bits */
#define U2X 1

/* UCSRB bits */
#define TXEN 3
#define RXEN 4
#define TXCIE 6
#define RXCIE 7

/* UCSRC bits */
#define UCSZ0 1
#define UCSZ1 2
#define URSEL 7

#endif /* DOMECONTROL_FAKE_AVR_IO_H */