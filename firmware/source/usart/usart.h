#ifndef UART_H_
#define UART_H_

#include <stdint.h>

void usartInit ();

uint8_t usartReceiveByte();

void usartTransmitByte(uint8_t data);

void usartTransmitString(char* str);

#endif /* UART_H_ */