
#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdint.h>
#include <stdbool.h>

void motorInit();
void motorStart(bool direction);
void motorStop();
void motorProceed();
bool motorIsStarted();
//stop if started and start if stopped
void motorToggle(bool direction);
void motorGoTo(int16_t position);

#endif /* MOTOR_H_ */