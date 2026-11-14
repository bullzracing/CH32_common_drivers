#include "interrupt.h"

/* Volatile measurement storage */
static volatile uint32_t s_pulse_acc        = 0;
static volatile uint32_t s_latest_frequency = 0;
static volatile uint32_t s_total_pulses     = 0;
static volatile uint8_t  s_data_ready       = 0;

/* Function prototypes using QingKe V4B fast hardware stacking */
void EXTI1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)  __attribute__((interrupt("WCH-Interrupt-fast")));

void PulseMeasure_Init(void) {
    /* 1. Enable peripheral clocks for GPIOA, AFIO, and TIM2 */
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO;
    RCC->APB1PCENR |= RCC_APB1Periph_TIM2;                       

    /* 2. Configure PA1 as Input Pull-Up (CNF = 10b, MODE = 00b -> 0x8) */
    GPIOA->CFGLR &= ~(0x0F << (1 * 4));
    GPIOA->CFGLR |=  (0x08 << (1 * 4));
    GPIOA->OUTDR |=  (1 << 1);          // OUTDR bit 1 high selects pull-up

    /* 3. Map EXTI Line 1 to Port A (EXTI1[3:0] = 0000b) */
    AFIO->EXTICR[0] &= ~(0x0F << 4);

    /* 4. Configure EXTI Line 1 for Rising Edge Trigger */
    EXTI->INTENR |= (1 << 1);  // Enable EXTI Line 1 interrupt
    EXTI->RTENR  |= (1 << 1);  // Trigger on rising edge
    EXTI->FTENR  &= ~(1 << 1); // Disable falling edge

    /* 5. Configure TIM2 for 1.000 s Interval @ 72 MHz (FUNCONF_SYSTEM_CORE_CLOCK)
     * Clock base: 72,000,000 Hz / (7199 + 1) = 10,000 Hz (0.1 ms per tick).
     * Auto-reload: 10,000 counts = 1.000 second.
     */
    TIM2->CTLR1 = 0;
    TIM2->PSC   = (uint16_t)((FUNCONF_SYSTEM_CORE_CLOCK / 10000) - 1); // 7199
    TIM2->ATRLR = (uint16_t)(10000 - 1);                               // 9999
    TIM2->CNT   = 0;

    TIM2->INTFR     = 0;         // Clear any pending interrupt flags
    TIM2->DMAINTENR |= (1 << 0); // Enable Update Interrupt (UIE)
    TIM2->CTLR1     |= (1 << 0); // Enable Timer Counter (CEN)

    /* 6. Enable Interrupts in the QingKe PFIC */
    NVIC_EnableIRQ(EXTI1_IRQn); // Vector 23: EXTI Line 1[cite: 91, 100]
    NVIC_EnableIRQ(TIM2_IRQn);  // Vector 44: TIM2 Global[cite: 91, 100]
}

uint8_t PulseMeasure_IsReady(void) {
    return s_data_ready;
}

uint32_t PulseMeasure_GetFrequency(void) {
    /* Atomic read: temporarily mask TIM2 IRQ to avoid mid-read updates */
    NVIC_DisableIRQ(TIM2_IRQn);
    uint32_t freq = s_latest_frequency;
    s_data_ready = 0;
    NVIC_EnableIRQ(TIM2_IRQn); 

    return freq;
}

uint32_t PulseMeasure_GetTotalPulses(void) {
    /* Atomic read: temporarily mask EXTI1 IRQ to avoid mid-read updates */
    NVIC_DisableIRQ(EXTI1_IRQn);
    uint32_t total = s_total_pulses;
    NVIC_EnableIRQ(EXTI1_IRQn); 

    return total;
}

void PulseMeasure_ResetTotalPulses(void) {
    NVIC_DisableIRQ(EXTI1_IRQn);
    s_total_pulses = 0;
    NVIC_EnableIRQ(EXTI1_IRQn); 
}

/**
 * @brief EXTI1 ISR: Executes on every rising pulse edge from the sensor
 */
void EXTI1_IRQHandler(void) {
    if (EXTI->INTFR & (1 << 1)) {
        s_pulse_acc++;
        s_total_pulses++;
        EXTI->INTFR = (1 << 1); // Write 1 to clear pending flag
    }
}

/**
 * @brief TIM2 ISR: Executes once every 1.000 second to snapshot frequency
 */
void TIM2_IRQHandler(void) {
    if (TIM2->INTFR & (1 << 0)) { // Check Update Interrupt Flag (UIF)
        s_latest_frequency = s_pulse_acc;
        s_pulse_acc        = 0;
        s_data_ready       = 1;

        TIM2->INTFR = (uint16_t)~(1 << 0); // Clear UIF flag (write 0)
    }
}