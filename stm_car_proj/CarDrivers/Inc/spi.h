/**
 * spi.h — bare-metal SPI1 master driver for STM32F334R8.
 *
 * Mode 0 (CPOL=0, CPHA=0), MSB first, 8-bit frames, software chip select.
 * That is the mode the nRF24L01 expects (spec §8.3: data sampled on the
 * rising SCK edge, MSBit first).
 *
 * Chip select is NOT handled here — the device driver owns its CS pin, so the
 * same bus can later carry other SPI parts.
 */
#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stddef.h>

/* Configure PA5/PA6/PA7 as SPI1 and enable the peripheral.
 * target_hz: highest SCK you want; the driver picks the nearest
 * prescaler that does not exceed it. Returns the actual SCK in Hz. */
uint32_t spi1_init(uint32_t target_hz);

/* Full-duplex: clock one byte out, return the byte clocked in. */
uint8_t  spi1_transfer(uint8_t tx);

/* Block helpers. tx may be NULL (sends 0xFF), rx may be NULL (discards). */
void     spi1_transfer_buf(const uint8_t *tx, uint8_t *rx, size_t len);

/* Wait until the last bit has left the shift register.
 * Call this before raising CS. */
void     spi1_wait_idle(void);

#endif /* SPI_H */
