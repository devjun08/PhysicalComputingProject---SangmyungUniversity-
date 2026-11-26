#include "stm32f4xx.h"
#include "sensors.h"
#include "timebase.h"
volatile uint8_t adc_error;
static void ShortWait(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0U; i < count; ++i) {
        __NOP();
    }
}

/* ---------- CdS 9채널 GPIO 및 ADC1 초기화 ---------- */
void Sensors_Init(void)
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

/* ---------- 채널 하나를 변환하고 완료 플래그 확인 ---------- */
static uint16_t ReadADCChannel(uint8_t channel)
{
    uint32_t started = Timebase_Now();

    ADC_ClearFlag(ADC1, ADC_FLAG_EOC | ADC_FLAG_OVR);
    ADC_RegularChannelConfig(ADC1, channel, 1U, ADC_SampleTime_480Cycles);
    ADC_SoftwareStartConv(ADC1);

    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {
        if (Timebase_Now() - started >= 2U) {
            adc_error = 1U;
            return 0xFFFFU;  /* invalid result: not a real 12-bit ADC value */
        }
    }
    return ADC_GetConversionValue(ADC1);
}


/* ---------- 표적 번호 순서대로 9채널 읽기 ---------- */
uint8_t Sensors_Read(uint16_t values[9])
{
    static const uint8_t channels[9] = {ADC_Channel_0, ADC_Channel_1,
        ADC_Channel_4, ADC_Channel_8, ADC_Channel_11, ADC_Channel_10,
        ADC_Channel_9, ADC_Channel_12, ADC_Channel_13};
    uint8_t i;
    adc_error = 0U;
    for (i = 0U; i < 9U; ++i) {
        values[i] = ReadADCChannel(channels[i]);
        if (adc_error) return 0U;
    }
    return 1U;
}
