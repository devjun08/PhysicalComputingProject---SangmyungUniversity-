#ifndef MOTOR_H
#define MOTOR_H
#include <stdint.h>
typedef enum { MOTOR_STOPPED, MOTOR_HOMING, MOTOR_READY, MOTOR_PATROLLING, MOTOR_FAULT } MotorStatus;
void Motor_Init(void);
void Motor_Home(uint32_t now);
void Motor_Patrol(uint32_t now);
void Motor_Stop(void);
void Motor_Update(uint32_t now);
void Motor_SetDuty(uint8_t duty);
MotorStatus Motor_Status(void);
uint8_t Motor_Error(void);
#endif
