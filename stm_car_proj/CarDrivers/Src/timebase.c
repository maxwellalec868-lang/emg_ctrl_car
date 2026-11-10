/**
 * timebase.c — see timebase.h.
 */
#include "timebase.h"
#include "stm32f3xx.h"

static volatile uint32_t s_ms;

void timebase_init(void)
{
    SystemCoreClockUpdate();

    /* 1 kHz SysTick */
    SysTick_Config(SystemCoreClock / 1000u);

    /* DWT cycle counter for microsecond delays (Cortex-M4 core feature,
     * doesn't use up a hardware timer you might need for PWM). */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

void SysTick_Handler(void)
{
    s_ms++;
}

uint32_t millis(void)
{
    return s_ms;
}

void delay_us(uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000u);
    while ((DWT->CYCCNT - start) < cycles) { }   /* wrap-safe subtraction */
}

void delay_ms(uint32_t ms)
{
    uint32_t start = millis();
    while ((millis() - start) < ms) { }
}
