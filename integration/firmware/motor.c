/* 모터 PWM은 PA8/TIM1_CH1, PA9/TIM1_CH2를 사용한다.
 * Reverse 끝은 PB4, Forward 끝은 PB10이다. 내부 풀업을 사용하며 눌리면 Low다.
 * main 루프에서 스위치와 경과 시간을 확인해 다음 단계로 진행한다.
 */
#include "stm32f4xx.h"
#include "config.h"
#include "motor.h"
/* ================= 모터 상태 ================= */
static MotorStatus status;
static uint8_t phase, error, forward = 1U, duty = MOTOR_PATROL_DUTY;
static uint32_t since, travel, center;
static uint8_t pressed(uint16_t pin) { return GPIO_ReadInputDataBit(GPIOB, pin) == Bit_RESET; }
static void halt(void) { TIM1->CCR1 = 0; TIM1->CCR2 = 0; }
static void drive(uint8_t dir, uint8_t percent)
{
    halt();
    if (dir) TIM1->CCR1 = MOTOR_PWM_COUNTS * percent / 100U;
    else TIM1->CCR2 = MOTOR_PWM_COUNTS * percent / 100U;
}
static void fail(uint8_t code) { halt(); error = code; status = MOTOR_FAULT; }
/* ---------- PWM 출력, 보드 LED, 양 끝 스위치 초기화 ---------- */
void Motor_Init(void)
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
    tim.TIM_Period            = MOTOR_PWM_COUNTS - 1;
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
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &gpio);
    halt();
    status = MOTOR_STOPPED;
}


void Motor_Stop(void) { halt(); status = MOTOR_STOPPED; }
MotorStatus Motor_Status(void) { return status; }
uint8_t Motor_Error(void) { return error; }
void Motor_SetDuty(uint8_t value) { duty = value > 100U ? 100U : value; }
/* ================= 호밍 ================= */
void Motor_Home(uint32_t now)
{
    halt(); error = 0U; status = MOTOR_HOMING; since = now;
    phase = pressed(GPIO_Pin_4) ? 1U : 0U;
    if (phase == 0U) drive(0U, MOTOR_HOME_DUTY);
}
/* ================= 순찰 ================= */
void Motor_Patrol(uint32_t now)
{
    if (status == MOTOR_FAULT) return;
    status = MOTOR_PATROLLING; phase = 6U; since = now;
    /* Always allow an asserted destination switch to trigger a reversal. */
    drive(forward, duty);
}
/* ---------- 현재 단계와 경과 시간으로 다음 동작 결정 ---------- */
void Motor_Update(uint32_t now)
{
    uint32_t dt = now - since;
    uint8_t rev = pressed(GPIO_Pin_4), end = pressed(GPIO_Pin_10);
    if (status != MOTOR_HOMING && status != MOTOR_PATROLLING) return;
    if (rev && end) { fail(5U); return; }
    switch (phase) {
    case 0: /* 1) Reverse 쪽 끝 스위치까지 이동 */
        if (rev) { halt(); phase = 1U; since = now; }
        else if (dt >= MOTOR_TIMEOUT_MS) fail(1U);
        break;
    case 1: /* 2) 정지 시간 이후 Forward 주행 및 편도 시간 측정 시작 */
        if (dt >= MOTOR_STOP_MS) { drive(1U, MOTOR_HOME_DUTY); phase = 2U; since = now; }
        break;
    case 2:
        if (end) {
            halt(); travel = dt;
            if (travel < MOTOR_TRAVEL_MIN_MS || travel > MOTOR_TRAVEL_MAX_MS) { fail(3U); break; }
            center = (uint32_t)((int32_t)(travel / 2U) + MOTOR_CENTER_TRIM_MS);
            phase = 3U; since = now;
        } else if (dt >= MOTOR_TIMEOUT_MS) fail(2U);
        else if (dt >= MOTOR_LEAVE_MS && rev) fail(6U);
        break;
    case 3: /* 3) 편도 시간의 절반만큼 Reverse로 복귀 시작 */
        if (dt >= MOTOR_STOP_MS) { drive(0U, MOTOR_HOME_DUTY); phase = 4U; since = now; }
        break;
    case 4:
        if (rev || (dt >= MOTOR_LEAVE_MS && end)) { fail(4U); break; }
        if (dt >= center) { halt(); phase = 5U; since = now; }
        break;
    case 5:
        if (dt >= MOTOR_STOP_MS) { status = MOTOR_READY; forward = 1U; }
        break;
    case 6: /* 순찰 중 목적지 스위치를 확인하고 눌리면 반전 준비 */
        if ((forward && end) || (!forward && rev)) {
            halt(); forward = !forward; phase = 7U; since = now;
        } else if (dt >= MOTOR_TIMEOUT_MS) fail(7U);
        else if (dt >= MOTOR_LEAVE_MS && ((forward && rev) || (!forward && end))) fail(6U);
        else drive(forward, duty);
        break;
    case 7:
        if (dt >= MOTOR_STOP_MS) { drive(forward, duty); phase = 6U; since = now; }
        break;
    default: fail(8U); break;
    }
}
