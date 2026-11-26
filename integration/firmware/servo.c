/* Lecture-style initialization from the individual SG90 test.
 * Wiring unchanged: PB9 / TIM4_CH4, 16 MHz HSI, 20 ms PWM period.
 * TIM2 belongs to timebase.c; the standalone LED test is not duplicated here.
 */
#include "stm32f4xx.h"
#include "config.h"
#include "servo.h"

/* ---------- 서보 출력 (PB9: TIM4_CH4, AF2) ---------- */
static void Servo_GPIO_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_TIM4);
}

/* ---------- TIM4: 1µs 카운트, 20ms 반복 주기 ---------- */
static void TIM4_Configuration(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    /* TIM4 clock: 16 MHz HSI / 16 = 1 MHz, so one count is 1 us. */
    TIM_TimeBaseStructure.TIM_Prescaler = 16 - 1;
    TIM_TimeBaseStructure.TIM_Period = 20000 - 1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
}

/* ---------- 채널 4 PWM 및 비교값 프리로드 설정 ---------- */
static void PWM_TIM4_Configuration(void)
{
    TIM_OCInitTypeDef TIM_OCInitStructure;

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OC4Init(TIM4, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig(TIM4, TIM_OCPreload_Enable);
}

/* ================= 서보 초기화 ================= */
void Servo_Init(void)
{
    Servo_GPIO_init();
    TIM4_Configuration();
    PWM_TIM4_Configuration();
    Servo_Stand();
    /* Apply the standing pulse before the first PWM cycle, as in the individual test. */
    TIM_GenerateEvent(TIM4, TIM_EventSource_Update);
    TIM_Cmd(TIM4, ENABLE);
}

/* ---------- 각도를 PWM 펄스 폭으로 환산 (0~180°, 1000~2000µs) ---------- */
void Servo_SetAngle(uint16_t angle)
{
    uint32_t pulse_us;
    if (angle > 180U) angle = 180U;
    pulse_us = 500U + (1900U * angle) / 180U;
    TIM_SetCompare4(TIM4, pulse_us);
}

void Servo_Stand(void) { Servo_SetAngle(SERVO_STAND_ANGLE); }
void Servo_Down(void) { Servo_SetAngle(SERVO_DOWN_ANGLE); }
