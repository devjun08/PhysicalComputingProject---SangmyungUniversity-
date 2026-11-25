#ifndef SERVO_H
#define SERVO_H
#include <stdint.h>
/* Existing PB9 / TIM4_CH4 SG90 driver; angle mapping is configured in config.h. */
void Servo_Init(void);
void Servo_SetAngle(uint16_t angle);
void Servo_Stand(void);
void Servo_Down(void);
int Servo_Available(void);
#endif
