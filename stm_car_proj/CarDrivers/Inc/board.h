/**
 * board.h — every pin the radio uses, in one place.
 *
 * Target: STM32F334R8 (Nucleo-F334R8 Arduino-header names in brackets).
 *
 * If any of these collide with pins your motor PWM / servo / encoder drivers
 * already use, change them HERE only. Nothing else in the radio code hard-codes
 * a pin.
 *
 *   nRF24L01 pin   STM32 pin        Notes
 *   ------------   --------------   -----------------------------------------
 *   VCC            3V3              3.3 V ONLY (1.9–3.6 V). 5 V kills it.
 *   GND            GND
 *   SCK            PA5  [D13]       SPI1_SCK  (AF5)  — shares Nucleo LED LD2
 *   MISO           PA6  [D12]       SPI1_MISO (AF5)
 *   MOSI           PA7  [D11]       SPI1_MOSI (AF5)
 *   CSN            PB6  [D10]       plain GPIO output, active low
 *   CE             PC7  [D9]        plain GPIO output, active high
 *   IRQ            PA8  [D7]        GPIO input + EXTI8, active low (optional)
 *
 * PA5/PA6/PA7 are 3.3 V-only (TTa) pins. That is fine: the nRF24L01 runs at
 * 3.3 V, so nothing on this bus is ever 5 V.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f3xx.h"

/* ---- SPI1 bus (fixed by the alternate-function table, AF5) ---- */
#define NRF_SPI             SPI1
#define NRF_SPI_GPIO        GPIOA
#define NRF_SCK_PIN         5u
#define NRF_MISO_PIN        6u
#define NRF_MOSI_PIN        7u
#define NRF_SPI_AF          5u

/* SPI clock target. nRF24L01 allows up to 8 MHz (4 MHz with heavy load).
 * 2 MHz is a safe start on jumper wires; raise it once the link works. */
#define NRF_SPI_HZ          2000000u

/* ---- Chip select (CSN) ---- */
#define NRF_CSN_GPIO        GPIOB
#define NRF_CSN_PIN         6u
#define NRF_CSN_RCC_EN      RCC_AHBENR_GPIOBEN

/* ---- Chip enable (CE) ---- */
#define NRF_CE_GPIO         GPIOC
#define NRF_CE_PIN          7u
#define NRF_CE_RCC_EN       RCC_AHBENR_GPIOCEN

/* ---- Interrupt (IRQ) — set NRF_USE_IRQ to 0 to poll instead ---- */
#define NRF_USE_IRQ         1
#define NRF_IRQ_GPIO        GPIOA
#define NRF_IRQ_PIN         8u
#define NRF_IRQ_EXTI_PORT   0u          /* SYSCFG_EXTICR code: 0 = port A */
#define NRF_IRQ_IRQn        EXTI9_5_IRQn

#endif /* BOARD_H */
