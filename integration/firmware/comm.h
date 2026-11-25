/*
 * comm.h - USART2(PA2/PA3, ST-LINK 가상 COM) 통신 모듈
 *
 * 수업(lec9) 방식 그대로 StdPeriph + USART2 115200 8-N-1을 쓰되,
 * 폴링 대신 인터럽트로 송수신해서 FSM/타이머를 막지 않게 만든 버전.
 *
 * 다른 팀원이 부르는 함수
 *   Comm_SetState(ST_PATROL);          // FSM 상태가 바뀔 때 (동건/FSM)
 *   Comm_ReportHit(zone, reaction_ms); // 피격 판정 직후 (ADC 담당, ISR 안에서 호출 OK)
 *   Comm_SetAdc(adc9);                 // ADC 9채널 값 갱신 (ISR 안에서 호출 OK)
 *
 * 내가(main 쪽) 부르는 함수
 *   Comm_Init();                       // 시작 시 1회
 *   Comm_Tick10ms();                   // TIM2 1ms ISR에서 10번에 1번 호출
 *   Comm_Process();                    // main while(1)에서 계속 호출
 *   c = Comm_GetCommand();             // PC 명령 1바이트 (-1 = 없음)
 */
#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include "protocol.h"

void Comm_Init(void);

void Comm_SetState(uint8_t state);
void Comm_ReportHit(uint8_t zone, uint16_t reaction_ms);
void Comm_SetAdc(const uint16_t adc[PROTO_NUM_CH]);

void Comm_Tick10ms(void);
void Comm_Process(void);
int  Comm_GetCommand(void);

/* 디버깅용 카운터 */
uint32_t Comm_TxDropCount(void);   /* 송신 버퍼가 꽉 차서 버린 프레임 수 */
uint32_t Comm_RxDropCount(void);   /* 수신 버퍼가 꽉 차서 버린 바이트 수 */

#endif
