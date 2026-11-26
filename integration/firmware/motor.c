
/*
 * integration/firmware/motor.c
 *
 * NUCLEO-F411RE
 *
 * Motor PWM:
 *   PA8  - TIM1_CH1 (Forward)
 *   PA9  - TIM1_CH2 (Reverse)
 *
 * Limit switches:
 *   PB4  - Reverse end (EXTI4)
 *   PB10 - Forward end (EXTI10)
 *
 * Switch wiring:
 *   GPIO input + internal pull-up
 *   Pressed = LOW
 *
 * Switch events are captured by EXTI interrupts.
 * Motor control remains in Motor_Update().
 */

#include "stm32f4xx.h"
#include "config.h"
#include "motor.h"
#include "timebase.h"

/* ================= Switch configuration ================= */

#define SWITCH_DEBOUNCE_MS 100U

#define SW_REV_EVENT 0x01U
#define SW_END_EVENT 0x02U

/* ================= Motor state ================= */

static MotorStatus status;

static uint8_t phase = 0U;
static uint8_t error = 0U;
static uint8_t forward = 1U;
static uint8_t duty = MOTOR_PATROL_DUTY;

static uint32_t since = 0U;
static uint32_t travel = 0U;
static uint32_t center = 0U;

/* ================= EXTI shared variables ================= */

static volatile uint8_t switch_events = 0U;

static volatile uint32_t last_pb4_ms = 0U;
static volatile uint32_t last_pb10_ms = 0U;

static volatile uint8_t pb4_seen = 0U;
static volatile uint8_t pb10_seen = 0U;

/* ================= GPIO helpers ================= */

static uint8_t pressed(uint16_t pin)
{
    return GPIO_ReadInputDataBit(GPIOB, pin) == Bit_RESET;
}

/* ================= Motor PWM ================= */

static void halt(void)
{
    TIM1->CCR1 = 0U;
    TIM1->CCR2 = 0U;
}

static void drive(uint8_t dir, uint8_t percent)
{
    halt();

    if (dir) {
        TIM1->CCR1 =
            MOTOR_PWM_COUNTS * percent / 100U;
    } else {
        TIM1->CCR2 =
            MOTOR_PWM_COUNTS * percent / 100U;
    }
}

static void fail(uint8_t code)
{
    halt();
    error = code;
    status = MOTOR_FAULT;
}

/* ================= Event handling ================= */

/*
 * Read and clear pending switch events atomically.
 *
 * The IRQ can set switch_events asynchronously,
 * so reading and clearing must not be interrupted.
 */
static uint8_t take_switch_events(void)
{
    uint8_t events;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();

    events = switch_events;
    switch_events = 0U;

    __set_PRIMASK(primask);

    return events;
}

/*
 * Discard events generated before a new movement phase.
 */
static void clear_switch_events(void)
{
    (void)take_switch_events();
}

/* ================= EXTI interrupt handlers ================= */

/*
 * PB4: Reverse limit switch
 */
void EXTI4_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line4) != RESET) {
        uint32_t now;

        EXTI_ClearITPendingBit(EXTI_Line4);

        now = Timebase_Now();

        if (!pb4_seen ||
            (uint32_t)(now - last_pb4_ms) >=
                SWITCH_DEBOUNCE_MS) {

            pb4_seen = 1U;
            last_pb4_ms = now;

            switch_events |= SW_REV_EVENT;
        }
    }
}

/*
 * PB10: Forward limit switch
 *
 * EXTI lines 10 through 15 share this IRQ.
 */
void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line10) != RESET) {
        uint32_t now;

        EXTI_ClearITPendingBit(EXTI_Line10);

        now = Timebase_Now();

        if (!pb10_seen ||
            (uint32_t)(now - last_pb10_ms) >=
                SWITCH_DEBOUNCE_MS) {

            pb10_seen = 1U;
            last_pb10_ms = now;

            switch_events |= SW_END_EVENT;
        }
    }
}

/* ================= EXTI initialization ================= */

