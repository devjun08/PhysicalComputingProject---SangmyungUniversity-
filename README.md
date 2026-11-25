# SMU Physical Computing

상명대학교 2026년 피지컬 컴퓨팅 수업의 개인 작업과 팀 통합 프로젝트를 모아 둔 저장소입니다. 레이저 사격 시스템에 사용할 CdS 센서 입력, 서보 구동, 모터 순찰, PC 통신을 각각 개발하고 테스트합니다.

## 개인 프로젝트

| 담당 | 프로젝트 | 역할 |
| --- | --- | --- |
| 동건 | [모터 호밍·순찰](individual/donggeon/motor-homing-patrol/) | 양쪽 리미트 스위치로 이동 시간을 측정해 중앙을 잡고, 스위치에 닿을 때마다 방향을 바꿉니다. |
| 준엽 | [서보 모터 테스트](individual/junyeop/SurboMotor_Final/) | 서보를 90도로 설정하고 PA5 LED를 500 ms 간격으로 켜고 끄는 테스트 코드입니다. |
| 시현 | [CdS 센서 9채널 ADC 테스트](individual/sihyeon/adc-cds-test/) | NUCLEO-F411RE의 ADC1을 폴링해 센서 9개의 12비트 원시값을 읽습니다. |
| 주원 | [USART2 통신 및 MATLAB 대시보드](individual/juwon/laser_comm_usart/) | MCU에서 게임 상태, 피격 정보, ADC 9채널 값을 PC로 보내고 PC에서 시작·종료·속도 명령을 받습니다. |

### 동건 · 모터 호밍과 순찰

[main.c](individual/donggeon/motor-homing-patrol/main.c)는 TIM1 PWM으로 모터를 구동하고 PB4·PB10 리미트 스위치를 EXTI로 감지합니다. 코드는 16 MHz 타이머 클럭을 전제로 하며, TIM2의 1 ms 틱으로 이동 시간과 호밍 타임아웃을 계산합니다.

### 준엽 · 서보 모터

IAR EWARM에서 [Servo90.eww](individual/junyeop/SurboMotor_Final/Servo90.eww)를 열면 `Servo90.ewp`가 연결됩니다. STM32F4 SPL의 `Libraries/`는 프로젝트 파일 옆에 둡니다. 최신 코드는 PB9(TIM4_CH4)에서 SG90에 90도 PWM을 출력합니다. 통합 코드도 같은 핀과 타이머를 사용합니다.

### 시현 · CdS 센서 입력

STM32F4 SPL V1.9.0의 `Libraries/`를 `USART.ewp` 옆에 두고 IAR에서 `USART.eww`의 **Debug** 구성을 엽니다. [배선과 핀 배정](individual/sihyeon/adc-cds-test/main.c)은 소스 상단에 있습니다. IAR Watch에서 `ADC1_data`부터 `ADC9_data`까지 확인할 수 있습니다. [프로젝트 안내](individual/sihyeon/adc-cds-test/README.md)

### 주원 · PC 통신과 대시보드

USART2는 ST-LINK 가상 COM을 통해 115200 bps, 8-N-1로 통신합니다. MATLAB에서 [pc_matlab](individual/juwon/laser_comm_usart/pc_matlab/) 폴더를 열고 `LaserDashboard`를 실행하면 데모 모드를 선택할 수 있습니다. 보드 단독 테스트와 Python 수신 확인 절차는 [테스트 가이드](individual/juwon/laser_comm_usart/TEST_GUIDE.md)를 참조하세요.

## 팀 통합

[통합 코드 실행 안내](integration/README.md)를 먼저 읽어 주세요. `integration/Integration.eww`를 IAR에서 열어 모터 원점 찾기·중앙 정렬·순찰, ADC 9채널, PC 통신을 함께 실행합니다. 기존 핀 배정과 개인 작업 원본은 유지했습니다.

서보는 기존 PB9(TIM4_CH4) 드라이버를 연결했습니다. 피격은 한 센서 값이 나머지 8개 평균보다 충분히 낮아진 상태를 3회 연속 확인하는 상대 비교 방식으로 기본 활성화했습니다. 서보 기립·도복은 실제 장착 각도를 확인한 뒤 활성화합니다. 준비물, 배선표, 라이브러리 설치, 빌드·보드 기록, MATLAB/Python 실행 및 오류 해결은 위 안내에 정리했습니다.
