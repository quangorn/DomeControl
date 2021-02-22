#include "source/common/definitions.h"
#include "usart.h"
#include <stddef.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#define UART_PRESCALER (F_CPU / 8 / UART_BAUD_RATE - 1)

char usartSendBuffer[SEND_BUFFER_SIZE];
volatile uint8_t usartSendPos = 0;
volatile uint8_t usartSendBufferEndPos = 0;
volatile bool usartSendIsActive = false;

char usartReceiveBuffer[MAX_COMMAND_LENGTH];
volatile uint8_t usartReceivePos = 0;
volatile bool usartCommandReceived = false;

void usartInit () {
	//Baud Rate speed setting
	UBRRH = UART_PRESCALER >> 8;
	UBRRL = UART_PRESCALER;

	//Double transmission speed
	UCSRA |= 1 << U2X;

	//Enable transmitter and receiver, enable receive ant transmit interrupt
	UCSRB = 1 << TXEN | 1 << RXEN | 1 << RXCIE | 1 << TXCIE;

	//Selecting UCSRC register, async mode, 8 data bits, 1 stop bit
	UCSRC = 1 << URSEL | 1 << UCSZ1 | 1 << UCSZ0;
}

void usartSendNextByte() {
	UDR = usartSendBuffer[usartSendPos];
}

void usartPrint(const char* str) {
	for (uint8_t i = 0; str[i]; i++) {
		//Make sure not to overwrite data in usartSendBuffer, waiting usartSendPos increment if needed
		while ((usartSendPos - usartSendBufferEndPos) % sizeof(usartSendBuffer) == 1);
		usartSendBuffer[usartSendBufferEndPos] = str[i];

		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			//Starting send if not already started
			if (!usartSendIsActive) {
				usartSendNextByte();
				usartSendIsActive = true;
			}
			usartSendBufferEndPos = (usartSendBufferEndPos + 1) % sizeof(usartSendBuffer);
		}
	}
}

void usartPrintln(const char* str) {
	usartPrint(str);
	usartPrint("\r\n");
}

const char* usartGetReceivedCommand() {
	if (!usartCommandReceived) {
		return NULL;
	}
	usartCommandReceived = false;
	return usartReceiveBuffer;
}

//======= Interrupt vectors =======
ISR (USART_TXC_vect) {
	usartSendPos = (usartSendPos + 1) % sizeof(usartSendBuffer);
	if (usartSendPos != usartSendBufferEndPos) {
		usartSendNextByte();
	} else {
		usartSendIsActive = false;
	}
}

ISR (USART_RXC_vect) {
	int8_t received = UDR;
	usartReceiveBuffer[usartReceivePos] = received;
	usartReceivePos = (usartReceivePos + 1) % sizeof(usartReceiveBuffer);
	if (received == END_COMMAND_CHARACTER) {
		usartReceiveBuffer[usartReceivePos] = '\0';
		usartReceivePos = 0;
		usartCommandReceived = true;
	}
}
