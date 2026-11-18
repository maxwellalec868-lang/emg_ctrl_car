/*
 * demos.h
 *
 * One entry point per demo. main.c does the CubeMX init (HAL, clock, GPIO, SPI)
 * and then calls exactly ONE of these. None of them return.
 *
 * RULE: a demo .c file must NOT include both "main.h" (HAL/CMSIS) and my custom
 * driver headers (stm32f334r8.h and everything that includes it). They define the
 * same names (GPIOA, I2C1, RCC, ENABLE, SET, GPIO_PIN_SET, ...) differently and the
 * file will not compile. Pick one world per demo file:
 *   - HAL world:    main.h + nrf24l01.h                   (e.g. demo_nrf_rx_led.c)
 *   - Custom world: custom driver headers + nrf24l01.h    (e.g. demo_emdm.c)
 * nrf24l01.h is safe in both because it only includes <stdint.h>/<stdbool.h>.
 *
 * This header has no includes on purpose so it is safe to include from anywhere.
 */

#ifndef DEMOS_H_
#define DEMOS_H_

/* demo_nrf_rx_led.c: radio test. LD2 on while the first payload byte is 0x45. */
void demo_nrf_rx_led_run(void);

/* demo_emdm.c: EMDM motor test with no radio. Spins M1, then M3, then loops a drive sequence. */
void demo_emdm_bench_run(void);

/* demo_emdm.c: full system. Radio commands (FORWARD/SLOW/STOP) drive the motors. */
void demo_emdm_radio_run(void);

/* demo_pwm_turning.c: Test the turning capabilities of the car using the PWM driver*/
void demo_pwm_turn_test(void);

#endif /* DEMOS_H_ */
