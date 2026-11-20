/*
 * full_drive_demo.c
 *
 *  Created on: Nov 19, 2026
 *      Author: amaxw
 *
 *
 *      Full keyboard drive demo. Speed (EMDM over I2C) and steering (servo PWM) at the same time:
 *      (1): pc_drive.py reads the held keys on the PC and sends a 1-byte bitmask to the Arduino every 50 ms
 *      (2): The Arduino (nrf_tx_drive.ino) forwards the byte over the nRF radio; the STM32 reads it over SPI
 *      (3): Every CTRL_PERIOD_MS the STM32 ramps speed (drive_update) and steering (steer_update) from the mask
 *
 *      Payload byte 0 (bitmask of keys held right now). Must match arduino_emg_acquisition/nrf_tx_drive/pc_drive.py
 *        0x01 W     = accelerate (speed holds when released = cruise)
 *        0x02 S     = slow down to 0 (never reverses). Wins over W
 *        0x04 A     = steer left  (wheels ease back to center when released)
 *        0x08 D     = steer right
 *        0x10 Space = EMERGENCY STOP: speed 0 and wheels centered at once (Esc in pc_drive.py also sends this)
 *
 *      FAILSAFE: no packet for KEY_TIMEOUT_MS (script closed, USB pulled, radio lost) = emergency stop.
 *
 *      Pins:
 *      (1): I2C1 PB8 = SCL, PB9 = SDA (EMDM)   - emdm_bringup()
 *      (2): SPI1 PB3/4/5 + CE/CSN (nRF radio)   - CubeMX MX_SPI1_Init(), used by nRF_radio_bringup()
 *      (3): PA0 = TIM2_CH1 (steering servo)     - pwm_bringup()
 *
 *      LD2: on while moving, off while stopped, fast blink = error (EMDM I2C or radio init).
 *      Watch in Live Expressions: keys, steer_deg, emdm.EMDM_DRIVE_SPEED, speed_limit, cmd_rx_count, emdm_status, battery_raw.
 *      Globals are static (demo_emdm.c uses the same names). If the debugger shows the wrong one: 'full_drive_demo.c'::emdm
 *
 *      Custom-driver world: must NOT include main.h (see demos.h). HAL_Delay/HAL_GetTick are declared below instead.
 *      TEST WITH THE WHEELS OFF THE GROUND FIRST.
 */


#include "stm32f334xx_gpio_driver.h"
#include "stm32f334xx_pwm_driver.h"
#include "emdm_device_driver.h"
#include "nrf24l01.h"
#include "demos.h"

/* HAL delay/tick (SysTick is already running from HAL_Init() in main.c) */
extern void     HAL_Delay(uint32_t Delay);
extern uint32_t HAL_GetTick(void);

/* ---------------- Start Settings ---------------- */
#define LD2_PIN             GPIO_PIN_NO_5   // LD2 = PA5 (already an output from MX_GPIO_Init())

// I2C pull-up: GPIO_PU for now. If the EMDM pulls SDA/SCL up to 5 V, change to GPIO_NO_PUPD (see docs/future_work.md #1)
#define EMDM_I2C_PUPD       GPIO_PU

// Radio: must match the Arduino sketch (same as demo_nrf_rx_led)
#define RADIO_ADDR          ((const uint8_t *)"JOY01")
#define RADIO_CHANNEL       76

// Key bits (byte 0 of the payload). Must match arduino_emg_acquisition/nrf_tx_drive/pc_drive.py
#define KEY_FWD             0x01
#define KEY_SLOW            0x02
#define KEY_LEFT            0x04
#define KEY_RIGHT           0x08
#define KEY_ESTOP           0x10

// Ramp rates: one step per control tick while the key is held
#define CTRL_PERIOD_MS      50u     // control tick
#define ACCEL_STEP          1       // W:   20 units/s -> 0..30 in 1.5 s
#define DECEL_STEP          2       // S:   40 units/s -> 30..0 in 0.75 s
#define STEER_STEP_DEG      1       // A/D: 20 deg/s   -> center..full lock in 0.5 s
#define STEER_RETURN_DEG    1       // released: back to center at 20 deg/s
#define KEY_TIMEOUT_MS      200u    // no packet for this long -> link lost -> emergency stop
#define DRIVE_SPEED_LIMIT   30      // starting top speed for testing (0..EMDM_SPEED_MAX = 30). Live-editable: speed_limit

// Servo timer + steering calibration: copied from demo_pwm_steering.c (keep them in sync)
#define SERVO_TIM_PSC       63u     // 64 MHz / 64 = 1 MHz -> 1 tick = 1 us
#define SERVO_TIM_ARR       19999u  // 1 MHz / 20000 = 50 Hz frame
#define SERVO_MIN_PULSE_US  500
#define SERVO_MAX_PULSE_US  2500
#define STEER_CENTER_DEG    90
#define STEER_TRIM_US       0
#define STEER_MAX_DEG       30
/* ---------------- End Settings ---------------- */


