/* SG90 90도 위치 테스트 (NUCLEO-F411RE)
 * PB9는 서보 제어 신호를 출력하고, PA5 LED는 500ms마다 반전된다.
 */
#include "stm32f4xx.h"

/* 표적에 장착한 뒤 기립·도복 위치를 각각 확인해서 조정한다. */
#define SERVO_STAND_ANGLE 0U
#define SERVO_DOWN_ANGLE  90U

/* 보드의 PA5 LED를 GPIO 출력으로 설정한다. */
static void LED_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* PB9를 대체 기능(AF2)으로 설정해 TIM4 채널 4 출력을 연결한다. */
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

/* TIM4의 카운터 속도와 PWM 반복 주기를 설정한다. */
static void TIM4_Configuration(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    /* HSI 16MHz를 16으로 나누면 한 카운트는 1µs다.
     * 카운터는 0부터 시작하므로 ARR에 19999를 넣으면 20ms(50Hz)마다 반복된다.
     */
    TIM_TimeBaseStructure.TIM_Prescaler = 16 - 1;
    TIM_TimeBaseStructure.TIM_Period = 20000 - 1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
}

/* PWM 모드 1에서 카운터가 CCR4보다 작은 동안 PB9가 High로 유지된다. */
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

/* 강의 SG90 실습 범위: 0~180도를 500~2400µs로 환산한다. */
static void Servo_SetAngle(uint16_t angle)
{
    if (angle > 180U)
    {
        angle = 180U;
    }

    TIM_SetCompare4(TIM4, 500U + (1900U * angle) / 180U);
}

void Servo_Stand(void)
{
    Servo_SetAngle(SERVO_STAND_ANGLE);
}

void Servo_Down(void)
{
    Servo_SetAngle(SERVO_DOWN_ANGLE);
}

/* TIM2의 업데이트 인터럽트가 LED 반전 시점을 결정한다. */
static void TIM2_Configuration(uint16_t interval_ms)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    /* HSI 16MHz를 16000으로 나누면 한 카운트는 1ms다. */
    TIM_TimeBaseStructure.TIM_Prescaler = 16000 - 1;
    TIM_TimeBaseStructure.TIM_Period = interval_ms - 1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    /* 초기화 중 발생한 업데이트 플래그를 지운 뒤 인터럽트를 켠다. */
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        GPIO_ToggleBits(GPIOA, GPIO_Pin_5);
    }
}

int main(void)
{
    LED_init();
    Servo_GPIO_init();
    TIM4_Configuration();
    PWM_TIM4_Configuration();
    Servo_Stand();
    /* 첫 PWM 주기부터 기립 위치의 펄스 폭을 적용한다. */
    TIM_GenerateEvent(TIM4, TIM_EventSource_Update);
    TIM_Cmd(TIM4, ENABLE);
    TIM2_Configuration(500);

    /* 서보 PWM은 TIM4 하드웨어가 출력하고, LED는 TIM2 인터럽트가 처리한다. */
    while (1)
    {
    }
}
