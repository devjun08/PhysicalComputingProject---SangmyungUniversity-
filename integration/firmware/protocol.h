/*
 * protocol.h - 레이저 사격 시스템 MCU <-> PC 패킷 정의
 *
 * 하드웨어와 무관한 순수 C 코드 (PC에서도 컴파일/테스트 가능)
 *
 * MCU -> PC : 27바이트 고정 길이 상태 프레임, 10ms(100Hz)마다 전송
 *
 *  idx  : 0    1    2      3       4     5     6     7..24          25    26
 *  내용 : A5   Seq  State  HitCnt  Zone  RT_H  RT_L  CH1_H..CH9_L   CRC8  5A
 *
 *  - Seq    : 프레임마다 +1 (0~255 순환). PC에서 유실 패킷 계산용
 *  - State  : FSM 상태 (아래 enum)
 *  - HitCnt : 피격될 때마다 +1. PC는 이 값이 바뀔 때만 "새 피격"으로 처리
 *  - Zone   : 마지막 피격 구역 1~9 (0 = 아직 없음)
 *  - RT     : 마지막 피격 반응속도 [ms], 상위 바이트 먼저 (수업 방식)
 *  - CH1~9  : CdS ADC 원시값 12bit, 상위 바이트 먼저
 *  - CRC8   : 다항식 0x07, 초기값 0x00, idx 1~24 범위
 *
 * PC -> MCU : 1바이트 명령 (수업의 write(usb, x, 'uint8') 방식)
 *  'S' 게임 시작, 'E' 게임 종료, '1'~'5' 순찰 속도 단계
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_HEAD        0xA5u
#define PROTO_TAIL        0x5Au
#define PROTO_FRAME_LEN   27u
#define PROTO_NUM_CH      9u

/* FSM 상태 코드 - 펌웨어 FSM 담당과 합의해서 맞출 것 */
typedef enum {
    ST_IDLE      = 0,   /* 게임 시작 전 대기 */
    ST_PATROL    = 1,   /* 레일 순찰 이동 중 */
    ST_HIT       = 2,   /* 피격 판정 -> 급제동 */
    ST_DOWN      = 3,   /* 서보 도복 상태 */
    ST_RECOVER   = 4,   /* 기립 복귀 중 */
    ST_GAMEOVER  = 5    /* 제한시간 종료 */
} ProtoState;

/* PC -> MCU 명령 바이트 */
#define CMD_START   'S'
#define CMD_END     'E'
#define CMD_SPEED1  '1'   /* '1'~'5' : 속도 단계 */
#define CMD_SPEED5  '5'

typedef struct {
    uint8_t  seq;
    uint8_t  state;
    uint8_t  hit_cnt;
    uint8_t  zone;                 /* 1~9, 0 = 없음 */
    uint16_t reaction_ms;
    uint16_t adc[PROTO_NUM_CH];    /* 12bit 원시값 */
} ProtoStatus;

uint8_t Proto_CRC8(const uint8_t *data, uint32_t len);

/* st 내용을 out[27]에 직렬화. 반환값: 프레임 길이(27) */
uint32_t Proto_BuildFrame(const ProtoStatus *st, uint8_t out[PROTO_FRAME_LEN]);

#endif