/* ---------------- Globals (Live Expressions) ---------------- */
static I2C_Handle_t     emdm_i2c;
static EMDM_RegConfig_t emdm;
static EMDM_Status_t    emdm_status;
static uint16_t         battery_raw;       // expect ~9900-12600 (mV) on a 3S pack
static nrf24_status_t   radio_status;
static uint32_t         cmd_rx_count;
static uint8_t          cmd_buf[32];
static uint8_t          keys;              // latest key bitmask (KEY_xxx)
static int32_t          steer_deg;         // steering offset from center, - = left, + = right
static int8_t           speed_limit = DRIVE_SPEED_LIMIT;  // top speed; edit live in the debugger, capped at EMDM_SPEED_MAX
static GPIO_Handle_t    GPIOPwm;
static PWM_Handle_t     pwm_tim2_ch1;
/* ---------------- End Globals (Live Expressions) ---------------- */


/* ---------------- Start Helpers and Bring Ups ---------------- */
static void error_blink_forever(void){
	while (1){
		GPIO_ToggleOutputPin(GPIOA, LD2_PIN);
		HAL_Delay(100);
	}
}

// Stores the status for Live Expressions. On error: try to stop the motors, then blink forever
static void emdm_check(EMDM_Status_t status){
	emdm_status = status;
	if (status != EMDM_OK){
		EMDM_Stop(&emdm, &emdm_i2c); // may also fail if the bus is dead
		error_blink_forever();
	}
}

