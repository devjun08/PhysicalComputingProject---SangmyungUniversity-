/*
 * main_comm_test.c - 통신 + GUI 단독 테스트용 main
 *
 * 다른 팀원 하드웨어(모터/센서) 없이 보드 하나만으로 MATLAB 대시보드를 테스트한다.
 *  - 가짜 ADC 파형(삼각파 9채널)을 100Hz로 보냄
 *  - 'S' 받으면 게임 시작: 순찰 1.5초 -> 가짜 피격 -> 도복 1.5초 -> 기립 -> 반복
 *  - 'E' 받으면 게임 종료, '1'~'5'는 속도 단계 (여기서는 LED 깜빡임 속도로 확인)
 *
 * 전제: 수업과 같이 system_stm32f4xx.c의 SetSysClock() 주석 처리 -> 16MHz (HSI)
 * 필요 소스: protocol.c, comm.c + StdPeriph (usart, gpio, rcc, tim, misc)
 * 실제 통합 시에는 이 파일 대신 팀 main에 Comm_* 호출만 옮겨 넣으면 됨.
 */
#include "stm32f4xx.h"
#include "comm.h"

static volatile uint32_t g_ms = 0;          /* 1ms 타임베이스 (TIM2) */
static volatile uint8_t  g_playing = 0;
static volatile uint8_t  g_speed = 3;

/* ---------------- 수업 코드 그대로 ---------------- */
void LED_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

/* 16MHz / 16000 = 1kHz 카운트 -> Period = intervalms - 1 (수업 TIM2_Configuration) */
void TIM2_Configuration(int intervalms)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Prescaler     = 16000 - 1;
    TIM_TimeBaseStructure.TIM_Period        = intervalms - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    NVIC_InitStructure.NVIC_IRQChannel                   = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/* ---------------- 가짜 게임 (FSM 흉내) ---------------- */
static uint32_t rng_state = 12345u;
static uint8_t rand_zone(void)
{
    rng_state = rng_state * 1103515245u + 12345u;
    return (uint8_t)(((rng_state >> 16) % 9u) + 1u);
}

static uint8_t  fake_state = ST_IDLE;
static uint32_t state_t0 = 0;       /* 현재 상태에 들어온 시각 [ms] */
static uint8_t  last_zone = 0;
static uint32_t next_hit_ms = 2000;  /* 순찰 시작 후 가짜 피격까지 시간 */

static void set_state(uint8_t s)
{
    fake_state = s;
    state_t0 = g_ms;
    if (s == ST_PATROL) {               /* 속도 단계 + 랜덤 0~799ms -> 반응속도에 변화 */
        rng_state = rng_state * 1103515245u + 12345u;
        next_hit_ms = 1000u + 250u * g_speed + ((rng_state >> 16) % 800u);
    }
    Comm_SetState(s);
}

/* 10ms마다 호출 */
static void fake_game_step(void)
{
    uint32_t dt = g_ms - state_t0;

    switch (fake_state) {
    case ST_PATROL:
        if (dt >= next_hit_ms) {
            last_zone = rand_zone();
            Comm_ReportHit(last_zone, (uint16_t)dt);   /* 반응속도 = 기립 후 경과 시간 */
            set_state(ST_HIT);
        }
        break;
    case ST_HIT:      if (dt >= 100u)  set_state(ST_DOWN);    break;
    case ST_DOWN:     if (dt >= 1500u) set_state(ST_RECOVER); break;
    case ST_RECOVER:  if (dt >= 300u)  set_state(ST_PATROL);  break;
    default: break;
    }

    /* 가짜 ADC: 채널마다 위상이 다른 삼각파 (1000~1400), 피격 채널은 순간적으로 3800 */
    uint16_t adc[PROTO_NUM_CH];
    for (uint32_t ch = 0; ch < PROTO_NUM_CH; ch++) {
        uint32_t ph = (g_ms / 10u + ch * 11u) % 100u;
        uint32_t tri = (ph < 50u) ? ph : (100u - ph);   /* 0~50 */
        adc[ch] = (uint16_t)(1000u + tri * 8u);
    }
    if (fake_state == ST_HIT && last_zone >= 1u) adc[last_zone - 1u] = 3800u;
    Comm_SetAdc(adc);
}

/* ---------------- 인터럽트 ---------------- */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        g_ms++;
        if ((g_ms % 10u) == 0u) {
            fake_game_step();
            Comm_Tick10ms();        /* 송신은 main에서 */
        }
    }
}

/* ---------------- main ---------------- */
int main(void)
{
    LED_init();
    Comm_Init();
    TIM2_Configuration(1);          /* 1ms */
    set_state(ST_IDLE);

    uint32_t led_t = 0;

    while (1) {
        Comm_Process();

        int c = Comm_GetCommand();
        if (c == CMD_START) {
            g_playing = 1;
            set_state(ST_PATROL);
        } else if (c == CMD_END) {
            g_playing = 0;
            set_state(ST_GAMEOVER);
        } else if (c >= CMD_SPEED1 && c <= CMD_SPEED5) {
            g_speed = (uint8_t)(c - '0');
        }

        /* LED: 게임 중이면 속도 단계에 따라 깜빡임, 아니면 꺼짐 */
        if (g_playing) {
            if (g_ms - led_t >= 600u - 100u * g_speed) {
                led_t = g_ms;
                GPIO_ToggleBits(GPIOA, GPIO_Pin_5);
            }
        } else {
            GPIO_ResetBits(GPIOA, GPIO_Pin_5);
        }
    }
}
