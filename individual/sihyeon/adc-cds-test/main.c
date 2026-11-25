/*
 * NUCLEO-F411RE - CdS sensor x9 ADC polling test
 * IAR EWARM + STM32F4 Standard Peripheral Library (SPL)
 *
 * Wiring for EACH sensor:
 *   3.3V -- 10k ohm resistor -- ADC pin -- CdS -- GND
 *   All sensors can share 3.3V and GND breadboard rails.
 *
 * Sensor 1: A0 (PA0)    ADC_Channel_0
 * Sensor 2: A1 (PA1)    ADC_Channel_1
 * Sensor 3: A2 (PA4)    ADC_Channel_4
 * Sensor 4: A3 (PB0)    ADC_Channel_8
 * Sensor 5: A4 (PC1)    ADC_Channel_11 (default board routing)
 * Sensor 6: A5 (PC0)    ADC_Channel_10 (default board routing)
 * Sensor 7: CN10 pin 24 (PB1) ADC_Channel_9
 * Sensor 8: CN7 pin 35  (PC2) ADC_Channel_12
 * Sensor 9: CN7 pin 37  (PC3) ADC_Channel_13
 *
 * IMPORTANT: Backup the existing main.c before replacing it.
 * This program only tests ADC via polling (no USART, DMA, or timer).
 * It has not been compiled with the user's IAR toolchain or run on hardware.
 */

#include "stm32f4xx.h"
#include "stm32f4xx_adc.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"

#if !defined(STM32F411xE)
#error Select STM32F411xE in the project's preprocessor definitions.
#endif

/* Add ADC1_data ... ADC9_data to IAR Watch / Live Watch. */
volatile uint16_t ADC1_data = 0U;
volatile uint16_t ADC2_data = 0U;
volatile uint16_t ADC3_data = 0U;
volatile uint16_t ADC4_data = 0U;
volatile uint16_t ADC5_data = 0U;
volatile uint16_t ADC6_data = 0U;
volatile uint16_t ADC7_data = 0U;
volatile uint16_t ADC8_data = 0U;
volatile uint16_t ADC9_data = 0U;

volatile uint32_t adc_cnt  = 0U;       /* number of complete 9-sensor reads */
volatile uint8_t  adc_error = 0U;      /* 0=OK, 1=conversion timeout */
volatile uint32_t clock_hz = 0U;

static void ShortWait(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0U; i < count; ++i) {
        __NOP();
    }
}

static void InitADC1GPIO(void)
{
    GPIO_InitTypeDef gpio;
    ADC_InitTypeDef adc;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA |
                           RCC_AHB1Periph_GPIOB |
                           RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Mode = GPIO_Mode_AN;
    gpio.GPIO_PuPd = GPIO_PuPd_NOPULL;

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4;
    GPIO_Init(GPIOA, &gpio); /* PA0, PA1, PA4 */

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOB, &gpio); /* PB0, PB1 */

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_Init(GPIOC, &gpio); /* PC0, PC1, PC2, PC3 */

    ADC_StructInit(&adc);
    adc.ADC_Resolution = ADC_Resolution_12b;
    adc.ADC_ScanConvMode = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    adc.ADC_DataAlign = ADC_DataAlign_Right;
    adc.ADC_NbrOfConversion = 1U;
    ADC_Init(ADC1, &adc);
    ADC_Cmd(ADC1, ENABLE);
    ShortWait(10000U);
}

static uint16_t ReadADCChannel(uint8_t channel)
{
    uint32_t timeout = 1000000U;

    ADC_ClearFlag(ADC1, ADC_FLAG_EOC | ADC_FLAG_OVR);
    ADC_RegularChannelConfig(ADC1, channel, 1U, ADC_SampleTime_480Cycles);
    ADC_SoftwareStartConv(ADC1);

    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {
        if (timeout-- == 0U) {
            adc_error = 1U;
            return 0xFFFFU;  /* invalid result: not a real 12-bit ADC value */
        }
    }
    return ADC_GetConversionValue(ADC1);
}

int main(void)
{
    SystemCoreClockUpdate();
    clock_hz = SystemCoreClock;
    InitADC1GPIO();

    while (1) {
        adc_error = 0U;

        ADC1_data = ReadADCChannel(ADC_Channel_0);   /* A0 / PA0 */
        ADC2_data = ReadADCChannel(ADC_Channel_1);   /* A1 / PA1 */
        ADC3_data = ReadADCChannel(ADC_Channel_4);   /* A2 / PA4 */
        ADC4_data = ReadADCChannel(ADC_Channel_8);   /* A3 / PB0 */
        ADC5_data = ReadADCChannel(ADC_Channel_11);  /* A4 / PC1 */
        ADC6_data = ReadADCChannel(ADC_Channel_10);  /* A5 / PC0 */
        ADC7_data = ReadADCChannel(ADC_Channel_9);   /* CN10-24 / PB1 */
        ADC8_data = ReadADCChannel(ADC_Channel_12);  /* CN7-35 / PC2 */
        ADC9_data = ReadADCChannel(ADC_Channel_13);  /* CN7-37 / PC3 */

        adc_cnt++;
        ShortWait(10000U);
        
    }
}