static void Switch_EXTI_Init(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    /* Enable SYSCFG clock */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_SYSCFG,
        ENABLE
    );

    /* PB4 -> EXTI4 */
    SYSCFG_EXTILineConfig(
        EXTI_PortSourceGPIOB,
        EXTI_PinSource4
    );

    /* PB10 -> EXTI10 */
    SYSCFG_EXTILineConfig(
        EXTI_PortSourceGPIOB,
        EXTI_PinSource10
    );

    /* Configure falling-edge interrupts */
    EXTI_StructInit(&exti);

    exti.EXTI_Line =
        EXTI_Line4 | EXTI_Line10;

    exti.EXTI_Mode =
        EXTI_Mode_Interrupt;

    exti.EXTI_Trigger =
        EXTI_Trigger_Falling;

    exti.EXTI_LineCmd = ENABLE;

    EXTI_Init(&exti);

    EXTI_ClearITPendingBit(
        EXTI_Line4 | EXTI_Line10
    );

    /* Configure NVIC */
    nvic.NVIC_IRQChannelPreemptionPriority = 1U;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    nvic.NVIC_IRQChannel = EXTI4_IRQn;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_Init(&nvic);
}

/* ================= Motor initialization ================= */

void Motor_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef oc;

    /* PA8 / PA9: TIM1 PWM */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOA,
        ENABLE
    );

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_TIM1,
        ENABLE
    );

    gpio.GPIO_Pin =
        GPIO_Pin_8 | GPIO_Pin_9;

    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(GPIOA, &gpio);

    GPIO_PinAFConfig(
        GPIOA,
        GPIO_PinSource8,
        GPIO_AF_TIM1
    );

    GPIO_PinAFConfig(
        GPIOA,
        GPIO_PinSource9,
        GPIO_AF_TIM1
    );

    /* PA5: Board LED */
    gpio.GPIO_Pin = GPIO_Pin_5;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOA, &gpio);

    /* TIM1 PWM configuration */
    tim.TIM_Prescaler = 0U;
    tim.TIM_Period = MOTOR_PWM_COUNTS - 1U;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0U;

    TIM_TimeBaseInit(TIM1, &tim);

    TIM_OCStructInit(&oc);

    oc.TIM_OCMode = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_OCPolarity = TIM_OCPolarity_High;
    oc.TIM_Pulse = 0U;

    TIM_OC1Init(TIM1, &oc);
    TIM_OC2Init(TIM1, &oc);

    TIM_OC1PreloadConfig(
        TIM1,
        TIM_OCPreload_Enable
    );

    TIM_OC2PreloadConfig(
        TIM1,
        TIM_OCPreload_Enable
    );

    TIM_ARRPreloadConfig(TIM1, ENABLE);

    TIM_Cmd(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);

    /* PB4 / PB10: Limit switch inputs */
    RCC_AHB1PeriphClockCmd(
        RCC_AHB1Periph_GPIOB,
        ENABLE
    );

    gpio.GPIO_Pin =
        GPIO_Pin_4 | GPIO_Pin_10;

    gpio.GPIO_Mode = GPIO_Mode_IN;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(GPIOB, &gpio);

    /* Enable EXTI after GPIO setup */
    Switch_EXTI_Init();

    halt();

    status = MOTOR_STOPPED;
    error = 0U;
    phase = 0U;
    forward = 1U;
    duty = MOTOR_PATROL_DUTY;
}

/* ================= Public motor API ================= */

void Motor_Stop(void)
{
    halt();
    status = MOTOR_STOPPED;
    clear_switch_events();
}

MotorStatus Motor_Status(void)
{
    return status;
}

uint8_t Motor_Error(void)
{
    return error;
}

void Motor_SetDuty(uint8_t value)
{
    duty = value > 100U ? 100U : value;
}

/* ================= Homing ================= */

void Motor_Home(uint32_t now)
{
    halt();

    error = 0U;
    status = MOTOR_HOMING;
    since = now;
    travel = 0U;
    center = 0U;

    clear_switch_events();

    /*
     * If PB4 is already pressed, skip the
     * first Reverse movement.
     */
    phase = pressed(GPIO_Pin_4) ? 1U : 0U;

    if (phase == 0U) {
        drive(0U, MOTOR_HOME_DUTY);
    }
}

/* ================= Patrol ================= */

void Motor_Patrol(uint32_t now)
{
    if (status == MOTOR_FAULT) {
        return;
    }

    status = MOTOR_PATROLLING;
    phase = 6U;
    since = now;

    clear_switch_events();

    drive(forward, duty);
}

/* ================= State machine ================= */

