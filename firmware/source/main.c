
#include "definitions.h"
#include "motor/motor.h"
#include "usart/usart.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

void ledOn() {
	PORTB |= 1 << PORTB5;
}

void ledOff() {
	PORTB &= ~(1 << PORTB5);
}

void ledToggle() {
	PORTB ^= 1 << PORTB5;
}

char buf[256];

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main (void) {
	//B5 is output
	DDRB |= 1 << DDB5;

	// PD2 and PD3 are buttons with pull up
	PORTD |= (1 << PORTD2) | (1 << PORTD3);

	//motorInit();
	//usartInit ();

	//sei();

	//uint8_t lastButtonsState = 0;
	while (1) {
		_delay_ms(1000);
		ledToggle();

//		uint8_t buttonsState = PIND & ((1 << PIND2) | (1 << PIND3));
//		if (buttonsState != lastButtonsState) {
//			if (!(PIND & (1 << PIND2))) {
//				motorToggle(true);
//			} else if (!(PIND & (1 << PIND3))) {
//				motorToggle(false);
//			}
//			lastButtonsState = buttonsState;
//		}
//
//		motorProceed();
	}
}
#pragma clang diagnostic pop

//ISR (USART_RX_vect) {
	//int8_t received = usartReceiveByte();
	//if (received == 'a') {
	//toggleLED();
	//}
	//usartTransmitByte(received);
//}

