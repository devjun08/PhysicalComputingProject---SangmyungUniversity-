#include "stm32f4xx.h"
#include "timebase.h"
#include "comm.h"
/* ================= 1ms 타임베이스 (TIM2) ================= */
static volatile uint32_t ticks;
uint32_t Timebase_Now(void) { return ticks; }
void Timebase_Init(void)
{
    TIM_TimeBaseInitTypeDef timer;
    NVIC_InitTypeDef irq;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseStructInit(&timer);
    /* 16MHz를 16으로 분주하고 1000카운트마다 인터럽트를 발생시킨다. */
    timer.TIM_Prescaler = 16U - 1U;
    timer.TIM_Period = 1000U - 1U;
    TIM_TimeBaseInit(TIM2, &timer);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    irq.NVIC_IRQChannel = TIM2_IRQn;
    irq.NVIC_IRQChannelPreemptionPriority = 0;
    irq.NVIC_IRQChannelSubPriority = 0;
    irq.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&irq);
    TIM_Cmd(TIM2, ENABLE);
}
/* ---------- 1ms 누적, 10ms마다 통신 모듈에 송신 시점 알림 ---------- */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        ++ticks;
        if (ticks % 10U == 0U) Comm_Tick10ms();
    }
}
