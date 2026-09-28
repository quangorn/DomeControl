#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

#include <stdbool.h>

#define USART_BAUD_RATE 115200

#define MAX_COMMAND_LENGTH 64
#define MAX_RESPONSE_LENGTH 64
#define SEND_BUFFER_SIZE MAX_RESPONSE_LENGTH

#define END_COMMAND_CHARACTER '#'

//An enum, not a pair of bools: a bare literal at a call site reads as the opposite direction
//(forward was `false`), which is how GOF/GOR ended up reversed. The numeric values are unchanged.
//`encoder.c` keeps one of these in a volatile shared with INT0, so it must stay 1 byte; -fshort-enums
//guarantees that (verified: sizeof(Direction) == 1). Note C does not reject `true` here, so the L0
//guard tools/test_conventions.py enforces the constants instead.
typedef enum {
	DIRECTION_FORWARD,
	DIRECTION_REVERSE
} Direction;

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