#ifndef _ADC_H_
#define _ADC_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sampling cycle presets (ADC_SAMPTRx: 3 bits per channel) */
#define ADC_SAMPLE_1_5     0  // 1.5 cycles
#define ADC_SAMPLE_7_5     1  // 7.5 cycles
#define ADC_SAMPLE_13_5    2  // 13.5 cycles
#define ADC_SAMPLE_28_5    3  // 28.5 cycles
#define ADC_SAMPLE_41_5    4  // 41.5 cycles
#define ADC_SAMPLE_55_5    5  // 55.5 cycles (recommended general purpose)
#define ADC_SAMPLE_71_5    6  // 71.5 cycles
#define ADC_SAMPLE_239_5   7  // 239.5 cycles (high impedance sources)

/* Public API */
void     ADC_Init(void);
bool     ADC_AddChannel(uint8_t channel);
void     ADC_SetSample(uint8_t channel, uint8_t sampleTime);
void     ADC_Start(void);
void     ADC_Stop(void);
void     ADC_ClearChannels(void);
uint16_t ADC_GetValue(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* _ADC_H_ */