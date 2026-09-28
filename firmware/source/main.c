#include "buttons/buttons.h"
#include "common/utils.h"
#include "common/definitions.h"
#include "encoder/encoder.h"
#include "led/led.h"
#include "limits/limits.h"
#include "motor/motor.h"
#include "settings/settings.h"
#include "usart/usart.h"
#include <avr/interrupt.h>
#include <util/delay.h>

static void processCommand(const char* cmd) {
	char buf[MAX_RESPONSE_LENGTH];

	if (checkCommand(CMD_GO_FORWARD, cmd)) {
		motorStart(DIRECTION_FORWARD);
		usartPrintln(RESP_OK);
	} else if (checkCommand(CMD_GO_REVERSE, cmd)) {
		motorStart(DIRECTION_REVERSE);
		usartPrintln(RESP_OK);
	} else if (checkCommand(CMD_STOP, cmd)) {
		motorStop();
		usartPrintln(RESP_OK);
	} else if (checkCommand(CMD_GOTO, cmd)) {
		int16_t position = parseInt(cmd + strlen(CMD_GOTO));
		motorGoTo(position);
#ifdef DEBUG
		usartPrint("Go to: ");
		printInt(position, buf);
		usartPrintln(buf);
#endif
		usartPrintln(RESP_OK);
	} else if (checkCommand(CMD_GET_ENCODER_VALUE, cmd)) {
		printInt(encoderGetValue(), buf);
		usartPrintln(buf);
	} else if (checkCommand(CMD_IS_ON_CENTER, cmd)) {
		printInt(limitsIsOnCenter(), buf);
		usartPrintln(buf);
	} else if (checkCommand(CMD_IS_MOVING, cmd)) {
		printInt(motorIsMoving(), buf);
		usartPrintln(buf);
	} else if (checkCommand(CMD_FIND_CENTER, cmd)) {
		motorFindCenter();
		usartPrintln(RESP_OK);
	} else {
		usartPrint("Unrecognized command: ");
		usartPrintln(cmd);
	}
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main (void) {

#ifdef DEBUG
	int16_t lastEncoderValue = 0;
	char buf[MAX_RESPONSE_LENGTH];
#endif

	settingsInitDefault();

	buttonsInit();
	encoderInit();
	ledInit();
	limitsInit();
	motorInit();
	usartInit();

	sei();

	while (1) {
		//Такт главного цикла. Это НЕ просто задержка: motorProceed() вызывается раз за итерацию,
		//и рампa OCR1B начисляется именно за вызов, поэтому задержка = скорость разгона
		//(замерено под simavr: 15 -> 128 за 0.377 с; без задержки 0.007 с, что сорвёт шаги).
		//Убирать только вместе с переводом рампы на реальное время (Timer0).
		_delay_ms(40);
		//ledToggle();

		const char* cmd = usartGetReceivedCommand();
		if (cmd) {
			processCommand(cmd);
		}

		if (buttonsIsForwardPressed()) {
			motorToggle(DIRECTION_FORWARD);
#ifdef DEBUG
			usartPrintln("Forward!");
#endif
		} else if (buttonsIsReversePressed()) {
			motorToggle(DIRECTION_REVERSE);
#ifdef DEBUG
			usartPrintln("Reverse!");
#endif
		}

#ifdef DEBUG
		int16_t encoderValue = encoderGetValue();
		if (encoderValue != lastEncoderValue) {
			lastEncoderValue = encoderValue;
			usartPrint("Encoder value: ");
			printInt(encoderValue, buf);
			usartPrintln(buf);
		}
#endif

		limitsProceed();
		motorProceed();
	}
}
#pragma clang diagnostic pop
