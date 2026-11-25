#include "stm32f4xx.h"

/* ================= 설정값 ================= */
#define DEBOUNCE_MS      100
#define STOP_WAIT_MS     30      // 방향 전환 전 정지 대기 (헛돎 줄이려고 짧게)
#define PWM_ARR          800     // 16MHz / 800 = 20kHz
#define PATROL_DUTY       50
#define HOME_DUTY       80

#define HOME_TIMEOUT_MS  4500    // 한 구간 최대 주행 시간 (편도 4.3초의 약 1.6배)
#define TRAVEL_MIN_MS    1500    // 측정한 편도 시간 정상 범위
#define TRAVEL_MAX_MS    4000
#define CENTER_TRIM_MS   0       // 중앙 미세조정: +면 센터링 때 더 가고, -면 덜 감
#define LEAVE_MS         300     // 스위치에서 막 떠난 직후 이 시간 동안은 스위치 신호 무시`

/* "Reverse 방향 끝"에 있는 스위치 핀.
   현재 코드 기준: PB4 = Reverse 쪽 끝. 반대로 판명되면 GPIO_Pin_10 으로 변경 */
#define REV_END_PIN      GPIO_Pin_4

/* ================= 변수 ================= */
static volatile uint32_t g_ms = 0;        // 1ms 시간 카운터
static volatile uint32_t g_lastMs4  = 0;  // 스위치별 마지막 인식 시각
static volatile uint32_t g_lastMs10 = 0;
static volatile uint8_t  g_hit = 0;       // 어느 쪽이든 끝 스위치가 눌리면 1
static volatile uint8_t  g_err = 0;       // 오류 코드 (Watch 확인용)
static volatile uint32_t g_travelMs = 0;  // 측정한 편도 시간 (Watch 확인용)
static volatile uint32_t g_duty = PATROL_DUTY;   // 현재 모터 듀티

/* ---------- 모터 (PA8: A-IA = TIM1_CH1, PA9: A-IB = TIM1_CH2) ---------- */
static void Motor_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef oc;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_8 | GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_TIM1);

    gpio.GPIO_Pin  = GPIO_Pin_5;               // 보드 LED
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_Init(GPIOA, &gpio);

    tim.TIM_Prescaler         = 0;
    tim.TIM_Period            = PWM_ARR - 1;
    tim.TIM_CounterMode       = TIM_CounterMode_Up;
    tim.TIM_ClockDivision     = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &tim);

    TIM_OCStructInit(&oc);
    oc.TIM_OCMode      = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_OCPolarity  = TIM_OCPolarity_High;
    oc.TIM_Pulse       = 0;
    TIM_OC1Init(TIM1, &oc);
    TIM_OC2Init(TIM1, &oc);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);

    TIM_Cmd(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

static void Motor_Stop(void)    { TIM1->CCR1 = 0; TIM1->CCR2 = 0; }
static void Motor_Forward(void) { TIM1->CCR2 = 0; TIM1->CCR1 = PWM_ARR * g_duty / 100; }
static void Motor_Reverse(void) { TIM1->CCR1 = 0; TIM1->CCR2 = PWM_ARR * g_duty / 100; }

/* ---------- 1ms 타임베이스 (TIM2) ---------- */
static void TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef tim;
    NVIC_InitTypeDef nvic;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    tim.TIM_Prescaler     = 16 - 1;      // 16MHz / 16 = 1MHz (1us)
    tim.TIM_Period        = 1000 - 1;    // 1000us = 1ms
    tim.TIM_CounterMode   = TIM_CounterMode_Up;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &tim);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    TIM_Cmd(TIM2, ENABLE);
}

static void Delay_ms(uint32_t ms)
{
    uint32_t s = g_ms;
    while ((g_ms - s) < ms);
}

/* ---------- 스위치 (PB4: EXTI4, PB10: EXTI10) ---------- */
static void Switch_Init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_4 | GPIO_Pin_10;
    gpio.GPIO_Mode  = GPIO_Mode_IN;
    gpio.GPIO_PuPd  = GPIO_PuPd_UP;          // 내부 풀업
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource4);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource10);

    exti.EXTI_Line    = EXTI_Line4 | EXTI_Line10;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;   // 눌릴 때 HIGH -> LOW
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannelPreemptionPriority = 0;  // 스위치가 TIM2보다 높은 우선순위
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    nvic.NVIC_IRQChannel = EXTI4_IRQn;
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_Init(&nvic);
}

/* ---------- 인터럽트 핸들러 ---------- */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        g_ms++;
    }
}

void EXTI4_IRQHandler(void)          // PB4 스위치
{
    if (EXTI_GetITStatus(EXTI_Line4) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line4);
        if ((g_ms - g_lastMs4) >= DEBOUNCE_MS)
        {
            g_lastMs4 = g_ms;
            g_hit = 1;                            // 어느 쪽 끝이든 "끝 도달" 알림
            GPIO_ToggleBits(GPIOA, GPIO_Pin_5);   // 확인용 LED
        }
    }
}

