> **먼저 `TEST_GUIDE.md`(보드 테스트 안내서)를 봐 주세요!**
> 이 폴더는 통신 단독 테스트용입니다. 팀 보드 실행은 [통합 코드 안내](../../../integration/README.md)를 참고하세요.
> `tools/check_comm.py` = Python 수신 확인, `pc_matlab/` = MATLAB 대시보드 (선택).

# 레이저 사격 시스템 – PC 통신 & GUI (초안)

담당: 이주원 · 기준: lec9 USART 실습 방식 (StdPeriph, USART2, MATLAB UI)

## 폴더 구성

| 파일 | 내용 |
|---|---|
| `firmware/protocol.h/.c` | 패킷 정의, CRC8, 프레임 생성 (하드웨어 무관) |
| `firmware/comm.h/.c` | USART2 인터럽트 송수신 드라이버 (PA2/PA3, ST-LINK 가상 COM) |
| `board_test/main_comm_test.c` | **보드 하나로** GUI를 테스트하는 가짜 게임 main |
| `pc_matlab/LaserDashboard.m` | PC 대시보드 (조준경 3×3, 탄착 분포, ADC 9채널, 반응속도, CSV 저장) |
| `pc_matlab/ls_parse_frames.m`, `ls_crc8.m`, `ls_build_frame.m` | 파서 / CRC / 프레임 생성 |
| `tools/check_comm.py` | PC에서 시리얼 프레임과 CRC 수신 확인 |

## 통신 규격

- **115200 bps, 8-N-1**, USART2 (PA2 TX / PA3 RX), CP2102 모듈 없음
- 클럭은 수업처럼 **16MHz (HSI)** 를 전제로 작성함

**MCU → PC : 27바이트 고정 프레임, 10ms마다 (100Hz, 한 프레임 약 2.3ms)**

```
idx : 0   1    2      3       4     5     6     7 ........ 24   25    26
      A5  Seq  State  HitCnt  Zone  RT_H  RT_L  CH1_H ... CH9_L CRC8  5A
```

| 필드 | 의미 |
|---|---|
| Seq | 프레임마다 +1 (0~255 순환) → PC가 유실 패킷 수 계산 |
| State | 0 IDLE, 1 PATROL, 2 HIT, 3 DOWN, 4 RECOVER, 5 GAME OVER |
| HitCnt | 피격마다 +1 → PC는 이 값이 바뀔 때만 새 피격으로 처리 (중복 집계 없음) |
| Zone | 마지막 피격 구역 1~9 (0 = 없음) |
| RT | 마지막 피격 반응속도 [ms], **상위 바이트 먼저** (수업 방식) |
| CH1~9 | CdS ADC 12bit 원시값, 상위 바이트 먼저 |
| CRC8 | 다항식 0x07, 초기값 0x00, idx 1~24 대상 |

**PC → MCU : 1바이트 명령** – `'S'` 시작, `'E'` 종료, `'1'`~`'5'` 순찰 속도

## 다른 팀원이 호출할 함수 (이것만 넣어주면 됨)

```c
#include "comm.h"

Comm_SetState(ST_PATROL);            // FSM 상태가 바뀔 때마다
Comm_ReportHit(zone, reaction_ms);   // 피격 판정 직후 (ISR 안에서 호출 가능)
Comm_SetAdc(adc9);                   // uint16_t[9], ADC 갱신 시 (ISR 안에서 호출 가능)
```

main 쪽 통합:

```c
Comm_Init();                         // 초기화 시 1회
// TIM2 1ms ISR 안에서: if (++ms % 10 == 0) Comm_Tick10ms();
while (1) {
    Comm_Process();                  // 실제 송신은 여기서 (ISR 안에서 보내지 않음)
    int c = Comm_GetCommand();       // -1이면 명령 없음
    ...
}
```

### 수업 코드와 다른 점 (이유)

- 수업은 `Serial_Send`를 폴링하면서 TIM2 ISR 안에서 보냄 → 27바이트면 약 2.3ms 동안 ISR이 막혀 **1ms 타임베이스가 밀림**.
  그래서 **TXE 인터럽트 + 링버퍼**로 바꿈.
- 수업은 `Serial_Receive`로 main이 멈춤 → **RXNE 인터럽트**로 받아 FSM이 계속 돌게 함.
- NVIC 그룹은 수업과 같은 `NVIC_PriorityGroup_1`. USART2는 선점 1 / 서브 3으로 TIM2·EXTI·ADC보다 낮음.
  **팀 전체가 같은 그룹을 써야 함.**

## 실행 방법

**보드 없이 GUI만 시연**
```matlab
cd pc_matlab
LaserDashboard          % Port에서 '(데모: 가짜 MCU)' 선택 → Connect → 게임 시작
```

**보드로 테스트**
1. Keil 프로젝트에 `protocol.c`, `comm.c`, `main_comm_test.c` + StdPeriph(usart, gpio, rcc, tim, misc) 추가
2. 수업처럼 `system_stm32f4xx.c`에서 `SetSysClock()` 주석 처리 (16MHz)
3. 굽고 MATLAB에서 `/dev/cu.usbmodem…` (맥) 또는 `COMx` (윈도우) 선택 → Connect
4. 게임 시작 → 약 4초마다 (순찰 1.75~2.55초 + 도복·기립 1.9초) 가짜 피격이 들어오면 정상. RX가 초당 약 100씩 늘고 CRC오류·유실이 0인지 확인

**Python으로 수신 확인**

[보드 테스트 안내서](TEST_GUIDE.md)의 `tools/check_comm.py` 실행 절차를 참고하세요.

## 아직 정할 것 / 확인 안 된 것

- [x] 클럭 16MHz 확정 (팀 합의: `system_stm32f4xx.c`의 `SetSysClock()` 주석 처리 → HSI 16MHz, 모든 타이머 클럭 16MHz)
      115200bps 실제 오차 약 0.08% (BRR = 8 + 11/16) → 문제 없음
- 통합 펌웨어는 순찰 상태 진입부터 피격 확정까지의 시간을 반응 시간으로 전송합니다.
- [ ] 센서 번호 1~9 배치가 사수 시점과 좌우 반대인지 → `LaserDashboard.m`의 `MIRROR_LR`
- [ ] 명중률: 총이 MCU와 연결되지 않아 빗나간 사격은 셀 수 없음
- [ ] **실제 보드에서는 아직 빌드·동작 확인 안 함** (PC에서 문법 검사와 프로토콜 테스트만 함)
