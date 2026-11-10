/**
 * nrf24l01_port_stm32.c — connects the portable nRF24 driver to the
 * STM32F334: SPI1 (spi.c), CE/CSN GPIO, IRQ on EXTI, delays (timebase.c).
 *
 * If you'd rather use your own GPIO driver from the car project, replace the
 * register pokes in pin_output() / nrf24_port_csn() / nrf24_port_ce() with
 * calls to it — the driver only needs these functions to behave.
 */
#include "nrf24l01_port.h"
#include "nrf24l01.h"
#include "board.h"
#include "spi.h"
#include "timebase.h"

static void pin_output(GPIO_TypeDef *port, uint32_t pin)
{
    port->MODER   = (port->MODER & ~(3u << (pin * 2u))) | (1u << (pin * 2u)); /* output */
    port->OTYPER &= ~(1u << pin);                                             /* PP     */
    port->OSPEEDR |= (3u << (pin * 2u));
    port->PUPDR  &= ~(3u << (pin * 2u));
}

void nrf24_port_init(void)
{
    RCC->AHBENR |= NRF_CSN_RCC_EN | NRF_CE_RCC_EN | RCC_AHBENR_GPIOAEN;

    /* Set levels BEFORE switching to output so CSN never glitches low. */
    NRF_CSN_GPIO->BSRR = (1u << NRF_CSN_PIN);           /* CSN high = deselected */
    NRF_CE_GPIO->BSRR  = (1u << (NRF_CE_PIN + 16u));    /* CE low   = standby    */
    pin_output(NRF_CSN_GPIO, NRF_CSN_PIN);
    pin_output(NRF_CE_GPIO,  NRF_CE_PIN);

    spi1_init(NRF_SPI_HZ);

#if NRF_USE_IRQ
    /* IRQ pin: input with pull-up (nRF drives it low when an event fires). */
    NRF_IRQ_GPIO->MODER &= ~(3u << (NRF_IRQ_PIN * 2u));
    NRF_IRQ_GPIO->PUPDR  = (NRF_IRQ_GPIO->PUPDR & ~(3u << (NRF_IRQ_PIN * 2u)))
                         | (1u << (NRF_IRQ_PIN * 2u));

    /* Route the pin to its EXTI line, falling edge. */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    uint32_t idx = NRF_IRQ_PIN >> 2, shift = (NRF_IRQ_PIN & 3u) * 4u;
    SYSCFG->EXTICR[idx] = (SYSCFG->EXTICR[idx] & ~(0xFu << shift))
                        | (NRF_IRQ_EXTI_PORT << shift);
    EXTI->FTSR |=  (1u << NRF_IRQ_PIN);
    EXTI->RTSR &= ~(1u << NRF_IRQ_PIN);
    EXTI->PR    =  (1u << NRF_IRQ_PIN);                 /* clear stale pending */
    EXTI->IMR  |=  (1u << NRF_IRQ_PIN);
    NVIC_SetPriority(NRF_IRQ_IRQn, 5);
    NVIC_EnableIRQ(NRF_IRQ_IRQn);
#endif
}

void nrf24_port_csn(uint8_t level)
{
    NRF_CSN_GPIO->BSRR = level ? (1u << NRF_CSN_PIN) : (1u << (NRF_CSN_PIN + 16u));
}

void nrf24_port_ce(uint8_t level)
{
    NRF_CE_GPIO->BSRR = level ? (1u << NRF_CE_PIN) : (1u << (NRF_CE_PIN + 16u));
}

uint8_t  nrf24_port_xfer(uint8_t byte)     { return spi1_transfer(byte); }
void     nrf24_port_spi_idle(void)         { spi1_wait_idle(); }
void     nrf24_port_delay_us(uint32_t us)  { delay_us(us); }
void     nrf24_port_delay_ms(uint32_t ms)  { delay_ms(ms); }
uint32_t nrf24_port_millis(void)           { return millis(); }

#if NRF_USE_IRQ
/* EXTI lines 5-9 share one vector. If another driver (e.g. a wheel encoder
 * on PB5-PB9) also defines EXTI9_5_IRQHandler, merge them into ONE handler
 * that checks each EXTI->PR bit. Keep this ISR tiny: no SPI here. */
void EXTI9_5_IRQHandler(void)
{
    if (EXTI->PR & (1u << NRF_IRQ_PIN)) {
        EXTI->PR = (1u << NRF_IRQ_PIN);                 /* write 1 to clear */
        nrf24_irq_notify();
    }
}
#endif
