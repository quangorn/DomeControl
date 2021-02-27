#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <stdbool.h>

///Macros
#define CONCAT(a, b)            a ## b
#define OUTPORT(name)           CONCAT(PORT, name)
#define INPORT(name)            CONCAT(PIN, name)
#define DDRPORT(name)           CONCAT(DDR, name)

bool checkCommand(const char* checkString, const char* cmd);

#endif //UTILS_H
