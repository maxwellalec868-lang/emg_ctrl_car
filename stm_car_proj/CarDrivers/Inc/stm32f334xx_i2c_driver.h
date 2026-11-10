/*
 * stm32f334xx_i2c_driver.h
 *
 *  Created on: Feb 19, 2026
 *      Author: amaxw
 */

#ifndef INC_STM32F334XX_I2C_DRIVER_H_
#define INC_STM32F334XX_I2C_DRIVER_H_


// Header file that contains MCU specific data (See driver notes for details)
#include "stm32f334r8.h"

/* This header will contain I2C driver specific information*/

/*Create a i2c handle structure for a i2c pin.
 * This struct will be used to store choices made in a main file using the driver. Sets register values accordingly
 * */

typedef struct{
	uint8_t  I2C_FREQ;				/*<Possible Values from @I2C_FREQ>*/
	uint8_t  I2C_MODE;				/*<Possible Values from @I2C_MODE>*/
	uint8_t  I2C_ANLG_FILTER_CTRL;	/*<Possible Values from @I2C_ANALOG_FILTER>*/
	uint8_t  I2C_DIGI_FILTER_CTRL;	/*<Possible Values from @I2C_DIGITAL_FILTER>*/
	uint8_t  I2C_ERR_INTERRUPT_EN;	/*<Possible Values(MACROS): ENABLE OR DISABLE>*/
	uint8_t  I2C_CLK_STRETCH;		/*<Possible Values(MACROS): ENABLE OR DISABLE>*/
	uint8_t  I2C_ADDR_MODE;			/*<Possible Values from: @I2C_ADDR_MODE >*/
	uint16_t I2C_SLAVE_ADDR;		/*<7-bit or 10-bit address>*/
	uint8_t  I2C_TRANSFER_DIR;		/*<Possible Values from @I2C_TRANSFER_DRIECTIONS>*/
	uint8_t  I2C_HEAD10R_EN;		/*<Possible Values(MACROS): ENABLE OR DISABLE>*/
	uint8_t  I2C_NBYTES;			/*<Possible Values are: 0-255 bytes since 8-bit number >*/
	uint8_t  I2C_AUTOEND_EN;		/*<Possible Values(MACROS): ENABLE OR DISABLE>*/
} I2C_PinConfig_t;



typedef struct
{
	// Pointer to hold the base address of the I2C peripheral
	I2C_RegDef_t* pI2C1; // Holds base addr of the I2C pin so they can be set in the source file
	I2C_PinConfig_t I2C_PinConfig; // Holds the I2C pin config settings from user
}I2C_Handle_t;


/* Enum will be used to store certain errors that can be found.
 * I believe this will be very useful when using interrupts and during actual implementation on car
 * */
typedef enum
{
    I2C_OK = 0,
    I2C_ERR_INVALID_ADDR_MODE,
	I2C_ERR_INVALID_RD_WR,
	I2C_ERR_INVALID_ADDR,
	I2C_ERR_NACK,
	I2C_ERR_INVALID_PARAM
} I2C_Status_t;



/* *********** START OF MACROS FOR I2C_PinConfig_t STRUCT *********** */

// @I2C_FREQ
#define I2C_FREQ_8MHZ				0
#define I2C_FREQ_16MHZ				1 // NOT USED FOR NOW
#define I2C_FREQ_48MHZ				2 // NOT USED FOR NOW

// @I2C_MODE
#define STANDARD_10K				0
#define STANDARD_100K				1
#define FAST						2
#define FAST_PLUS					3

// @I2C_ANALOG_FILTER
#define ANLG_FILTER_EN				0
#define ANLG_FILTER_DI				1

// @I2C_ADDR_MODE
#define ADDR_MODE_7BIT				0
#define ADDR_MODE_10BIT				1

// @I2C_TRANSFER_DRIECTIONS
#define WRITE						0
#define READ						1


/*
 * @I2C_DIGITAL_FILTER
 * Note: Digital filter disabled if 0, all other values will be active but spike with a length ...EN_x * t_i2cclk (See RM for more info)
 * */
#define DIGI_FILTER_DI				0
#define DIGI_FILTER_EN_1			1
#define DIGI_FILTER_EN_2			2
#define DIGI_FILTER_EN_3			3
#define DIGI_FILTER_EN_4			4
#define DIGI_FILTER_EN_5			5
#define DIGI_FILTER_EN_6			6
#define DIGI_FILTER_EN_7			7
#define DIGI_FILTER_EN_8			8
#define DIGI_FILTER_EN_9			9
#define DIGI_FILTER_EN_10			10
#define DIGI_FILTER_EN_11			11
#define DIGI_FILTER_EN_12			12
#define DIGI_FILTER_EN_13			13
#define DIGI_FILTER_EN_14			14
#define DIGI_FILTER_EN_15			15



/* *********** END OF MACROS FOR I2C_PinConfig_t STRUCT *********** */