void Motor_Update(uint32_t now)
{
    uint32_t dt;
    uint8_t events;
    uint8_t rev_hit;
    uint8_t end_hit;
    uint8_t rev_level;
    uint8_t end_level;

    if (status != MOTOR_HOMING &&
        status != MOTOR_PATROLLING) {
        return;
    }

    dt = now - since;

    /* Snapshot interrupt events */
    events = take_switch_events();

    rev_hit =
        (events & SW_REV_EVENT) != 0U;

    end_hit =
        (events & SW_END_EVENT) != 0U;

    /*
     * GPIO levels are still checked for:
     * - switches already pressed at startup
     * - stuck switches
     * - simultaneous switch faults
     */
    rev_level = pressed(GPIO_Pin_4);
    end_level = pressed(GPIO_Pin_10);

    if (rev_level && end_level) {
        fail(5U);
        return;
    }

    switch (phase) {

    /* -----------------------------------------
     * Phase 0:
     * Find Reverse limit switch (PB4)
     * ----------------------------------------- */
    case 0U:

        if (rev_hit || rev_level) {
            halt();

            phase = 1U;
            since = now;

            clear_switch_events();
        }
        else if (dt >= MOTOR_TIMEOUT_MS) {
            fail(1U);
        }

        break;

    /* -----------------------------------------
     * Phase 1:
     * Stop, then move Forward
     * ----------------------------------------- */
    case 1U:

        if (dt >= MOTOR_STOP_MS) {
            clear_switch_events();

            drive(1U, MOTOR_HOME_DUTY);

            phase = 2U;
            since = now;
        }

        break;

    /* -----------------------------------------
     * Phase 2:
     * Find Forward limit switch (PB10)
     * Measure travel time
     * ----------------------------------------- */
    case 2U:

        if (end_hit || end_level) {
            halt();

            travel = dt;

            if (travel < MOTOR_TRAVEL_MIN_MS ||
                travel > MOTOR_TRAVEL_MAX_MS) {

                fail(3U);
                break;
            }

            center = (uint32_t)(
                (int32_t)(travel / 2U) +
                MOTOR_CENTER_TRIM_MS
            );

            phase = 3U;
            since = now;

            clear_switch_events();
        }
        else if (dt >= MOTOR_TIMEOUT_MS) {
            fail(2U);
        }
        else if (dt >= MOTOR_LEAVE_MS &&
                 rev_level) {
            fail(6U);
        }

        break;

    /* -----------------------------------------
     * Phase 3:
     * Stop, then return toward center
     * ----------------------------------------- */
    case 3U:

        if (dt >= MOTOR_STOP_MS) {
            clear_switch_events();

            drive(0U, MOTOR_HOME_DUTY);

            phase = 4U;
            since = now;
        }

        break;

    /* -----------------------------------------
     * Phase 4:
     * Return for half the measured travel time
     * ----------------------------------------- */
    case 4U:

        /*
         * PB4 should not be reached while
         * returning only halfway.
         *
         * PB10 must be released after
         * MOTOR_LEAVE_MS.
         */
        if (rev_hit || rev_level ||
            (dt >= MOTOR_LEAVE_MS && end_level)) {

            fail(4U);
            break;
        }

        if (dt >= center) {
            halt();

            phase = 5U;
            since = now;

            clear_switch_events();
        }

        break;

    /* -----------------------------------------
     * Phase 5:
     * Homing complete
     * ----------------------------------------- */
    case 5U:

        if (dt >= MOTOR_STOP_MS) {
            status = MOTOR_READY;
            forward = 1U;

            clear_switch_events();
        }

        break;

    /* -----------------------------------------
     * Phase 6:
     * Patrol until destination switch is hit
     * ----------------------------------------- */
    case 6U:

        if ((forward && (end_hit || end_level)) ||
            (!forward && (rev_hit || rev_level))) {

            halt();

            forward = !forward;

            phase = 7U;
            since = now;

            clear_switch_events();
        }
        else if (dt >= MOTOR_TIMEOUT_MS) {
            fail(7U);
        }
        else if (dt >= MOTOR_LEAVE_MS &&
                 ((forward && rev_level) ||
                  (!forward && end_level))) {

            fail(6U);
        }
        else {
            drive(forward, duty);
        }

        break;

    /* -----------------------------------------
     * Phase 7:
     * Wait before changing direction
     * ----------------------------------------- */
    case 7U:

        if (dt >= MOTOR_STOP_MS) {
            clear_switch_events();

            drive(forward, duty);

            phase = 6U;
            since = now;
        }

        break;

    default:
        fail(8U);
        break;
    }
}
