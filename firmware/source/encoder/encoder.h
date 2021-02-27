#ifndef DOMECONTROL_ENCODER_H
#define DOMECONTROL_ENCODER_H

#include <stdint.h>
#include <stdbool.h>

void encoderInit();
void encoderEnableCounting(bool direction);
void encoderDisableCounting();
int16_t encoderGetValue();
void encoderSetValue(int16_t value);

#endif //DOMECONTROL_ENCODER_H
