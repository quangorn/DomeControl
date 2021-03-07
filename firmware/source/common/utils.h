#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

///Macros
#define CONCAT(a, b)            a ## b
#define OUTPORT(name)           CONCAT(PORT, name)
#define INPORT(name)            CONCAT(PIN, name)
#define DDRPORT(name)           CONCAT(DDR, name)

bool checkCommand(const char* checkString, const char* cmd);
void printInt(int16_t value, char* dst);
int16_t parseInt(const char* src);

#endif //UTILS_H
