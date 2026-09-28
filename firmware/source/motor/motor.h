
#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "source/common/definitions.h"

void motorInit();
void motorStart(Direction direction);
void motorStop();
void motorProceed();
bool motorIsStarted();
bool motorIsMoving();
//stop if started and start if stopped
void motorToggle(Direction direction);
void motorGoTo(int16_t position);
void motorFindCenter();

#endif /* MOTOR_H_ */