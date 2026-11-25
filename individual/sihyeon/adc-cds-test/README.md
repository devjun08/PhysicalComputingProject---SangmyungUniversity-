# CdS 센서 9채널 ADC 테스트

NUCLEO-F411RE에서 CdS 센서 9개를 ADC1 폴링으로 읽는 IAR EWARM 프로젝트입니다.

1. STM32F4 SPL V1.9.0의 `Libraries/` 폴더를 `USART.ewp` 옆에 둡니다.
2. IAR EWARM에서 `USART.eww`를 열고 **Debug** 구성을 선택합니다.
3. 센서 배선과 핀 번호는 [main.c](main.c) 상단을 참조합니다. IAR Watch에는 `ADC1_data`부터 `ADC9_data`까지 추가합니다.

`APPLY_ADC_TEST_ko.txt`는 이전 1채널 테스트 기록입니다.