static void led_show_moving(void){
	GPIO_WriteToOutputPin(GPIOA, LD2_PIN, (emdm.EMDM_DRIVE_SPEED != 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

// Copied from demo_pwm_steering.c: offset from center (- left, + right) -> pulse in us, clamped to +/- STEER_MAX_DEG
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


// Sets up PB8/PB9 + I2C1, checks the EMDM answers (battery read), then runs the Hiwonder init sequence
static void emdm_bringup(void){
	// (1) Pins: PB8 = SCL (D15), PB9 = SDA (D14), AF4, open-drain
	GPIO_Handle_t i2c_pin = {0};
	i2c_pin.pGPIOx = GPIOB;
	i2c_pin.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ALTFN;
	i2c_pin.GPIO_PinConfig.GPIO_PinAltFunMode = GPIO_ALT_FN_4;
	i2c_pin.GPIO_PinConfig.GPIO_PinOpType = GPIO_OP_TYPE_OD;	// I2C requirement
	i2c_pin.GPIO_PinConfig.GPIO_PinPuPdControl = EMDM_I2C_PUPD;
	i2c_pin.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPD_HIGH;		// Possibly lower if ringing/noise on scope

	GPIO_PeriClockControl(GPIOB, ENABLE);
	i2c_pin.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_8;
	GPIO_Init(&i2c_pin);
	i2c_pin.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_9;
	GPIO_Init(&i2c_pin);

	// (2) I2C1: 100 kHz, 7-bit, master write, AUTOEND
	emdm_i2c.pI2C1 = I2C1;
	emdm_i2c.I2C_PinConfig.I2C_FREQ = I2C_FREQ_8MHZ;
	emdm_i2c.I2C_PinConfig.I2C_MODE = STANDARD_100K;
	emdm_i2c.I2C_PinConfig.I2C_ANLG_FILTER_CTRL = ANLG_FILTER_EN;
	emdm_i2c.I2C_PinConfig.I2C_DIGI_FILTER_CTRL = DIGI_FILTER_DI;
	emdm_i2c.I2C_PinConfig.I2C_ERR_INTERRUPT_EN = DISABLE;
	emdm_i2c.I2C_PinConfig.I2C_CLK_STRETCH = ENABLE;
	emdm_i2c.I2C_PinConfig.I2C_ADDR_MODE = ADDR_MODE_7BIT;
	emdm_i2c.I2C_PinConfig.I2C_TRANSFER_DIR = WRITE;
	emdm_i2c.I2C_PinConfig.I2C_HEAD10R_EN = DISABLE;
	emdm_i2c.I2C_PinConfig.I2C_NBYTES = 1;
	emdm_i2c.I2C_PinConfig.I2C_AUTOEND_EN = ENABLE;

	I2C_Peri_ClockControl(ENABLE);
	I2C_Clock_Choice(HSI);				// I2C1 clock = HSI 8 MHz, so the 8 MHz timing tables apply (SYSCLK is 64 MHz)
	I2C_Peri_Control(I2C1, DISABLE);	// config bits can only be changed while PE = 0
	I2C_Init(&emdm_i2c);
	I2C_Clock_Config(&emdm_i2c);
	I2C_Peri_Control(I2C1, ENABLE);
	EMDM_Set_I2C_Slave(&emdm_i2c);

	// (3) First contact: read the battery. Proves I2C reads work before anything moves
	HAL_Delay(200);						// let the EMDM boot (Hiwonder example waits 200 ms)
	emdm_check(EMDM_Get_Battery_Raw(&emdm_i2c, &battery_raw));

	// (4) Hiwonder init sequence: motor type -> 5 ms -> polarity -> 2 s
	emdm.EMDM_MOTOR_TYPE = MOTOR_TYPE_JGB;
	emdm.EMDM_POLARITY = DEFAULT;
	emdm_check(EMDM_Set_Motor_Type(&emdm, &emdm_i2c));
	HAL_Delay(5);
	emdm_check(EMDM_Set_Polarity(&emdm, &emdm_i2c));
	HAL_Delay(2000);

	emdm_check(EMDM_Stop(&emdm, &emdm_i2c));
	led_show_moving();
}

static void nRF_radio_bringup(void){
	// Radio: same settings as demo_nrf_rx_led / the Arduino sketch
	// NOTE: SPI transfer is handled in nrf24I01_port_stm32.c which provides functions called in
	// nrf24_open_reading_pipe() so that is why no SPI code is seen here.
	nrf24_config_t cfg = NRF24_CONFIG_RF24_DEFAULTS;
	cfg.channel   = RADIO_CHANNEL;
	cfg.data_rate = NRF24_250KBPS;
	cfg.pa_level  = NRF24_PA_LOW;

	radio_status = nrf24_init(&cfg);
	if (radio_status != NRF24_OK){
		error_blink_forever();
	}
	nrf24_open_reading_pipe(1, RADIO_ADDR);
	nrf24_start_listening();
}

static void pwm_bringup(void){
	// Configure a timer pin to be used as the PWM controller to send signals to the servo motor for turning

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
}
/* ---------------- End Helpers and Bring Ups ---------------- */


/* ---------------- Start Controls ---------------- */
// E-stop = 0 now, S ramps down to 0, W ramps up, nothing held = keep speed (cruise)
static void drive_update(uint8_t k){
	int16_t speed = emdm.EMDM_DRIVE_SPEED;

	if (k & KEY_ESTOP)      speed = 0;
	else if (k & KEY_SLOW)  speed -= DECEL_STEP;
	else if (k & KEY_FWD)   speed += ACCEL_STEP;

	if (speed_limit > EMDM_SPEED_MAX) speed_limit = EMDM_SPEED_MAX;	// never exceed the driver limit
	if (speed > speed_limit)          speed = speed_limit;			// lowering the limit while driving drops to it
	if (speed < 0)                    speed = 0;					// last, so a negative limit can't cause reverse

	if (speed != emdm.EMDM_DRIVE_SPEED){	// only use the I2C bus when the speed changes
		emdm_check(EMDM_Set_Drive_Speed(&emdm, &emdm_i2c, (int8_t)speed));
	}
	led_show_moving();
}

// E-stop = center now, A/D ramp the angle, nothing held (or both) = ease back to center
static void steer_update(uint8_t k){
	int32_t dir = ((k & KEY_RIGHT) ? 1 : 0) - ((k & KEY_LEFT) ? 1 : 0);

	if (k & KEY_ESTOP){
		steer_deg = 0;
	}
	else if (dir != 0){
		steer_deg += dir * STEER_STEP_DEG;
	}
	else if (steer_deg > 0){
		steer_deg -= (steer_deg > STEER_RETURN_DEG) ? STEER_RETURN_DEG : steer_deg;
	}
	else if (steer_deg < 0){
		steer_deg += (-steer_deg > STEER_RETURN_DEG) ? STEER_RETURN_DEG : -steer_deg;
	}

	// Clamp the state too (not just the pulse) so holding a key does not wind up past full lock
	if (steer_deg > STEER_MAX_DEG)  steer_deg = STEER_MAX_DEG;
	if (steer_deg < -STEER_MAX_DEG) steer_deg = -STEER_MAX_DEG;

	pwm_set_compare(&pwm_tim2_ch1, steer_offset_to_pulse(steer_deg));
}
/* ---------------- End Controls ---------------- */


/* ---------------- Demo: full keyboard drive ---------------- */
void demo_full_PC_car_ctrl(void){
	emdm_bringup();
	pwm_bringup();
	nRF_radio_bringup();

	uint32_t last_rx   = HAL_GetTick();
	uint32_t last_ctrl = HAL_GetTick();
	keys = KEY_ESTOP;							// stay stopped until the first packet arrives

	while (1){
		if (nrf24_available(NULL) && nrf24_read(cmd_buf, sizeof cmd_buf) > 0){
			keys = cmd_buf[0];
			last_rx = HAL_GetTick();
			cmd_rx_count++;
		}

		uint32_t now = HAL_GetTick();
		if (now - last_ctrl >= CTRL_PERIOD_MS){
			last_ctrl = now;
			if (now - last_rx > KEY_TIMEOUT_MS){
				keys = KEY_ESTOP;				// FAILSAFE: link lost -> emergency stop
			}
			drive_update(keys);
			steer_update(keys);
		}
	}
}
