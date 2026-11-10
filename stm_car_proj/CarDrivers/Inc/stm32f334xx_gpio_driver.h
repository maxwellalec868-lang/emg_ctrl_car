/*
 * stm32f334xx_gpio_driver.h
 *
 *  Created on: Oct 6, 2025
 *      Author: amaxw
 */

#ifndef INC_STM32F334XX_GPIO_DRIVER_H_
#define INC_STM32F334XX_GPIO_DRIVER_H_

// Header file that contains MCU specific data (See driver notes for details)
#include "stm32f334r8.h"

/* This header will contain gpio driver specific information*/

/*Create a GPIO handle structure for a GPIO pin.
 * I think this is just used to essentially select/utilize a port/pin to use ?
 * */

typedef struct{
	uint8_t GPIO_PinNumber;			/*<Possible Values from @GPIO_PIN_NUMBERS>*/
	uint8_t GPIO_PinMode;			/*<Possible Values from @GPIO_PIN_MODES>*/
	uint8_t GPIO_PinSpeed;			/*<Possible Values from @GPIO_PIN_SPEED>*/
	uint8_t GPIO_PinPuPdControl;	/*<Possible Values from @GPIO_PIN_PUPD>*/
	uint8_t GPIO_PinOpType;			/*<Possible Values from @GPIO_OUT_TYPE>*/
	uint8_t GPIO_PinAltFunMode;		/*<Possible Values from @GPIO_ALT_FN_MODES>*/
} GPIO_PinConfig_t;



typedef struct
{
	// Pointer to hold the base address of the GPIO peripheral
	GPIO_RegDef_t* pGPIOx; // Holds base addr of the GPIO port to which the pin belongs so they can be actually set in the source file
	GPIO_PinConfig_t GPIO_PinConfig; // Holds the GPIO pin config settings from user
}GPIO_Handle_t;

/* *********** START OF MACROS FOR GPIO_PinConfig_t STRUCT *********** */

/*
 * @GPIO_PIN_NUMBERS
 * Possible GPIO pin NUMBERS
 * */
#define GPIO_PIN_NO_0				0
#define GPIO_PIN_NO_1				1
#define GPIO_PIN_NO_2				2
#define GPIO_PIN_NO_3				3
#define GPIO_PIN_NO_4				4
#define GPIO_PIN_NO_5				5
#define GPIO_PIN_NO_6				6
#define GPIO_PIN_NO_7				7
#define GPIO_PIN_NO_8				8
#define GPIO_PIN_NO_9				9
#define GPIO_PIN_NO_10				10
#define GPIO_PIN_NO_11				11
#define GPIO_PIN_NO_12				12
#define GPIO_PIN_NO_13				13
#define GPIO_PIN_NO_14				14
#define GPIO_PIN_NO_15				15


/*
 * @GPIO_PIN_MODES
 * Possible GPIO pin Modes
 * */
#define GPIO_MODE_IN				0
#define GPIO_MODE_OUT				1
#define GPIO_MODE_ALTFN				2
#define GPIO_MODE_ANALOG			3
#define GPIO_MODE_IT_FT				4
#define GPIO_MODE_IT_RT				5
#define GPIO_MODE_IT_RFT			6


/*
 * @GPIO_PIN_SPEED
 * Possible Output Speeds
 * */
#define GPIO_SPD_LOW				0			// Technically is 0 or 2 cause defiine as "x0" where x = dont care
#define GPIO_SPD_MED				1
#define GPIO_SPD_HIGH				3


/*
 * @GPIO_PIN_PUPD
 * Pullup and pull down configs
 * */
#define GPIO_NO_PUPD				0
#define GPIO_PU						1
#define GPIO_PD						2


/*
 * @GPIO_OUT_TYPE
 * Possible GPIO Output Types
 * */
#define GPIO_OP_TYPE_PP				0
#define GPIO_OP_TYPE_OD				1


/*
 * @GPIO_ALT_FN_MODES
 * Possible GPIO Output Types
 * */
#define GPIO_ALT_FN_0				0
#define GPIO_ALT_FN_1				1
#define GPIO_ALT_FN_2				2
#define GPIO_ALT_FN_3				3
#define GPIO_ALT_FN_4				4
#define GPIO_ALT_FN_5				5
#define GPIO_ALT_FN_6				6
#define GPIO_ALT_FN_7				7
#define GPIO_ALT_FN_8				8
#define GPIO_ALT_FN_9				9
#define GPIO_ALT_FN_10				10
#define GPIO_ALT_FN_11				11
#define GPIO_ALT_FN_12				12
#define GPIO_ALT_FN_13				13
#define GPIO_ALT_FN_14				14
#define GPIO_ALT_FN_15				15



/* *********** END OF MACROS FOR GPIO_PinConfig_t STRUCT *********** */

/**************************************************************************************
 * 							APIs Supported By This Driver
 * 				For More Information About the APIs Check the Function Definitions
 **************************************************************************************/

/*
 * Peri Clock Setup
 * */
void GPIO_PeriClockControl(GPIO_RegDef_t* pGPIOx, uint8_t EnorDi);

/*
 * Init De-init GPIO
 * */
// These functions will configure the pins in the handle struct;
void GPIO_Init(GPIO_Handle_t* pGPIOHandle);
void GPIO_DeInit(GPIO_RegDef_t* pGPIOx);


/*
 * Data Read and Write
 * */
uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t* pGPIOx, uint8_t PinNumber);
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t* pGPIOx);
void GPIO_WriteToOutputPin(GPIO_RegDef_t* pGPIOx, uint8_t PinNumber, uint8_t value);
void GPIO_WriteToOutputPort(GPIO_RegDef_t* pGPIOx, uint16_t value);
void GPIO_ToggleOutputPin(GPIO_RegDef_t* pGPIOx, uint8_t PinNumber);

/*
 * IRQ config and handling
 * */
void GPIO_IRQ_Interrupt_Config(uint8_t IRQNumber, uint8_t EnorDi);
void GPIO_IRQ_priorityConfig(uint8_t IRQNumber, uint32_t IRQPriority);
void GPIO_IRQHandling(uint8_t PinNumber);



#endif /* INC_STM32F334XX_GPIO_DRIVER_H_ */
