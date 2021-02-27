#include "buttons/buttons.h"
#include "led/led.h"
#include "motor/motor.h"
#include "usart/usart.h"
#include "common/utils.h"
#include "common/definitions.h"
#include <avr/interrupt.h>
#include <util/delay.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main (void) {
	ledInit();
	buttonsInit();
	motorInit();
	usartInit();

	sei();

	while (1) {
		//TODO: убрать delay, чтобы не пропустить команду по usart
		_delay_ms(40);
		//ledToggle();

		const char* cmd = usartGetReceivedCommand();
		if (cmd) {
			if (checkCommand(CMD_GO_FORWARD, cmd)) {
				motorStart(true);
				usartPrintln(RESP_GO_FORWARD_OK);
			} else if (checkCommand(CMD_GO_REVERSE, cmd)) {
				motorStart(false);
				usartPrintln(RESP_GO_REVERSE_OK);
			} else if (checkCommand(CMD_STOP, cmd)) {
				motorStop();
				usartPrintln(RESP_STOP_OK);
			} else {
				usartPrint("Unrecognized command: ");
				usartPrint(cmd);
			}
		}

		if (buttonsIsForwardPressed()) {
			motorToggle(true);
#ifdef DEBUG
			usartPrintln("Forward!");
#endif
		} else if (buttonsIsReversePressed()) {
			motorToggle(false);
#ifdef DEBUG
			usartPrintln("Reverse!");
#endif
		}

		motorProceed();
	}
}
#pragma clang diagnostic pop
