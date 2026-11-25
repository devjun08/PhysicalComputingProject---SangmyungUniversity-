#ifndef SENSORS_H
#define SENSORS_H
#include <stdint.h>
void Sensors_Init(void);
uint8_t Sensors_Read(uint16_t values[9]);
extern volatile uint8_t adc_error;
#endif
