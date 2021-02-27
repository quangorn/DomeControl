#include "led.h"
#include "source/common/utils.h"
#include "source/common/definitions.h"
#include <avr/io.h>

void ledInit() {
	//set pins to output mode
	DDRPORT(LED_PORT) |= 1 << LED_PIN;
}

void ledOn() {
	OUTPORT(LED_PORT) |= 1 << LED_PIN;
}

void ledOff() {
	OUTPORT(LED_PORT) &= ~(1 << LED_PIN);
}

void ledToggle() {
	OUTPORT(LED_PORT) ^= 1 << LED_PIN;
}
