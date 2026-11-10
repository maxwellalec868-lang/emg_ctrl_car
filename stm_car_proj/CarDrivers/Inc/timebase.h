/**
 * timebase.h — millisecond tick (SysTick) and microsecond busy-wait (DWT).
 *
 * The radio needs both: delay_us() for CE pulses (>=10 us) and the 130 us
 * TX/RX settling time; millis() for link-loss timeouts.
 *
 * If your project ALREADY has a SysTick_Handler / ms counter (e.g. from your
 * earlier car project or CubeMX's HAL_GetTick), delete the SysTick parts of
 * timebase.c and make millis() return your existing counter.
 */
#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

void     timebase_init(void);      /* call once, after the clock tree is set */
uint32_t millis(void);             /* wraps after ~49 days                  */
void     delay_us(uint32_t us);
void     delay_ms(uint32_t ms);

#endif /* TIMEBASE_H */
