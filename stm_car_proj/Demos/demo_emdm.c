/*
 * demo_emdm.c
 *
 * EMDM (encoder motor driver) demos. Two entry points share emdm_bringup():
 *
 *   demo_emdm_bench_run()  - No radio. Reads the battery, spins M1 alone, then M3 alone,
 *                            then loops: drive 15 -> speed up +10 -> stop.
 *                            Use it to confirm I2C works and to set EMDM_DRIVE_DIR_Mx.
 *   demo_emdm_radio_run()  - Full system. Radio packets from the Arduino drive the car:
 *                              byte 0 = command: CMD_FORWARD (speed up), CMD_SLOW (slow down, stops at 0), CMD_STOP
 *                              byte 1 = step size (0 = DEMO_STEP)
 *                            Unknown commands (e.g. 0x45 from nrf_tx_0x45) are ignored.
 *                            NO FAILSAFE YET: the car keeps its last speed if the radio drops (docs/future_work.md #2).
 *
 * Wiring: EMDM SCL -> PB8 (D15), SDA -> PB9 (D14), GND common with the Nucleo.
 *
 * LD2: on while moving, off while stopped, fast blink = error (EMDM I2C or radio init).
 * Watch in Live Expressions: emdm_status, battery_raw, bench_phase, emdm.EMDM_DRIVE_SPEED,
 *                            cmd_rx_count, last_cmd, radio_status.
 *
 * Custom-driver world: must NOT include main.h (see demos.h). HAL_Delay is declared below instead.
 * TEST WITH THE WHEELS OFF THE GROUND FIRST.
 */

#include "stm32f334xx_gpio_driver.h"
#include "emdm_device_driver.h"
#include "nrf24l01.h"
#include "demos.h"

/* HAL delay (SysTick is already running from HAL_Init() in main.c) */
extern void HAL_Delay(uint32_t Delay);

/* ---------------- Settings ---------------- */
#define LD2_PIN             GPIO_PIN_NO_5   // LD2 = PA5 (already an output from MX_GPIO_Init())

// I2C pull-up: GPIO_PU for now. If the EMDM pulls SDA/SCL up to 5 V, change to GPIO_NO_PUPD (see docs/future_work.md #1)
#define EMDM_I2C_PUPD       GPIO_PU

// Radio: must match the Arduino sketch (same as demo_nrf_rx_led)
#define RADIO_ADDR          ((const uint8_t *)"JOY01")
#define RADIO_CHANNEL       76

// Radio commands (byte 0 of the payload). Move to a header shared with the Arduino later
#define CMD_FORWARD         0x01    // speed up by step
#define CMD_SLOW            0x02    // slow down by step, never below 0
#define CMD_STOP            0x03    // stop immediately
#define DEMO_STEP           5       // default step when byte 1 is 0

#define BENCH_TEST_SPEED    15      // speed used by the bench demo

/* ---------------- Globals (Live Expressions) ---------------- */
I2C_Handle_t     emdm_i2c;
EMDM_RegConfig_t emdm;
EMDM_Status_t    emdm_status;
uint16_t         battery_raw;       // expect ~9900-12600 (mV) on a 3S pack
uint8_t          bench_phase;       // 0 bring-up, 1 M1 alone, 2 M3 alone, 3 drive, 4 faster, 5 stopped
nrf24_status_t   radio_status;
uint32_t         cmd_rx_count;
uint8_t          last_cmd;
uint8_t          cmd_buf[32];


/* ---------------- Helpers ---------------- */
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

// Bench helper: spin one channel (0 = M1 ... 3 = M4) alone for 2 s, then stop for 1 s
static void spin_one_channel(uint8_t channel){
	for (uint8_t i = 0; i < 4; i++){
		emdm.EMDM_SPEED[i] = (i == channel) ? BENCH_TEST_SPEED : 0;
	}
	GPIO_WriteToOutputPin(GPIOA, LD2_PIN, GPIO_PIN_SET);
	emdm_check(EMDM_Set_Fixed_Speed_Reg(&emdm, &emdm_i2c));
	HAL_Delay(2000);

	emdm_check(EMDM_Stop(&emdm, &emdm_i2c));
	led_show_moving();
	HAL_Delay(1000);
}


/* ---------------- Demo: bench (no radio) ---------------- */
void demo_emdm_bench_run(void){
	bench_phase = 0;
	emdm_bringup();

	// Raw channel test: watch which wheel turns and which way. Both should turn "forward" for the car
	// to go forward; if not, flip that channel's EMDM_DRIVE_DIR_Mx sign in emdm_device_driver.h
	bench_phase = 1;
	spin_one_channel(0);	// M1
	bench_phase = 2;
	spin_one_channel(2);	// M3

	// Drive-level test
	while (1){
		bench_phase = 3;
		emdm_check(EMDM_Set_Drive_Speed(&emdm, &emdm_i2c, BENCH_TEST_SPEED));
		led_show_moving();
		HAL_Delay(2000);

		bench_phase = 4;
		emdm_check(EMDM_Change_Drive_Speed(&emdm, &emdm_i2c, +10));
		HAL_Delay(2000);

		bench_phase = 5;
		emdm_check(EMDM_Stop(&emdm, &emdm_i2c));
		led_show_moving();
		HAL_Delay(2000);
	}
}


/* ---------------- Demo: radio commands (full system) ---------------- */
void demo_emdm_radio_run(void){
	emdm_bringup();

	// Radio: same settings as demo_nrf_rx_led / the Arduino sketch
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

	while (1){
		if (!nrf24_available(NULL)){
			continue;
		}
		if (nrf24_read(cmd_buf, sizeof cmd_buf) == 0){
			continue;	// corrupt packet
		}
		cmd_rx_count++;
		last_cmd = cmd_buf[0];

		uint8_t step = cmd_buf[1];
		if (step == 0)              step = DEMO_STEP; // If no cmd sent in the buffer, then just step at 5
		if (step > EMDM_SPEED_MAX)  step = EMDM_SPEED_MAX;

		switch (last_cmd){
		case CMD_FORWARD:
			emdm_check(EMDM_Change_Drive_Speed(&emdm, &emdm_i2c, (int8_t)step));
			break;

		case CMD_SLOW: {
			// Slow down but stop at 0 (no reverse in this demo)
			int8_t decrease = (step > emdm.EMDM_DRIVE_SPEED) ? emdm.EMDM_DRIVE_SPEED : (int8_t)step;
			emdm_check(EMDM_Change_Drive_Speed(&emdm, &emdm_i2c, -decrease));
			break;
		}

		case CMD_STOP:
			emdm_check(EMDM_Stop(&emdm, &emdm_i2c));
			break;

		default:
			break;	// unknown command (e.g. 0x45 from the old test sketch): ignore
		}

		led_show_moving();
	}
}
