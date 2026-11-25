#ifndef HIT_DETECTOR_H
#define HIT_DETECTOR_H
#include <stdint.h>
void HitDetector_Reset(void);
/* 1..9 for a confirmed new hit, 0 otherwise. Values are raw ADC counts. */
uint8_t HitDetector_Update(const uint16_t values[9], uint32_t now);
#endif
