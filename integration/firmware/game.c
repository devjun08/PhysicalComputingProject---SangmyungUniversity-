/* main 루프에서 게임 상태와 상태 진입 시간을 관리한다. */
#include "game.h"
#include "config.h"
#include "comm.h"
#include "motor.h"
#include "servo.h"
#include "hit_detector.h"
/* ================= 게임 상태 ================= */
static uint8_t state, homing;
static uint32_t entered, started;
static void enter(uint8_t next, uint32_t now)
{
    state = next; entered = now; Comm_SetState(next);
}
static void finish(uint32_t now)
{
    Motor_Stop(); homing = 0U; Servo_Stand(); enter(ST_GAMEOVER, now);
}
void Game_Init(uint32_t now)
{
    homing = 0U;
    HitDetector_Reset();
    Motor_Stop(); Servo_Stand(); enter(ST_IDLE, now);
}
/* ---------- PC 시작, 종료, 속도 명령 ---------- */
void Game_Command(int command, uint32_t now)
{
    static const uint8_t speeds[5] = {30U, 40U, 50U, 60U, 70U};
    if (command == CMD_END) finish(now);
    else if (command >= CMD_SPEED1 && command <= CMD_SPEED5)
        Motor_SetDuty(speeds[command - CMD_SPEED1]);
    else if (command == CMD_START && !homing &&
             (state == ST_IDLE || state == ST_GAMEOVER)) {
        /* 호밍 중에는 PC에 IDLE을 보내고, 중앙에 도착하면 PATROL로 바꾼다. */
        HitDetector_Reset(); Servo_Stand(); Motor_Home(now); homing = 1U; enter(ST_IDLE, now);
    }
}
/* ---------- 모터 상태와 경과 시간에 따른 게임 진행 ---------- */
void Game_Update(uint32_t now)
{
    Motor_Update(now);
    if (Motor_Status() == MOTOR_FAULT) {
        /* Preserve the motor's diagnostic fault until a new START retries homing. */
        homing = 0U; Servo_Stand(); enter(ST_GAMEOVER, now); return;
    }
    if (homing) {
        if (Motor_Status() == MOTOR_READY) {
            homing = 0U; started = now;
            Motor_Patrol(now); enter(ST_PATROL, now);
        }
        return;
    }
#if GAME_DURATION_MS > 0
    if (state >= ST_PATROL && state <= ST_RECOVER && now - started >= GAME_DURATION_MS) {
        finish(now); return;
    }
#endif
    /* 피격 후 HIT(정지) → DOWN(도복) → RECOVER(기립) → PATROL(순찰) */
    switch (state) {
    case ST_HIT:
        if (now - entered >= HIT_BRAKE_MS) {
            Servo_Down();
            enter(ST_DOWN, now);
        }
        break;
    case ST_DOWN:
        if (now - entered >= TARGET_DOWN_MS) { Servo_Stand(); enter(ST_RECOVER, now); }
        break;
    case ST_RECOVER:
        if (now - entered >= TARGET_RECOVER_MS) { Motor_Patrol(now); enter(ST_PATROL, now); }
        break;
    default: break;
    }
}
/* ---------- ADC 전송과 순찰 중 피격 처리 ---------- */
void Game_Sensors(const uint16_t values[9], uint32_t now)
{
    uint8_t zone;
    Comm_SetAdc(values);
    zone = HitDetector_Update(values, now);
    if (HIT_DETECTION_ENABLED && zone && state == ST_PATROL && !homing) {
        uint32_t reaction = now - entered;
        Motor_Stop();
        Comm_ReportHit(zone, reaction > 65535U ? 65535U : (uint16_t)reaction);
        enter(ST_HIT, now);
    }
}
