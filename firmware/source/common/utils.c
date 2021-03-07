#include "utils.h"
#include <stdlib.h>

bool checkCommand(const char* checkString, const char* cmd) {
	return !strncmp(checkString, cmd, strlen(checkString));
}

void printInt(int16_t value, char* dst) {
    itoa(value, dst, 10);
}

int16_t parseInt(const char* src) {
    return (int16_t)atoi(src); // NOLINT(cert-err34-c)
}
