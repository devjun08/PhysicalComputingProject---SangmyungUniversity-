#include "stm32f4xx.h"
#include "timebase.h"
#include "comm.h"
static volatile uint32_t ticks;
uint32_t Timebase_Now(void) { return ticks; }
void Timebase_Init(void)
{
    TIM_TimeBaseInitTypeDef timer;
    NVIC_InitTypeDef irq;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseStructInit(&timer);
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
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        ++ticks;
        if (ticks % 10U == 0U) Comm_Tick10ms();
    }
}