/* *********** START OF CLOCK CONFIG MACROS *********** */
//All values found from timing settings tables 124 -126 on RM pg. 911-912
// Possible prescalar values
/*Values for f_i2cclk = 8 MHz */
#define PRESC_STAN_8				1
#define PRESC_FAST_8				0
#define PRESC_FAST_PLUS_8			0
/*Values for f_i2cclk = 16 MHz */
#define PRESC_STAN_16				3
#define PRESC_FAST_16				1
#define PRESC_FAST_PLUS_16			0
/*Values for f_i2cclk = 48 MHz */
#define PRESC_STAN_48				0xB
#define PRESC_FAST_48				5
#define PRESC_FAST_PLUS_48			5


// Possible SCLL values
/*Values for f_i2cclk = 8 MHz */
#define SCLL_STAN10_8				0xC7
#define SCLL_STAN100_8				0x13
#define SCLL_FAST_8					0x9
#define SCLL_FAST_PLUS_8			0x6
/*Values for f_i2cclk = 16 MHz */
#define SCLL_STAN10_16				0xC7
#define SCLL_STAN100_16				0x13
#define SCLL_FAST_16				0x9
#define SCLL_FAST_PLUS_16			0x4
/*Values for f_i2cclk = 48 MHz */
#define SCLL_STAN10_48				0xC7
#define SCLL_STAN100_48				0x13
#define SCLL_FAST_48				0x9
#define SCLL_FAST_PLUS_48			0x3


// Possible SCLH values
/*Values for f_i2cclk = 8 MHz */
#define SCLH_STAN10_8				0xC3
#define SCLH_STAN100_8				0xF
#define SCLH_FAST_8					0x3
#define SCLH_FAST_PLUS_8			0x3
/*Values for f_i2cclk = 16 MHz */
#define SCLH_STAN10_16				0xC3
#define SCLH_STAN100_16				0xF
#define SCLH_FAST_16				0x3
#define SCLH_FAST_PLUS_16			0x2
/*Values for f_i2cclk = 48 MHz */
#define SCLH_STAN10_48				0xC3
#define SCLH_STAN100_48				0xF
#define SCLH_FAST_48				0x3
#define SCLH_FAST_PLUS_48			0x1


// Possible SDADEL values
/*Values for f_i2cclk = 8 MHz */
#define SDADEL_STAN_8				0x2
#define SDADEL_FAST_8				0x1
#define SDADEL_FAST_PLUS_8			0x0
/*Values for f_i2cclk = 16 MHz */
#define SDADEL_STAN_16				0x2
#define SDADEL_FAST_16				0x2
#define SDADEL_FAST_PLUS_16			0x0
/*Values for f_i2cclk = 48 MHz */
#define SDADEL_STAN_48				0x2
#define SDADEL_FAST_48				0x3
#define SDADEL_FAST_PLUS_48			0x0


// Possible SCLDEL values
/*Values for f_i2cclk = 8 MHz */
#define SCLDEL_STAN_8				0x4
#define SCLDEL_FAST_8				0x3
#define SCLDEL_FAST_PLUS_8			0x1
/*Values for f_i2cclk = 16 MHz */
#define SCLDEL_STAN_16				0x4
#define SCLDEL_FAST_16				0x3
#define SCLDEL_FAST_PLUS_16			0x2
/*Values for f_i2cclk = 48 MHz */
#define SCLDEL_STAN_48				0x4
#define SCLDEL_FAST_48				0x3
#define SCLDEL_FAST_PLUS_48			0x1


/* *********** END OF CLOCK CONFIG MACROS*********** */



/* *********** START OF MACROS FOR Clock Functions *********** */
// USE THESE IN MAIN TO SET EITHER HSI OR SYSCLK
/*Clock_Choice Macros*/
#define HSI							0
#define SYSCLK						1

#define SET_HSI()					(RCC->CFGR3 &= ~(1 << 4))
#define SET_SYSCLK()				(RCC->CFGR3 |= (1 << 4))

/* *********** END OF MACROS FOR Clock Functions *********** */



/**************************************************************************************
 * 							APIs Supported By This Driver
 * 				For More Information About the APIs Check the Function Definitions
 **************************************************************************************/
void I2C_Peri_ClockControl(uint8_t EnorDi);
void I2C_Clock_Choice(uint8_t HsiOrSysclk);
void I2C_Clock_Config(I2C_Handle_t* pI2CHandle); // Set params still
void I2C_Peri_Control(I2C_RegDef_t* pI2Cx, uint8_t EnorDi);
void I2C_Init(I2C_Handle_t* pI2CHandle);
I2C_Status_t I2C_Config_Master(I2C_Handle_t* pI2CHandle);
void I2C_Start(I2C_RegDef_t* pI2Cx);
void I2C_Clear_Flags(I2C_RegDef_t* pI2Cx);
I2C_Status_t I2C_Master_TX(I2C_Handle_t* pI2CHandle, uint8_t* pData_buf, uint8_t repeated_start);
I2C_Status_t I2C_Master_RX(I2C_Handle_t* pI2CHandle, uint8_t* pData_buf, uint8_t repeated_start);
I2C_Status_t I2C_Master_Reg_Read(I2C_Handle_t* pI2CHandle, uint8_t start_reg_addr, uint8_t* pRx_data_buf, uint8_t num_bytes);


#endif /* INC_STM32F334XX_I2C_DRIVER_H_ */