void EXTI15_10_IRQHandler(void)      // PB10 스위치 (EXTI10~15 공유)
{
    if (EXTI_GetITStatus(EXTI_Line10) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line10);
        if ((g_ms - g_lastMs10) >= DEBOUNCE_MS)
        {
            g_lastMs10 = g_ms;
            g_hit = 1;
            GPIO_ToggleBits(GPIOA, GPIO_Pin_5);
        }
    }
}

/* ================= 호밍 ================= */

/* 오류: 모터 정지 후 LED를 code 번 깜빡이고 1초 쉬기를 반복.
   1 = 1단계(Reverse 끝 찾기) 타임아웃
   2 = 2단계(Forward 끝 찾기, 시간 측정) 타임아웃
   3 = 측정한 편도 시간이 정상 범위 밖
   4 = 센터링 도중 스위치가 눌림 */
static void Error_Halt(uint8_t code)
{
    uint8_t i;

    g_err = code;
    Motor_Stop();
    while (1)
    {
        for (i = 0; i < code; i++)
        {
            GPIO_SetBits(GPIOA, GPIO_Pin_5);
            Delay_ms(200);
            GPIO_ResetBits(GPIOA, GPIO_Pin_5);
            Delay_ms(200);
        }
        Delay_ms(1000);
    }
}

/* dirForward=1: Forward, 0: Reverse 로 끝 스위치가 눌릴 때까지 주행.
   ignoreMs: 출발 후 이 시간 동안 들어온 스위치 신호는 무시
             (방금 떠난 스위치의 접점 떨림 방지)
   성공 1 (걸린 시간 *elapsed), 타임아웃 0 */
static uint8_t Drive_Until_Hit(uint8_t dirForward, uint32_t timeoutMs,
                               uint32_t ignoreMs, uint32_t *elapsed)
{
    uint32_t t0;

    g_hit = 0;
    t0 = g_ms;
    if (dirForward) Motor_Forward();
    else            Motor_Reverse();

    while (1)
    {
        if (g_hit)
        {
            if ((g_ms - t0) >= ignoreMs) break;
            g_hit = 0;                          // 떨림 무시
        }
        if ((g_ms - t0) >= timeoutMs)
        {
            Motor_Stop();
            return 0;
        }
    }
    *elapsed = g_ms - t0;
    Motor_Stop();
    Delay_ms(STOP_WAIT_MS);
    return 1;
}

static void Homing(void)
{
    g_duty = HOME_DUTY;               // 호밍 시작 시 듀티 올림
    uint32_t t = 0;
    uint32_t travelMs = 0;
    uint32_t centerMs;
    uint32_t t0;

    Delay_ms(DEBOUNCE_MS);    // 시작 직후 디바운스 구간 통과

    /* 1) Reverse 쪽 끝으로 이동. 이미 그 끝 스위치가 눌려 있으면 생략 */
    if (GPIO_ReadInputDataBit(GPIOB, REV_END_PIN) != Bit_RESET)
    {
        if (!Drive_Until_Hit(0, HOME_TIMEOUT_MS, 0, &t)) Error_Halt(1);
    }

    /* 2) 반대쪽 끝까지 Forward 로 이동하며 편도 시간 T 측정 */
    if (!Drive_Until_Hit(1, HOME_TIMEOUT_MS, LEAVE_MS, &travelMs)) Error_Halt(2);
    g_travelMs = travelMs;
    if (travelMs < TRAVEL_MIN_MS || travelMs > TRAVEL_MAX_MS) Error_Halt(3);

    /* 3) T/2 만큼 Reverse 로 되돌아가 중앙에 정지 */
    centerMs = travelMs / 2 + CENTER_TRIM_MS;
    g_hit = 0;
    t0 = g_ms;
    Motor_Reverse();
    while ((g_ms - t0) < centerMs)
    {
        if (g_hit)
        {
            if ((g_ms - t0) >= LEAVE_MS) Error_Halt(4);   // 진짜 이상
            g_hit = 0;                                    // 방금 떠난 스위치 떨림
        }
    }
    Motor_Stop();
    Delay_ms(STOP_WAIT_MS);
    
    g_duty = PATROL_DUTY;               // 호밍 끝나면 순찰 듀티로 복귀
}

/* ================= main ================= */
int main(void)
{
    uint8_t dirForward;
    uint32_t tMove;

    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    Motor_Init();
    TIM2_Init();
    Switch_Init();

    Homing();                         // 게임 시작 시 중앙 정렬

    Delay_ms(2000);
    
    /* 순찰: Forward 부터 시작, 끝 스위치에 닿을 때마다 방향 반전 */
    dirForward = 1;
    g_hit = 0;
    tMove = g_ms;
    Motor_Forward();

    while (1)
    {
        if (g_hit)
        {
            g_hit = 0;
            if ((g_ms - tMove) < LEAVE_MS) continue;   // 방금 떠난 스위치 떨림 무시

            Motor_Stop();
            Delay_ms(STOP_WAIT_MS);
            dirForward = !dirForward;
            tMove = g_ms;
            if (dirForward) Motor_Forward();
            else            Motor_Reverse();
        }
    }
}
