#include "stm32f4xx.h"
#include "config.h"
#include "comm.h"
#include "game.h"
#include "motor.h"
#include "sensors.h"
#include "servo.h"
#include "timebase.h"
#if !defined(STM32F411xE)
#error Select STM32F411xE and USE_STDPERIPH_DRIVER.
#endif
/* ================= main ================= */
int main(void)
{
    uint32_t last_sample = 0U;
    uint16_t adc[9];
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    Motor_Init(); /* PWM을 0으로 시작하고 PA5를 오류 표시용으로 설정 */
    /* PWM과 타임베이스 계산은 HSI 16MHz, APB1/APB2 분주 1 기준이다. */
    if (SystemCoreClock != 16000000U ||
        (RCC->CFGR & (RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) != 0U) {
        GPIO_SetBits(GPIOA, GPIO_Pin_5);
        while (1) {} /* 클럭 설정 오류. 모터를 정지 상태로 유지한다. */
    }
    /* ---------- 서보, PC 통신, 1ms 타임베이스, ADC 초기화 ---------- */
    Servo_Init(); Comm_Init(); Timebase_Init(); Sensors_Init();
    Game_Init(Timebase_Now());
    /* ---------- PC 명령 → 게임 상태 → 센서 확인 → 상태 송신 ---------- */
    while (1) {
        uint32_t now = Timebase_Now();
        int command;
        while ((command = Comm_GetCommand()) >= 0) Game_Command(command, now);
        Game_Update(now);
        if (now - last_sample >= SENSOR_PERIOD_MS) {
            last_sample = now;
            if (Sensors_Read(adc)) Game_Sensors(adc, Timebase_Now());
            else Game_Command(CMD_END, Timebase_Now());
        }
        Comm_Process();
    }
}
