#include "stm32f4xx.h"

#define SERVO_TIMER_HZ       1000000U
#define SERVO_PERIOD_US      20000U
#define SERVO_MIN_PULSE_US   1000U
#define SERVO_MAX_PULSE_US   2000U
#define SERVO_MAX_ANGLE      180U

static void LED_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_5;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(GPIOA, &gpio);
}

static void Servo_Init(void)
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
}

static void Servo_SetAngle(uint16_t angle)
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

static void DelayMs(uint32_t milliseconds)
{
    while (milliseconds > 0U)
    {
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
        }
        --milliseconds;
    }
}

int main(void)
{
    SystemCoreClockUpdate();
    SysTick->LOAD = SystemCoreClock / 1000U - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    LED_Init();
    Servo_Init();
    Servo_SetAngle(90U);
    /* Load the first compare value before starting the PWM output. */
    TIM_GenerateEvent(TIM4, TIM_EventSource_Update);
    TIM_Cmd(TIM4, ENABLE);

    while (1)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_5);
        DelayMs(500U);

        GPIO_ResetBits(GPIOA, GPIO_Pin_5);
        DelayMs(500U);
    }
}
