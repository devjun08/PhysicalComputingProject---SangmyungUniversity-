#include <assert.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "config.h"
#include "motor.h"
#include "game.h"
#include "protocol.h"
MockTimer mock_tim;
unsigned mock_switches;
static unsigned wire_state, hits, zone, reaction, down_count;
void Comm_SetState(uint8_t value) { wire_state = value; }
void Comm_SetAdc(const uint16_t *values) { (void)values; }
void Comm_ReportHit(uint8_t value, uint16_t ms) { ++hits; zone=value; reaction=ms; }
void Servo_Init(void) {}
void Servo_Stand(void) {}
void Servo_Down(void) { ++down_count; }
int Servo_Available(void) { return 1; }
static void home(uint32_t base)
{
    mock_switches=0; Game_Command(CMD_START,base);
    assert(Motor_Status()==MOTOR_HOMING && wire_state==ST_IDLE);
    mock_switches=GPIO_Pin_4; Game_Update(base+100);
    Game_Update(base+130);
    mock_switches=0; Game_Update(base+500);
    mock_switches=GPIO_Pin_10; Game_Update(base+2130);
    Game_Update(base+2160);
    mock_switches=0; Game_Update(base+3160);
    Game_Update(base+3190);
    assert(Motor_Status()==MOTOR_PATROLLING && wire_state==ST_PATROL);
}
int main(void)
{
    uint16_t adc[9]={0};
    Motor_Init(); Game_Init(0);
    assert(mock_tim.CCR1==0 && mock_tim.CCR2==0);
    Game_Command(CMD_START,0); Game_Command(CMD_END,1);
    Game_Update(10000);
    assert(Motor_Status()==MOTOR_STOPPED && wire_state==ST_GAMEOVER);
    home(20000);
    Game_Command('5',23191); Game_Update(23191);
    assert(mock_tim.CCR1==560);
    Game_Sensors(adc,23192); adc[2]=3500;
    Game_Sensors(adc,23200);
#if HIT_DETECTION_ENABLED
    assert(wire_state==ST_HIT && hits==1 && zone==3 && reaction==10);
    assert(mock_tim.CCR1==0 && mock_tim.CCR2==0);
    Game_Update(23300); assert(wire_state==ST_DOWN && down_count==1);
    Game_Update(24800); assert(wire_state==ST_RECOVER);
    Game_Update(25100); assert(wire_state==ST_PATROL);
    Game_Sensors(adc,25101); assert(hits==1); /* persistent light cannot re-hit */
    adc[2]=0; Game_Sensors(adc,25102);
    adc[2]=3500; Game_Sensors(adc,25103); assert(hits==2);
#else
    assert(wire_state==ST_PATROL && hits==0);
#endif
    Game_Command(CMD_END,26000); assert(wire_state==ST_GAMEOVER);
    mock_switches=0; Game_Command(CMD_START,27000); Game_Update(31500);
    assert(Motor_Status()==MOTOR_FAULT && Motor_Error()==1 && wire_state==ST_GAMEOVER);
    assert(mock_tim.CCR1==0 && mock_tim.CCR2==0);
    home(40000); /* start retries after a fault */
    mock_switches=GPIO_Pin_10; Game_Update(43191);
    assert(mock_tim.CCR1==0 && mock_tim.CCR2==0);
    mock_switches=0; Game_Update(43221); assert(mock_tim.CCR2==560);
    mock_switches=GPIO_Pin_4|GPIO_Pin_10; Game_Update(43222);
    assert(Motor_Status()==MOTOR_FAULT && Motor_Error()==5);
    /* Unsigned elapsed time remains correct across millisecond counter wrap. */
    mock_switches=0; Motor_Home(0xfffffff0U); Motor_Update(0x1184U);
    assert(Motor_Status()==MOTOR_FAULT && Motor_Error()==1);
    {
        ProtoStatus status={0}; uint8_t frame[27];
        status.seq=7; status.reaction_ms=0x1234; status.adc[8]=0x0abc;
        assert(Proto_CRC8((const uint8_t *)"123456789",9)==0xf4);
        assert(Proto_BuildFrame(&status,frame)==27);
        assert(frame[0]==0xa5 && frame[1]==7 && frame[5]==0x12 && frame[6]==0x34);
        assert(frame[23]==0x0a && frame[24]==0xbc && frame[26]==0x5a);
        assert(frame[25]==Proto_CRC8(frame+1,24));
    }
    puts("control and protocol checks passed");
    return 0;
}
