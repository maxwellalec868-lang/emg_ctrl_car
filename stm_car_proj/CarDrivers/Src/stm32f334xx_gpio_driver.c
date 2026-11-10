/*
 * stm32f334xx_gpio_driver.c
 *
 *  Created on: Oct 6, 2025
 *      Author: amaxw
 */


#include "stm32f334xx_gpio_driver.h"



/**************************************************************************************
 * @fn					- GPIO_PerClockControl
 *
 * @brief				- Function to enable or disable a GPIOs port peripheral clock
 *
 * @param[in]			- Base address of the GPIO peripheral
 * @param[in]			- ENABLE OR DISABLE Macros
 *
 * @return				- void
 *
 * @Note				- Special Note Section; Nothing to include for this function (do "None" usually)
 *
 **************************************************************************************/

void GPIO_PeriClockControl(GPIO_RegDef_t* pGPIOx, uint8_t EnorDi){
	if (EnorDi == ENABLE){
		if (pGPIOx == GPIOA){
			GPIOA_PCLK_EN();
		}
		else if(pGPIOx == GPIOB){
			GPIOB_PCLK_EN();
		}
		else if(pGPIOx == GPIOC){
			GPIOC_PCLK_EN();
		}
		else if(pGPIOx == GPIOD){
			GPIOD_PCLK_EN();
		}
		else if(pGPIOx == GPIOF){
			GPIOF_PCLK_EN();
		}
	}
	else{
		if (pGPIOx == GPIOA){
			GPIOA_PCLK_DI();
		}
		else if(pGPIOx == GPIOB){
			GPIOB_PCLK_DI();
		}
		else if(pGPIOx == GPIOC){
			GPIOC_PCLK_DI();
		}
		else if(pGPIOx == GPIOD){
			GPIOD_PCLK_DI();
		}
		else if(pGPIOx == GPIOF){
			GPIOF_PCLK_DI();
		}
	}

}

/*
 * Init De-init GPIO
 * These functions will configure the pins in the handle struct
 * */


/**************************************************************************************
 * @fn					- GPIO_Init
 *
 * @brief				- Initializes/configures the registers of a GPIO port
 *
 * @param[in]			- Struct to easily config/manipulate registers of a GPIO port (GPIO_RegDef_t* pGPIOx;) and store desired configs for each pin (GPIO_PinConfig_t GPIO_PinConfig)
 *
 *
 * @return				- void
 *
 * @Note				- None
 *
 **************************************************************************************/
