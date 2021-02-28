#ifndef DOMECONTROL_LIMITS_H
#define DOMECONTROL_LIMITS_H

#include <stdbool.h>

void limitsInit();
bool limitsIsForwardLimitReached();
bool limitsIsReverseLimitReached();
bool limitsIsOnParkPosition();

#endif //DOMECONTROL_LIMITS_H
