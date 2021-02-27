#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

#include <stdbool.h>

#define USART_BAUD_RATE 115200

#define MAX_COMMAND_LENGTH 64
#define MAX_RESPONSE_LENGTH 64
#define SEND_BUFFER_SIZE MAX_RESPONSE_LENGTH

#define END_COMMAND_CHARACTER '#'

#define DIRECTION_FORWARD   false
#define DIRECTION_REVERSE   true

///IO Ports
#define MOTOR_STEP_PORT         B
#define MOTOR_STEP_PIN          2 //timer1 PWM output OC1B
#define MOTOR_DIR_PORT          B
#define MOTOR_DIR_PIN           1

#define BUTTON_FORWARD_PORT     D
#define BUTTON_FORWARD_PIN      3
#define BUTTON_REVERSE_PORT     D
#define BUTTON_REVERSE_PIN      4

#define ENCODER_PORT            D
#define ENCODER_PIN             2 //external interrupt INT0

#define LED_PORT                B
#define LED_PIN                 5


///Commands
const char* CMD_GO_FORWARD;
const char* CMD_GO_REVERSE;
const char* CMD_STOP;


///Responses
const char* RESP_GO_FORWARD_OK;
const char* RESP_GO_REVERSE_OK;
const char* RESP_STOP_OK;

#endif /* DEFINITIONS_H_ */