void GPIO_Init(GPIO_Handle_t* pGPIOHandle){
	uint32_t tmp = 0;

	// 1. Configure mode of GPIO pin
	if (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode <= GPIO_MODE_ANALOG){
		// Non interrupt modes (output, input, AF)
		tmp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber)); // tmp will contain a 32-bit int that will be all 0's except for location 2*pin_number which will hold PinMode
																											  // value. Say that val is 3 and pin number = 2 then bits 0 - 5 will be: 000011...0 VERIFY WITH GPT
		pGPIOHandle->pGPIOx->MODER &= ~(0x3 << 2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber); // clearing bits
		pGPIOHandle->pGPIOx->MODER |= tmp; // -> is dereference operator for pointer structs. Using it allows for modification of values in a struct
		// TODO: **** Change how this is done since as of now the whole register is being cleared; Implement bit masking techniques ****
		}else
		{
			// TODO: This is interrupt mode
			if (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_FT){
				//1.1 Configure the FTSR. Need to set line of which ever pin I am using where lines correspond to pin numbers so just use pin_number to choose which line to set.
				EXTI->EXTI_FTSR1 |= (1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
				EXTI->EXTI_RTSR1 &= ~(1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber); // Clear corresponding bit in RTSR reg to ensure it is off
			}
			else if (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_RT){
				//1.2 Configure the RTSR
				EXTI->EXTI_RTSR1 |= (1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
				EXTI->EXTI_FTSR1 &= ~(1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber); // Clear corresponding bit in RTSR reg to ensure it is off
			}
			else if (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_RFT){
				// 1.3 Configure both FTSR and RTSR
				EXTI->EXTI_FTSR1 |= (1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
				EXTI->EXTI_RTSR1 |= (1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
			}
			// 2. Configure GPIO port selection in SYSCFG_EXTICR
			// TODO: Rename tmps to better variables
			uint8_t tmp1 = pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber / 4;
			uint8_t tmp2 = pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber % 4;
			uint8_t portcode = GPIO_BASEADDR_TO_CODE(pGPIOHandle->pGPIOx);
			SYSCFG_PCLK_EN();
			SYSCFG->EXTICR[tmp1] = portcode << (tmp2 * 4);



			// 3. Enable the EXTI interrupt delivery using IMR
			EXTI->EXTI_IMR1 |= (1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);

			}
	// 2. Then speed
	tmp = 0;
	tmp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinSpeed << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
	pGPIOHandle->pGPIOx->OSPEEDR &= ~(0x3 << 2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
	pGPIOHandle->pGPIOx->OSPEEDR |= tmp;

	// 3. Then pupd
	tmp = 0;
	tmp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinPuPdControl << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
	pGPIOHandle->pGPIOx->PUPDR &= ~(0x3 << 2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
	pGPIOHandle->pGPIOx->PUPDR |= tmp;

	// 4. Then otype
	tmp = 0;
	tmp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinOpType << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
	pGPIOHandle->pGPIOx->OTYPER &= ~(0x1 << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
	pGPIOHandle->pGPIOx->OTYPER |= tmp;

	// 5. AF
	if (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_ALTFN){
		uint8_t tmp1, tmp2;
		tmp1 = pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber / 8;		// performs int division; If Greater than 8 results in 1, use AFRH, If less than 8 results in 0; use AFRL
		tmp2 = pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber % 8;		// Perform mod so resuslt tells you which bit position to start at based on pin number
		if (tmp1 == 0){
			// Handle AFRL
			pGPIOHandle->pGPIOx->AFRL &= ~(0xF << (4 * tmp2));
			pGPIOHandle->pGPIOx->AFRL |= (pGPIOHandle->GPIO_PinConfig.GPIO_PinAltFunMode << (4 * tmp2));
		}else
		{
			// Handle AFRH
			pGPIOHandle->pGPIOx->AFRH &= ~(0xF << (4 * tmp2));
			pGPIOHandle->pGPIOx->AFRH |= (pGPIOHandle->GPIO_PinConfig.GPIO_PinAltFunMode << (4 * tmp2));
		}
	}

}

/**************************************************************************************
 * @fn					- GPIO_DeInit
 *
 * @brief				- Call bus reset register to reset all registers (IMPROVE!!!!)
 *
 * @param[in]			- Struct pointer to specific port base address
 *
 *
 * @return				- void
 *
 * @Note				- None
 *
 **************************************************************************************/
void GPIO_DeInit(GPIO_RegDef_t* pGPIOx){
	if (pGPIOx == GPIOA){
		GPIOA_REG_RESET();
	}
	else if(pGPIOx == GPIOB){
		GPIOB_REG_RESET();
	}
	else if(pGPIOx == GPIOC){
		GPIOC_REG_RESET();
	}
	else if(pGPIOx == GPIOD){
		GPIOD_REG_RESET();
	}
	else if(pGPIOx == GPIOF){
		GPIOF_REG_RESET();
	}
}





/*
 * Data Read and Write
 * */


/**************************************************************************************
 * @fn					- GPIO_ReadFromInputPin
 *
 * @brief				- Reads the value at a specific input pin
 *
 * @param[in]			- pGPIOx will be sa specific GPIO port macro (GPIOA, GPIOB, GPIOC, GPIOD, or GPIOF)
 * @param[in]			- PinNumber is specific pin to read from
 *
 * @return				- Value at that pin
 *
 * @Note				- None
 *
 **************************************************************************************/
uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t* pGPIOx, uint8_t PinNumber){
	uint8_t value;
	//  Moves desired bit to LSB of the 32-bit int by right shifting PinNumber amount of times.
	//	Now pin that I want to read is at bit 0, so can now mask off everything besides bit 0 using 0x00000001 so result is:
	//	If 1 then 1 causes 1 AND 1 = 1; If 0 then 0 cause 1 AND 0 = 0
	// Finally, type cast to uint8_t
	value = (uint8_t) (pGPIOx->IDR >> PinNumber) & (0x00000001);
	return value;

}


/**************************************************************************************
 * @fn					- GPIO_ReadFromInputPort
 *
 * @brief				- Reads all values of an input port
 *
 * @param[in]			- pGPIOx will be sa specific GPIO port macro (GPIOA, GPIOB, GPIOC, GPIOD, or GPIOF)
 *
 * @return				- All pin values in an uint16_t variables
 *
 * @Note				- None
 *
 **************************************************************************************/
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t* pGPIOx){
	uint16_t value;
	value = (uint16_t)pGPIOx->IDR;
	return value;
}


/**************************************************************************************
 * @fn					- GPIO_WriteToOutputPin
 *
 * @brief				- PLACEHOLDER
 *
 * @param[in]			- PLACEHOLDER
 * @param[in]			- PLACEHOLDER
 *
 * @return				- PLACEHOLDER
 *
 * @Note				- PLACEHOLDER
 *
 **************************************************************************************/
void GPIO_WriteToOutputPin(GPIO_RegDef_t* pGPIOx, uint8_t PinNumber, uint8_t value){
	if (value == GPIO_PIN_SET){
		// write 1 to ODR at the bit field corresponding to the pin number
		pGPIOx->ODR |= (1 << PinNumber);
	}else
	{
		// write 0
		pGPIOx->ODR &= ~(1 << PinNumber);
	}
}


/**************************************************************************************
 * @fn					- GPIO_WriteToOutputPort
 *
 * @brief				- PLACEHOLDER
 *
 * @param[in]			- PLACEHOLDER
 * @param[in]			- PLACEHOLDER
 *
 * @return				- PLACEHOLDER
 *
 * @Note				- PLACEHOLDER
 *
 **************************************************************************************/
void GPIO_WriteToOutputPort(GPIO_RegDef_t* pGPIOx, uint16_t value){
	pGPIOx->ODR = value;

}


/**************************************************************************************
 * @fn					- GPIO_ToggleOutputPin
 *
 * @brief				- PLACEHOLDER
 *
 * @param[in]			- PLACEHOLDER
 * @param[in]			- PLACEHOLDER
 *
 * @return				- PLACEHOLDER
 *
 * @Note				- PLACEHOLDER
 *
 **************************************************************************************/
void GPIO_ToggleOutputPin(GPIO_RegDef_t* pGPIOx, uint8_t PinNumber){
	pGPIOx->ODR ^= (1 << PinNumber);
}



/*
 * IRQ config and handling
 * */

/**************************************************************************************
 * @fn					- GPIO_IRQConfig
 *
 * @brief				- PLACEHOLDER
 *
 * @param[in]			- PLACEHOLDER
 * @param[in]			- PLACEHOLDER
 *
 * @return				- PLACEHOLDER
 *
 * @Note				- PLACEHOLDER
 *
 **************************************************************************************/
void GPIO_IRQ_Interrupt_Config(uint8_t IRQNumber, uint8_t EnorDi){
	if (EnorDi == ENABLE){
		if (IRQNumber <= 31){
			// Program ISER0 Register
			*NVIC_ISER0 |= (1 << IRQNumber);
		}
		else if(IRQNumber > 31 && IRQNumber < 64){
			// Program ISER1 Register
			*NVIC_ISER1 |= (1 << (IRQNumber % 32));
		}
		else if (IRQNumber >= 64 && IRQNumber < 96){
			// Program ISER2 Register
			// Since I only have 81 interrupts, ISER2 is sufficient
			*NVIC_ISER2 |= (1 << (IRQNumber % 64));
		}

	}
	else{

		if (IRQNumber <= 31){
			// Program ICER0 Register
			*NVIC_ICER0 |= (1 << IRQNumber);
			}
		else if (IRQNumber > 31 && IRQNumber < 64){
			// Program ICER1 Register
			*NVIC_ICER1 |= (1 << (IRQNumber % 32));
			}
		else if (IRQNumber >= 64 && IRQNumber < 96){
			// Program ICER2 Register
			*NVIC_ICER2 |= (1 << (IRQNumber % 64));

			}
		}
}

// NEED TO UNDERSTAND THIS FUNCTION BETTER
//*** THIS SECTION MAY NOT BE WORKING ***
void GPIO_IRQ_priorityConfig(uint8_t IRQNumber,uint32_t IRQPriority){
	// 1. Find out IPR register
	uint8_t iprx = IRQNumber / 4;
	uint8_t iprx_section = IRQNumber % 4;
	uint8_t shift_amount = (8 * iprx_section) + (8 - NUM_PR_BITS_IMPLEMENTED);
	*(NVIC_PR_BASE_ADDR + iprx) |= (IRQPriority << (shift_amount));
}


/**************************************************************************************
 * @fn					- GPIO_IRQHandling
 *
 * @brief				- PLACEHOLDER
 *
 * @param[in]			- PLACEHOLDER
 * @param[in]			- PLACEHOLDER
 *
 * @return				- PLACEHOLDER
 *
 * @Note				- PLACEHOLDER
 *
 **************************************************************************************/
void GPIO_IRQHandling(uint8_t PinNumber){
	// (1): Implement ISR (Interrupt Service Routine) Function
	// (2): Store the addr of ISR at the vector addr location corresponding to the IRQ number for which you have written the ISR
	if (EXTI->EXTI_PR1 & (1 << PinNumber)){
		EXTI->EXTI_PR1 |= (1 << PinNumber);
	}

}







