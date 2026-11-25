/* Application state machine. All entry points are called from main, never ISR. */
#include "game.h"
#include "config.h"
#include "comm.h"
#include "motor.h"
#include "servo.h"
#include "hit_detector.h"
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
void Game_Command(int command, uint32_t now)
{
    static const uint8_t speeds[5] = {30U, 40U, 50U, 60U, 70U};
    if (command == CMD_END) finish(now);
    else if (command >= CMD_SPEED1 && command <= CMD_SPEED5)
        Motor_SetDuty(speeds[command - CMD_SPEED1]);
    else if (command == CMD_START && !homing &&
             (state == ST_IDLE || state == ST_GAMEOVER)) {
        /* Homing stays IDLE on wire: existing PC protocol has no HOMING state. */
        HitDetector_Reset(); Servo_Stand(); Motor_Home(now); homing = 1U; enter(ST_IDLE, now);
    }
}
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
