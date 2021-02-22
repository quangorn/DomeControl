#ifndef UTILS_H
#define UTILS_H

#include <string.h>

bool checkCommand(const char* checkString, const char* cmd) {
	return !strncmp(checkString, cmd, strlen(checkString));
}

#endif //UTILS_H
