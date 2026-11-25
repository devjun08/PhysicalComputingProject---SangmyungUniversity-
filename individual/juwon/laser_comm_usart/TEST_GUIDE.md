# 통신 보드 테스트 안내서

MCU ↔ PC 통신이 보드에서 잘 되는지 확인하는 순서예요.
**30분 정도** 걸리고, 다른 하드웨어(센서/모터) 없이 **보드 + USB 케이블만** 있으면 돼요.

---

## 0. 준비물

- NUCLEO 보드 + USB 케이블 (ST-LINK 쪽 USB 포트에 연결)
- 윈도우 노트북 + Keil (수업 때 쓰던 것)
- Python + pyserial (없으면 명령 프롬프트에서 `pip install pyserial`)
- 이 zip 파일 (`laser_comm_share`)

> **하드웨어(레일·과녁·센서) 조립이 안 돼 있어도 돼요.** 이 테스트 코드는 USART2(PA2/PA3)와 보드 LED(PA5)만 써요.
> 보드만 따로 있으면 그게 제일 좋아요.
>
> 이미 모터·서보가 연결돼 있다면 **모터용 5V 어댑터는 뽑아 주세요.**
> 테스트 코드는 모터 핀(PA8/PA9)과 서보 핀(PB9)을 설정하지 않아서, 이 핀들이 떠 있는 상태가 돼요. 그래서 모터나 서보가 예상치 않게 움직일 수 있어요.
> CdS 센서 선은 꽂혀 있어도 괜찮아요.

---

## 1. Keil 프로젝트 만들기 (10분)

1. **수업 USART 실습 프로젝트 폴더를 통째로 복사**해서 새 이름으로 만들어 주세요 (예: `comm_test`).
   - 이미 StdPeriph 파일이 들어 있고, `system_stm32f4xx.c`의 `SetSysClock();`이 주석 처리돼 있어서 편해요.
   - 주석 처리가 안 돼 있다면 꼭 해 주세요 (16MHz).
2. Keil에서 프로젝트를 열고, 왼쪽 Project 창에서
   - 원래 있던 **`main.c`를 프로젝트에서 제거** (우클릭 → Remove)
   - 아래 3개 파일을 **추가** (우클릭 → Add Existing Files)
     - `firmware/protocol.c`
     - `firmware/comm.c`
     - `board_test/main_comm_test.c`
3. **Options for Target (마법봉 아이콘) → C/C++ 탭 → Include Paths**에 `firmware` 폴더 추가
4. StdPeriph 소스 중 아래 파일들이 프로젝트에 있는지 확인 (없으면 추가)
   - `stm32f4xx_usart.c`, `stm32f4xx_tim.c`, `stm32f4xx_gpio.c`, `stm32f4xx_rcc.c`, `misc.c`

## 2. 빌드 & 다운로드 (2분)

1. **F7** (Build) → `0 Error(s)` 확인
2. **F8** (Download)
3. 보드의 **검은색 RESET 버튼** 한 번 누르기

> ❗ 빌드 에러가 `multiply defined` (예: `main`, `TIM2_IRQHandler`, `USART2_IRQHandler`)라면
> → 원래 `main.c`가 아직 프로젝트에 남아 있거나, `stm32f4xx_it.c`에 같은 이름의 함수가 있는 거예요. 그쪽 함수를 지워 주세요.

## 3. PC에서 수신 확인 (10분)

⚠️ **PuTTY / ComPortMaster / MATLAB이 켜져 있으면 먼저 꺼 주세요** (포트는 한 프로그램만 열 수 있어요).

1. 명령 프롬프트에서 `check_comm.py`가 있는 폴더로 이동
   ```
   cd laser_comm_share\tools
   ```
2. 포트 이름 확인
   ```
   python check_comm.py
   ```
   → `COM5  STMicroelectronics STLink Virtual COM Port` 같은 줄이 보이면 그 COM 번호 사용
3. 연결
   ```
   python check_comm.py COM5
   ```
4. 1초마다 이런 줄이 나오면 **수신 성공** ✅
   ```
   [   3.0s] 초당 100 | 누적    300 | CRC오류 0 | 유실 0 | 상태 IDLE      | ADC 1080 1168 ...
   ```
5. 명령 테스트: **`S` 입력 후 Enter** → 게임 시작
   - 보드 LED(초록)가 깜빡이기 시작
   - 상태가 `PATROL → HIT → DOWN → RECOVER → PATROL`로 바뀜
   - 약 4초마다 `** 피격! 구역 N  반응속도 NNNN ms` 출력
6. **`5` + Enter** → LED가 빨리 깜빡임 / **`1` + Enter** → 느리게 깜빡임
7. **`E` + Enter** → 상태 `GAME OVER`, LED 꺼짐
8. **`Q` + Enter** 로 종료

> 피격·센서 값은 **가짜 데이터**예요 (통신 테스트용). 실제 센서와는 아직 연결 안 했어요.

## 4. 1,000패킷 무손실 테스트 (1분) — 계획서 검증 항목

```
python check_comm.py COM5 --count 1000
```
→ 마지막에 `무손실률 100.00%  (목표 99.9% 이상 -> 통과)`가 나오면 성공. 이 화면은 보고서에 쓸 수 있도록 캡처해 두세요.

---

## ✅ 정상 기준 체크리스트

| 항목 | 정상 |
|---|---|
| 빌드 | 0 Error |
| `초당` | 약 100 (95~100) |
| `CRC오류`, `유실` | 계속 0 |
| `S` 입력 | LED 깜빡임 + 상태 바뀜 + 약 4초마다 피격 |
| `1`~`5` 입력 | LED 깜빡임 속도 변화 |
| `E` 입력 | GAME OVER + LED 꺼짐 |
| `--count 1000` | 무손실률 99.9% 이상 |

## 🔧 안 될 때

| 증상 | 원인 / 해결 |
|---|---|
| `포트를 열 수 없습니다` | COM 번호 확인, PuTTY 등 다른 프로그램 끄기, USB 다시 꽂기 |
| `수신 프레임 없음 (받은 바이트 0)` | RESET 버튼 누르기, 다운로드 됐는지 확인, USB가 ST-LINK 쪽에 꽂혔는지 확인 |
| `수신 프레임 없음`인데 받은 바이트는 늘어남 / CRC오류 계속 증가 | **클럭 문제** → `SetSysClock();` 주석 처리 확인 후 다시 빌드 |
| `초당`이 100보다 훨씬 적음 | 클럭 확인 (`SetSysClock();` 주석 처리), RESET 후 다시 실행 |
| 명령 보내도 LED 반응 없음 | 수신 쪽 문제 → `S` 입력 후 Enter 눌렀는지 (`>> 명령 전송` 출력 확인), `comm.c`가 프로젝트에 들어갔는지 확인 |

(선택) 실습실 PC에 **MATLAB**이 있으면 `pc_matlab` 폴더에서 `LaserDashboard` 실행 → 포트 선택 → Connect → 게임 시작으로 화면까지 볼 수 있어요. 이때는 Python 스크립트를 먼저 꺼 주세요.
