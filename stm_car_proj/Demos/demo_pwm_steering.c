/*
 * demo_pwm_steering.c
 *
 *  Created on: Nov 17, 2026
 *      Author: amaxw
 *
 *      Steering servo demo for the car (LD-1501MG). Eases the wheels from center to full right,
 *      to full left, and back to center at half the speed of demo_pwm_turning.c.
 *      The on-board LED LD2 (PA5) toggles at each end of the sweep.
 *
 *      Servo signal: PA0 (TIM2_CH1, AF1).
 *
 *      LD-1501MG vs HPS-2018 (bench servo): both use 500-2500 us -> 0-180 deg at 50 Hz, so the
 *      timer settings and PWM driver are the same. The differences handled here are mechanical:
 *        - The steering linkage cannot turn 180 deg. Every command is clamped to +/- STEER_MAX_DEG
 *          around center so the servo never pushes into the linkage stops (stall current is 2.4-3 A).
 *        - The horn sits on splines, so "straight ahead" is not exactly 90 deg. STEER_TRIM_US fixes that.
 *        - The demo starts at center, not 0 deg, so the wheels do not slam to full lock at power-up.
 *
 *      CALIBRATION (wheels off the ground):
 *        1. Adjust STEER_TRIM_US until the wheels point straight during the 1 s center hold.
 *        2. Raise STEER_MAX_DEG a few degrees at a time until the wheels stop just short of the
 *           linkage stops. If the servo buzzes at either end, it is pushing a stop: back off.
 *
 *
 *      Custom-driver world: must NOT include main.h (see demos.h). HAL_Delay is declared below instead.
 */

#include <string.h>
#include "stm32f334r8.h"
#include "stm32f334xx_gpio_driver.h"
#include "stm32f334xx_pwm_driver.h"

/* HAL delay (SysTick is already running from HAL_Init() in main.c) */
extern void HAL_Delay(uint32_t Delay);

/* On-board LED LD2 on the Nucleo-F334R8 is PA5 (LD2_Pin in main.h) */
#define LED_PORT				GPIOA
#define LED_PIN					GPIO_PIN_NO_5

/* Timer: 64 MHz / (63 + 1) = 1 MHz tick (1 us), 1 MHz / 20000 = 50 Hz frame. See demo_pwm_turning.c. */
#define SERVO_TIM_PSC			63u
#define SERVO_TIM_ARR			19999u

/* LD-1501MG pulse range (same as HPS-2018): 500 us = 0 deg, 2500 us = 180 deg */
#define SERVO_MIN_PULSE_US		500
#define SERVO_MAX_PULSE_US		2500

/* Steering geometry: CALIBRATE THESE ON THE CAR */
#define STEER_CENTER_DEG		90		/* servo angle for straight ahead before trim */
#define STEER_TRIM_US			0		/* fine adjustment so the wheels point straight (+/- us) */
#define STEER_MAX_DEG			10		/* travel each side of center; start small and widen */

/* Speed: 2 deg every 20 ms = 100 deg/s, half of demo_pwm_turning.c (10 deg / 50 ms = 200 deg/s).
 * 20 ms is one PWM frame, so the servo gets a new position every frame. */
#define STEER_STEP_DEG			2
#define STEER_STEP_MS			20u
#define STEER_HOLD_MS			1000u	/* pause at center and at each end */

/* Converts an offset from center (negative = left, positive = right) to a pulse width in us.
 * Clamped to +/- STEER_MAX_DEG so no caller can drive the linkage into its stops. */
static uint32_t steer_offset_to_pulse(int32_t offset_deg){
	if (offset_deg > STEER_MAX_DEG){
		offset_deg = STEER_MAX_DEG;
	}
	else if (offset_deg < -STEER_MAX_DEG){
		offset_deg = -STEER_MAX_DEG;
	}

	int32_t angle = STEER_CENTER_DEG + offset_deg;
	int32_t pulse = SERVO_MIN_PULSE_US
			+ (angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / 180
			+ STEER_TRIM_US;

	// Last line of defence: never leave the servo's rated pulse range, even with a bad trim value
	if (pulse < SERVO_MIN_PULSE_US){
		pulse = SERVO_MIN_PULSE_US;
	}
	else if (pulse > SERVO_MAX_PULSE_US){
		pulse = SERVO_MAX_PULSE_US;
	}
	return (uint32_t)pulse;
}

static void steer_set(PWM_Handle_t* pwm, int32_t offset_deg){
	pwm_set_compare(pwm, steer_offset_to_pulse(offset_deg));
}

/* Moves from one offset to another in STEER_STEP_DEG steps, one step per STEER_STEP_MS. */
static void steer_ease(PWM_Handle_t* pwm, int32_t from_deg, int32_t to_deg){
	int32_t step = (to_deg > from_deg) ? STEER_STEP_DEG : -STEER_STEP_DEG;
	int32_t pos = from_deg;

	while (pos != to_deg){
		pos += step;
		// Do not overshoot the target if the distance is not a multiple of the step
		if ((step > 0 && pos > to_deg) || (step < 0 && pos < to_deg)){
			pos = to_deg;
		}
		steer_set(pwm, pos);
		HAL_Delay(STEER_STEP_MS);
	}
}


void demo_pwm_steering_run(void){
	GPIO_Handle_t GPIOLed;
	GPIO_Handle_t GPIOPwm;
	PWM_Handle_t pwm_tim2_ch1;

	memset(&pwm_tim2_ch1, 0, sizeof(pwm_tim2_ch1));
	memset(&GPIOLed, 0, sizeof(GPIOLed));
	memset(&GPIOPwm, 0, sizeof(GPIOPwm));

	/* ********************* LED (LD2 = PA5) ********************* */
	GPIOLed.pGPIOx = LED_PORT;
	GPIOLed.GPIO_PinConfig.GPIO_PinNumber = LED_PIN;
	GPIOLed.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_OUT;
	GPIOLed.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPD_HIGH;
	GPIOLed.GPIO_PinConfig.GPIO_PinOpType = GPIO_OP_TYPE_PP;
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
	pwm_tim2_ch1.PWM_PinConfig.PWM_Op_Cmp_Val = steer_offset_to_pulse(0);	// start straight ahead

	pwm_PeriClockControl(ENABLE, TIM_SELECT_2);
	pwm_init(&pwm_tim2_ch1);
	pwm_start(&pwm_tim2_ch1);

	while(1){
		HAL_Delay(STEER_HOLD_MS);									// hold center

		steer_ease(&pwm_tim2_ch1, 0, STEER_MAX_DEG);				// center -> full right
		GPIO_ToggleOutputPin(LED_PORT, LED_PIN);
		HAL_Delay(STEER_HOLD_MS);

		steer_ease(&pwm_tim2_ch1, STEER_MAX_DEG, -STEER_MAX_DEG);	// full right -> full left
		GPIO_ToggleOutputPin(LED_PORT, LED_PIN);
		HAL_Delay(STEER_HOLD_MS);

		steer_ease(&pwm_tim2_ch1, -STEER_MAX_DEG, 0);				// full left -> center
	}
}
