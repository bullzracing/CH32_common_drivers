#include "ADC.h"
#include "ch32fun.h"
#include <stdio.h>
#include <stdbool.h>

#define MAX_CONFIGURED_CHANNELS 16
#define TOTAL_ADC_CHANNELS      18 // Channels 0 to 17 (including internal sensors)

static uint8_t           channelList[MAX_CONFIGURED_CHANNELS];
static volatile uint16_t convertedValues[TOTAL_ADC_CHANNELS];
static volatile uint8_t  channelCount = 0;
static volatile uint8_t  currentIndex = 0;
static volatile bool     adcRunning   = false;

/* Helper: Configure target pin to Analog Input (MODE=00b, CNF=00b) */
static void ADC_PinConfig(uint8_t channel)
{
    if (channel <= 7) // PA0 - PA7 (Channels 0 - 7)
    {
        RCC->APB2PCENR |= (1 << 2);                 // Enable GPIOA clock
        GPIOA->CFGLR &= ~(0x0F << (channel * 4));   // MODE=00, CNF=00
    }
    else if (channel == 8 || channel == 9) // PB0, PB1 (Channels 8 - 9)
    {
        RCC->APB2PCENR |= (1 << 3);                 // Enable GPIOB clock
        GPIOB->CFGLR &= ~(0x0F << ((channel - 8) * 4));
    }
    else if (channel >= 10 && channel <= 15) // PC0 - PC5 (Channels 10 - 15)
    {
        RCC->APB2PCENR |= (1 << 4);                 // Enable GPIOC clock
        GPIOC->CFGLR &= ~(0x0F << ((channel - 10) * 4));
    }
    else if (channel == 16 || channel == 17)
    {
        // Internal Temp Sensor (16) and VREFINT (17) require TSVREFE power-on
        ADC1->CTLR2 |= (1 << 23);                   // TSVREFE: Enable Temp sensor & Vref
    }
}

void ADC_Init(void)
{
    /* 1. Enable ADC clock (Bit 9 of APB2PCENR) in {clock.c} */
    /* 2. ADCCLK Prescaler: PCLK2 / 6 (72MHz / 6 = 12MHz <= 14MHz max) in clock.c */
    /* 3. Power on ADC analog circuitry */
    ADC1->CTLR2 |= (1 << 0);                        // ADON = 1
    Delay_Us(10);                                   // Stabilization delay
    /* 4. Reset & run hardware calibration */
    ADC1->CTLR2 |= (1 << 3);                        // RSTCAL = 1
    while (ADC1->CTLR2 & (1 << 3));
    ADC1->CTLR2 |= (1 << 2);                        // CAL = 1
    while (ADC1->CTLR2 & (1 << 2));
    /* 5. Enable End-of-Conversion Interrupt (Bit 5 of CTLR1) */
    ADC1->CTLR1 |= (1 << 5);
    /* 6. Enable ADC interrupt in PFIC (IRQ34 is ADC1_2_IRQn) */
    NVIC_EnableIRQ(ADC1_2_IRQn);
}
bool ADC_AddChannel(uint8_t channel)
{
    if (channelCount >= MAX_CONFIGURED_CHANNELS || channel >= TOTAL_ADC_CHANNELS)
    {
        return false;
    }

    ADC_PinConfig(channel);
    ADC_SetSample(channel, ADC_SAMPLE_55_5);

    channelList[channelCount] = channel;
    channelCount++;
    return true;
}

void ADC_SetSample(uint8_t channel, uint8_t sampleTime)
{
    sampleTime &= 0x07;
    if (channel <= 9)
    {
        ADC1->SAMPTR2 &= ~(0x07 << (channel * 3));
        ADC1->SAMPTR2 |=  (sampleTime << (channel * 3));
    }
    else if (channel <= 17)
    {
        ADC1->SAMPTR1 &= ~(0x07 << ((channel - 10) * 3));
        ADC1->SAMPTR1 |=  (sampleTime << ((channel - 10) * 3));
    }
}

void ADC_Start(void)
{
    if (channelCount == 0) return;

    adcRunning = true;
    currentIndex = 0;

    /* Sequence length: 1 conversion (L[3:0] in RSQR1 = 0000b) */
    ADC1->RSQR1 &= ~(0x0F << 20);

    /* Set first channel in sequence Rank 1 */
    ADC1->RSQR3 = (channelList[0] & 0x1F);

    /* Select software trigger (EXTSEL = 111b) and enable external trigger (EXTTRIG = 1) */
    ADC1->CTLR2 &= ~(0x07 << 17);
    ADC1->CTLR2 |=  (0x07 << 17) | (1 << 20);

    /* Clear lingering status flags, then fire software conversion */
    ADC1->STATR = 0;
    ADC1->CTLR2 |= (1 << 22);                       // SWSTART = 1
}

void ADC_Stop(void)
{
    adcRunning = false;
    ADC1->CTLR2 &= ~(1 << 22);                      // Clear SWSTART
}

void ADC_ClearChannels(void)
{
    ADC_Stop();
    channelCount = 0;
    currentIndex = 0;
}

uint16_t ADC_GetValue(uint8_t channel)
{
    if (channel < TOTAL_ADC_CHANNELS)
    {
        return convertedValues[channel];
    }
    return 0;
}

/* QingKe RISC-V fast hardware interrupt */
void ADC1_2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void ADC1_2_IRQHandler(void)
{
    if (ADC1->STATR & (1 << 1))                     // EOC flag (Bit 1)
    {
        /* Reading RDATAR clears the EOC flag automatically */
        uint16_t val = (uint16_t)(ADC1->RDATAR & 0x0FFF);
        convertedValues[channelList[currentIndex]] = val;

        if (!adcRunning)
        {
            return;
        }

        /* Advance to next registered channel */
        currentIndex++;
        if (currentIndex >= channelCount)
        {
            currentIndex = 0;
        }

        /* Queue the next channel in Rank 1 */
        ADC1->RSQR3 = (channelList[currentIndex] & 0x1F);

        /* Start next conversion */
        ADC1->CTLR2 |= (1 << 22);                   // SWSTART = 1
    }
}