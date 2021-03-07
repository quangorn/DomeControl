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

#define ENCODER_CENTER_POSITION 0

///IO Ports
#define MOTOR_STEP_PORT             B
#define MOTOR_STEP_PIN              2 //timer1 PWM output OC1B
#define MOTOR_DIR_PORT              B
#define MOTOR_DIR_PIN               1

#define BUTTON_FORWARD_PORT         D
#define BUTTON_FORWARD_PIN          3
#define BUTTON_REVERSE_PORT         D
#define BUTTON_REVERSE_PIN          4

#define ENCODER_PORT                D
#define ENCODER_PIN                 2 //external interrupt INT0

#define LED_PORT                    B
#define LED_PIN                     5

#define LIMIT_FORWARD_PORT          C
#define LIMIT_FORWARD_PIN           0
#define LIMIT_REVERSE_PORT          C
#define LIMIT_REVERSE_PIN           1
#define LIMIT_CENTER_PORT           C
#define LIMIT_CENTER_PIN            2


///Commands
extern const char* CMD_GO_FORWARD;
extern const char* CMD_GO_REVERSE;
extern const char* CMD_STOP;
extern const char* CMD_GOTO;
extern const char* CMD_GET_ENCODER_VALUE;
extern const char* CMD_IS_ON_CENTER;
extern const char* CMD_IS_MOVING;
extern const char* CMD_FIND_CENTER;


///Responses
extern const char* RESP_OK;

#endif /* DEFINITIONS_H_ */