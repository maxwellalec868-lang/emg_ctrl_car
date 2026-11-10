/**
 * spi.c — bare-metal SPI1 master for STM32F334R8 (see spi.h).
 *
 * Reference: RM0364 (STM32F334 reference manual), SPI chapter.
 *
 * Two F3-specific gotchas this file handles:
 *  1. The F3 SPI has a 32-bit FIFO and a programmable frame size (DS bits).
 *     A 16-bit write to DR (e.g. `SPI1->DR = x;` through a uint16_t/uint32_t
 *     pointer) packs TWO 8-bit frames. We must access DR as a byte.
 *  2. FRXTH must be set so RXNE fires after 1 byte instead of 2.
 */
#include "spi.h"
#include "board.h"

static uint32_t pclk2_hz(void)
{
    SystemCoreClockUpdate();                 /* refresh from RCC registers */
    uint32_t ppre2 = (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos;
    /* PPRE2: 0xx = /1, 100 = /2, 101 = /4, 110 = /8, 111 = /16 */
    uint32_t div = (ppre2 < 4u) ? 1u : (1u << (ppre2 - 3u));
    return SystemCoreClock / div;
}

static void gpio_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af)
{
    port->MODER   = (port->MODER   & ~(3u << (pin * 2u))) | (2u << (pin * 2u)); /* AF   */
    port->OTYPER &= ~(1u << pin);                                               /* PP   */
    port->OSPEEDR |= (3u << (pin * 2u));                                        /* high */
    port->PUPDR  &= ~(3u << (pin * 2u));                                        /* none */
    volatile uint32_t *afr = &port->AFR[pin >> 3];
    uint32_t shift = (pin & 7u) * 4u;
    *afr = (*afr & ~(0xFu << shift)) | (af << shift);
}

uint32_t spi1_init(uint32_t target_hz)
{
    RCC->AHBENR  |= RCC_AHBENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;                      /* short delay after clock enable */

    gpio_af(NRF_SPI_GPIO, NRF_SCK_PIN,  NRF_SPI_AF);
    gpio_af(NRF_SPI_GPIO, NRF_MISO_PIN, NRF_SPI_AF);
    gpio_af(NRF_SPI_GPIO, NRF_MOSI_PIN, NRF_SPI_AF);
    /* weak pull-up on MISO so it doesn't float when the nRF tri-states it */
    NRF_SPI_GPIO->PUPDR |= (1u << (NRF_MISO_PIN * 2u));

    /* Pick BR[2:0]: SCK = PCLK2 / 2^(BR+1). Smallest divider <= target. */
    uint32_t pclk = pclk2_hz();
    uint32_t br = 0;
    while (br < 7u && (pclk >> (br + 1u)) > target_hz) {
        br++;
    }

    NRF_SPI->CR1 = 0;                        /* disable while configuring */
    NRF_SPI->CR1 = SPI_CR1_MSTR              /* master                     */
                 | SPI_CR1_SSM | SPI_CR1_SSI /* software NSS, keep master  */
                 | (br << SPI_CR1_BR_Pos);   /* CPOL=0 CPHA=0 MSB first    */
    NRF_SPI->CR2 = (7u << SPI_CR2_DS_Pos)    /* DS = 0111 -> 8-bit frames  */
                 | SPI_CR2_FRXTH;            /* RXNE at 1 byte in FIFO     */
    NRF_SPI->CR1 |= SPI_CR1_SPE;

    return pclk >> (br + 1u);
}

uint8_t spi1_transfer(uint8_t tx)
{
    while (!(NRF_SPI->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&NRF_SPI->DR = tx;  /* 8-bit access: see header note */
    while (!(NRF_SPI->SR & SPI_SR_RXNE)) { }
    return *(volatile uint8_t *)&NRF_SPI->DR;
}

void spi1_transfer_buf(const uint8_t *tx, uint8_t *rx, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        uint8_t in = spi1_transfer(tx ? tx[i] : 0xFFu);
        if (rx) {
            rx[i] = in;
        }
    }
}

void spi1_wait_idle(void)
{
    while (NRF_SPI->SR & SPI_SR_FTLVL) { }   /* TX FIFO empty  */
    while (NRF_SPI->SR & SPI_SR_BSY)   { }   /* shifter idle   */
}
