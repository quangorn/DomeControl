#include "utils.h"

bool checkCommand(const char* checkString, const char* cmd) {
	return !strncmp(checkString, cmd, strlen(checkString));
}
