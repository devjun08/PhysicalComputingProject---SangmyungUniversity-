/* Adapted from Junyeop's SG90 driver, keeping PB9 / TIM4_CH4. */
#include "stm32f4xx.h"
#include "config.h"
#include "servo.h"
#define SERVO_TIMER_HZ 1000000U
#define SERVO_PERIOD_US 20000U
#define SERVO_MIN_PULSE_US 1000U
#define SERVO_MAX_PULSE_US 2000U
#define SERVO_MAX_ANGLE 180U
void Servo_Init(void)
{
    GPIO_InitTypeDef gpio;
    RCC_ClocksTypeDef clocks;
    TIM_TimeBaseInitTypeDef timer;
    TIM_OCInitTypeDef pwm;
    uint32_t timerClockHz;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    RCC_GetClocksFreq(&clocks);
    timerClockHz = clocks.PCLK1_Frequency;
    /* On the default TIMPRE setting, a divided APB1 doubles the timer clock. */
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency)
    {
        timerClockHz *= 2U;
    }

    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(timerClockHz / SERVO_TIMER_HZ - 1U);
    timer.TIM_Period = SERVO_PERIOD_US - 1U;
    TIM_TimeBaseInit(TIM4, &timer);

    TIM_OCStructInit(&pwm);
    pwm.TIM_OCMode = TIM_OCMode_PWM1;
    pwm.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OC4Init(TIM4, &pwm);
    TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_TIM4);
    GPIO_Init(GPIOB, &gpio);
    Servo_SetAngle(SERVO_STAND_ANGLE);
    TIM_GenerateEvent(TIM4, TIM_EventSource_Update);
    TIM_Cmd(TIM4, ENABLE);
}

void Servo_SetAngle(uint16_t angle)
{
    uint32_t pulseUs;

    if (angle > SERVO_MAX_ANGLE)
    {
        angle = SERVO_MAX_ANGLE;
    }

    pulseUs = SERVO_MIN_PULSE_US
            + ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * angle) / SERVO_MAX_ANGLE;
    TIM_SetCompare4(TIM4, pulseUs);
}


void Servo_Stand(void) { Servo_SetAngle(SERVO_STAND_ANGLE); }
void Servo_Down(void) { Servo_SetAngle(SERVO_DOWN_ANGLE); }
int Servo_Available(void) { return SERVO_POSES_CONFIRMED; }
