#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

#define UART_BAUD_RATE 115200

#define MAX_COMMAND_LENGTH 64
#define MAX_RESPONSE_LENGTH 64
#define SEND_BUFFER_SIZE MAX_RESPONSE_LENGTH

#define END_COMMAND_CHARACTER '\n'

///Commands
const char* CMD_GO_FORWARD;
const char* CMD_GO_REVERSE;
const char* CMD_STOP;

///Responses
const char* RESP_GO_FORWARD_OK;
const char* RESP_GO_REVERSE_OK;
const char* RESP_STOP_OK;

#endif /* DEFINITIONS_H_ */