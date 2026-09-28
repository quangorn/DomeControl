#ifndef DOMECONTROL_ENCODER_H
#define DOMECONTROL_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include "source/common/definitions.h"

void encoderInit();
void encoderEnableCounting(Direction direction);
void encoderDisableCounting();

int16_t encoderGetValue();
int16_t encoderGetForwardLimitPosition();
int16_t encoderGetReverseLimitPosition();
int16_t encoderGetCenterPosition();

void encoderSetValue(int16_t value);
void encoderSetForwardLimitPosition();
void encoderSetReverseLimitPosition();
void encoderSetCenterPosition();

#endif //DOMECONTROL_ENCODER_H
