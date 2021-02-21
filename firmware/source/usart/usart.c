#include "../definitions.h"
#include "usart.h"
#include <avr/io.h>

#define UART_PRESCALER (F_CPU / 16 / UART_BAUD_RATE - 1)

void usartInit () {
	//Baud Rate speed setting
	UBRRH = UART_PRESCALER >> 8;
	UBRRL = UART_PRESCALER;

	//Enable transmitter and receiver, enable receive interrupt
	UCSRB = (1 << TXEN | 1 << RXEN | 1 << RXCIE);

	//Async mode, 8 data bits, 1 stop bit
	UCSRC = (1 << UCSZ1) | (1 << UCSZ0);
}

uint8_t usartReceiveByte() {
	while (!(UCSRA & (1 << RXC)));
	return UDR;
}

void usartTransmitByte(uint8_t data) {
	while (!(UCSRA & (1 << UDRE)));
	UDR = data;
}

void usartTransmitString(char* str) {
	for (uint16_t i = 0; str[i]; i++) {
		usartTransmitByte(str[i]);
	}
}