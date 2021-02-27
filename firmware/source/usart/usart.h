#ifndef USART_H_
#define USART_H_

#include <stdint.h>

void usartInit ();

void usartPrint(const char* str);
void usartPrintln(const char* str);
const char* usartGetReceivedCommand();

#endif /* USART_H_ */