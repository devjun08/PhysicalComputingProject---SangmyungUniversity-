# SG90 서보 90° 테스트

NUCLEO-F411RE가 시작하면 SG90에 90° 위치 명령을 보내고, 보드의 PA5 LED를 500ms 간격으로 켜고 끕니다.

| SG90 단자 | 연결 |
| --- | --- |
| 제어 신호 | PB9 (TIM4_CH4) |
| VCC | 5V 전원 |
| GND | 5V 전원과 NUCLEO의 공통 GND |

STM32F4 SPL V1.9.0의 `Libraries/`를 `Servo90.ewp` 옆에 두고 IAR EWARM에서 `Servo90.eww`를 엽니다. Debug를 빌드해 보드에 다운로드합니다.

PWM 수업 자료의 SG90 실습 배선과 90° 기준 펄스를 적용했습니다. 16MHz HSI에서 TIM4 카운터를 1µs 단위로 설정하며, PB9는 20ms마다 1.5ms 동안 High가 됩니다. 이 값은 서보 축의 기준 위치 명령입니다. 표적이 눕는 방향은 서보 혼의 장착 각도에 따라 달라집니다.

TIM4는 서보 PWM을 출력하고, 강의 예제의 TIM2 업데이트 인터럽트는 PA5 LED를 500ms마다 토글합니다.

시작할 때 `Servo_Stand()`가 기립 위치를 설정합니다. `Servo_Down()`을 호출하면 도복 위치로 이동합니다. 두 위치의 초기값은 모두 90°이며, 표적을 장착한 뒤 `main.c`의 `SERVO_STAND_ANGLE`과 `SERVO_DOWN_ANGLE`을 실제 위치에 맞게 조정합니다.
