/*
 * demo_pwm_turning.c
 *
 *  Created on: Nov 17, 2026
 *      Author: amaxw
 *
 *
 *      This demo will test the PWM driver on the cars on-board servo motor used for turning wheels. The driver
 *      may need to be modified for 2 main reasons:
 *      	(1) Need a function that provides fine incremental or decremental control in either wheel direction
 *      	(2) The driver was developed
 *
 *      Sweeps a servo on PA0 (TIM2_CH1, AF1) from 0 to 180 deg in 10 deg steps and back.
 *      The LED on PA12 toggles at each end of the sweep.
 *
 *      Custom-driver world: must NOT include main.h (see demos.h). HAL_Delay is declared below instead.
 */

#include <string.h>
#include "stm32f334r8.h"
#include "stm32f334xx_gpio_driver.h"
#include "stm32f334xx_pwm_driver.h"

/* HAL delay (SysTick is already running from HAL_Init() in main.c) */
extern void HAL_Delay(uint32_t Delay);

/*
 * TIMER CLOCK IS 64 MHz, NOT 8 MHz.
 * main.c runs SystemClock_Config() before this demo: HSI/2 (4 MHz) x PLL16 = 64 MHz SYSCLK,
 * APB1 = SYSCLK/2 = 32 MHz. When the APB1 prescaler is not 1, the timers on APB1 (TIM2/TIM3)
 * get 2 x PCLK1 = 64 MHz.
 *
 * Servo frame: 50 Hz (20 ms period).
 *   Counter clock = 64 MHz / (PSC + 1) = 64 MHz / 64 = 1 MHz  -> 1 tick = 1 us
 *   PWM freq      = 1 MHz / (ARR + 1)  = 1 MHz / 20000 = 50 Hz
 * With a 1 us tick, CCR1 is the pulse width in microseconds (500 = 0.5 ms, 2500 = 2.5 ms).
 *
 * const vs macro: both work. A macro (#define) is a plain text substitution with no type and no storage.
 * A const global has a type and is visible in the debugger, but it takes up a variable in flash.
 * For fixed config values like these, macros are the usual choice in embedded C. static const is
 * also fine. "static" keeps the name private to this file so it cannot clash with another demo.
 */
#define SERVO_TIM_PSC			63u		/* 64 MHz / 64 = 1 MHz counter clock */
#define SERVO_TIM_ARR			19999u	/* 1 MHz / 20000 = 50 Hz frame */

#define SERVO_MIN_PULSE_US		500u	/* pulse for 0 deg */
#define SERVO_MAX_PULSE_US		2500u	/* pulse for 180 deg */
#define SERVO_STEP_DELAY_MS		50u		/* time between 10 deg steps */

static void set_servo_angle(PWM_Handle_t* pwm_timX_chX, uint8_t angle){
	if (angle > 180){
		angle = 180; // Caps at 180 so everything functions as expected
	}
	uint32_t pulse_us = SERVO_MIN_PULSE_US
			+ ((uint32_t)angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / 180u;
	pwm_set_compare(pwm_timX_chX, pulse_us); // 1 tick = 1 us, so this is the pulse width directly
}


void demo_pwm_turn_test(void){
	// Start by initializing the GPIO_Handle_t and PWM_Handle_t structs
	GPIO_Handle_t GPIOLed;
	GPIO_Handle_t GPIOPwm;
	PWM_Handle_t pwm_tim2_ch1;

	// memset fills every byte of the struct with 0. Local structs start with random stack garbage,
	// so this makes sure any field you do not set below is 0 instead of a random value.
	memset(&pwm_tim2_ch1, 0, sizeof(pwm_tim2_ch1));
	memset(&GPIOLed, 0, sizeof(GPIOLed));
	memset(&GPIOPwm, 0, sizeof(GPIOPwm));

	/* ********************* LED (PA12) ********************* */
	GPIOLed.pGPIOx = GPIOA;
	GPIOLed.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_12;
	GPIOLed.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_OUT;
	GPIOLed.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPD_HIGH;
	GPIOLed.GPIO_PinConfig.GPIO_PinOpType = GPIO_OP_TYPE_PP; 		// Push-Pull Configuration
	GPIOLed.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_NO_PUPD;

	GPIO_PeriClockControl(GPIOA, ENABLE);
	GPIO_Init(&GPIOLed);

	/* ********************* PWM pin (PA0 = TIM2_CH1, AF1) ********************* */
	GPIOPwm.pGPIOx = GPIOA;
	GPIOPwm.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_0;
	GPIOPwm.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ALTFN;
	GPIOPwm.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPD_HIGH;
	GPIOPwm.GPIO_PinConfig.GPIO_PinOpType = GPIO_OP_TYPE_PP;
	GPIOPwm.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_NO_PUPD;
	GPIOPwm.GPIO_PinConfig.GPIO_PinAltFunMode = GPIO_ALT_FN_1;

	GPIO_Init(&GPIOPwm);

	/* ********************* TIM2 CH1 PWM ********************* */
	pwm_tim2_ch1.pTIMx = TIM2;
	pwm_tim2_ch1.PWM_PinConfig.PWM_Cnt_Dir = UPCOUNTING;
	pwm_tim2_ch1.PWM_PinConfig.PWM_PSC = SERVO_TIM_PSC;
	pwm_tim2_ch1.PWM_PinConfig.PWM_ARR = SERVO_TIM_ARR;
	pwm_tim2_ch1.PWM_PinConfig.PWM_Mode = TIM_OP_MODE_1;
	pwm_tim2_ch1.PWM_PinConfig.PWM_Preload = TIM_PRELOAD_EN;
	pwm_tim2_ch1.PWM_PinConfig.PWM_Op_Pol = TIM_POL_ACT_HI;
	pwm_tim2_ch1.PWM_PinConfig.PWM_Chn_Op_Mode = TIM_CCXS_CHN_OUT;
	pwm_tim2_ch1.PWM_PinConfig.PWM_Op_Cmp_Val = SERVO_MIN_PULSE_US;	// start at 0 deg

	pwm_PeriClockControl(ENABLE, TIM_SELECT_2);
	pwm_init(&pwm_tim2_ch1);

	pwm_start(&pwm_tim2_ch1);
	HAL_Delay(500);						// give the servo time to reach 0 deg

	while(1){
		for (uint32_t angle = 0; angle <= 180; angle += 10){
			set_servo_angle(&pwm_tim2_ch1, angle);
			HAL_Delay(SERVO_STEP_DELAY_MS);
		}

		GPIO_ToggleOutputPin(GPIOA, GPIO_PIN_NO_12);

		for (uint32_t angle = 180; angle > 0; angle -= 10){
			set_servo_angle(&pwm_tim2_ch1, angle);
			HAL_Delay(SERVO_STEP_DELAY_MS);
		}

		GPIO_ToggleOutputPin(GPIOA, GPIO_PIN_NO_12);
	}
}
