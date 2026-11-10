/**
 * nrf24l01_port.h — the ONLY hardware the nRF24 driver touches.
 *
 * nrf24l01.c is plain C with no STM32 registers in it. Everything
 * board-specific goes through these functions, which are implemented in
 * nrf24l01_port_stm32.c. (That split is also what lets the driver be unit
 * tested on a PC against a fake chip.)
 */
#ifndef NRF24L01_PORT_H
#define NRF24L01_PORT_H

#include <stdint.h>

void     nrf24_port_init(void);          /* SPI + CE/CSN GPIO + optional IRQ */
void     nrf24_port_csn(uint8_t level);  /* 0 = select chip                 */
void     nrf24_port_ce(uint8_t level);   /* 1 = radio active (RX or TX)     */
uint8_t  nrf24_port_xfer(uint8_t byte);  /* one full-duplex SPI byte        */
void     nrf24_port_spi_idle(void);      /* wait for SPI to finish          */
void     nrf24_port_delay_us(uint32_t us);
void     nrf24_port_delay_ms(uint32_t ms);
uint32_t nrf24_port_millis(void);

#endif /* NRF24L01_PORT_H */
