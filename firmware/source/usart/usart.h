#ifndef UART_H_
#define UART_H_

#include <stdint.h>

void usartInit ();

void usartPrint(const char* str);
void usartPrintln(const char* str);
const char* usartGetReceivedCommand();

#endif /* UART_H_ */