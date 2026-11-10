/**
 * nrf24l01_port_stm32.c — connects the portable nRF24 driver (nrf24l01.c)
 * to the STM32F334 using HAL.
 *
 * SPI1 and the CSN/CE pins are configured by CubeMX (MX_SPI1_Init,
 * MX_GPIO_Init in main.c) and must be initialised before nrf24_init().
 *   SPI1: PB3 = SCK, PB4 = MISO, PB5 = MOSI  (mode 0, 2 MHz)
 *   CSN : PB6 (NRF_CSN), active low
 *   CE  : PC7 (NRF_CE),  active high
 */
#include "main.h"
#include "nrf24l01_port.h"

extern SPI_HandleTypeDef hspi1;   /* defined in main.c by CubeMX */

void nrf24_port_init(void)
{
    /* Enable the DWT cycle counter for microsecond delays (HAL has none). */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void nrf24_port_csn(uint8_t level)
{
    HAL_GPIO_WritePin(NRF_CSN_GPIO_Port, NRF_CSN_Pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void nrf24_port_ce(uint8_t level)
{
    HAL_GPIO_WritePin(NRF_CE_GPIO_Port, NRF_CE_Pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

uint8_t nrf24_port_xfer(uint8_t byte)
{
    uint8_t rx = 0;
    HAL_SPI_TransmitReceive(&hspi1, &byte, &rx, 1, 10);
    return rx;
}

/* HAL_SPI_TransmitReceive() already waits until the bus is idle. */
void nrf24_port_spi_idle(void) { }

void nrf24_port_delay_us(uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000u);
    while ((DWT->CYCCNT - start) < cycles) { }
}

void     nrf24_port_delay_ms(uint32_t ms) { HAL_Delay(ms); }
uint32_t nrf24_port_millis(void)          { return HAL_GetTick(); }
