#ifndef DOMECONTROL_LIMITS_H
#define DOMECONTROL_LIMITS_H

#include <stdbool.h>

void limitsInit();
void limitsProceed();
bool limitsIsForwardLimitReached();
bool limitsIsReverseLimitReached();
bool limitsIsOnCenter();

#endif //DOMECONTROL_LIMITS_H
