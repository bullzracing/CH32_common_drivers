#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stdint.h>
#include "ch32fun.h"

#ifdef __cplusplus
extern "C" {
#endif

void PulseMeasure_Init(void);

uint8_t PulseMeasure_IsReady(void);

uint32_t PulseMeasure_GetFrequency(void);

uint32_t PulseMeasure_GetTotalPulses(void);

void PulseMeasure_ResetTotalPulses(void);

#ifdef __cplusplus
}
#endif

#endif /* _INTERRUPT_H_ */