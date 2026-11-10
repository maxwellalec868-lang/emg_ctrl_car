/*
 * stm32f334xx_i2c_driver.c
 *
 *  Created on: Feb 19, 2026
 *      Author: amaxw
 */

#include "stm32f334xx_i2c_driver.h"

/**************************************************************************************
 * @fn					- I2C_Peri_ClockControl
 *
 * @brief				- Function to enable or disable I2Cs peripheral clock
 *
 * @param[in]			- EnorDi: ENABLE OR DISABLE Macros
 *
 * @return				- void
 *
 * @Note				- None
 *
 **************************************************************************************/
void I2C_Peri_ClockControl(uint8_t EnorDi){
	if (EnorDi == ENABLE){
		I2C1_PCLK_EN();
	}
	else{
		I2C1_PCLK_DI();
	}
}



/**************************************************************************************
 * @fn					- I2C_Clock_Choice
 *
 * @brief				- Function to choose which clock the I2C clock should use. Can be HSI or SYSCLK
 *
 * @param[in]			- HsiOrSysclk: Call either HSI or SYSCLK macros to set HSI or SYSCLK as the I2C clock
 *
 * @return				- void
 *
 * @Note				- Sysclk could be any configurable clock (HSI, HSE, PLL, etc.). VERIFY AND IMPROVE THIS STATEMENT ONCE I NEED TO USE SYSCLK
 * 						- Possibly add else statement to handle input not HSI or SYSCLK (0 or 1). NOT SURE HOW THIS HANDLED IN DRIVER DEV; FIGURE OUT
 *
 **************************************************************************************/
void I2C_Clock_Choice(uint8_t HsiOrSysclk){
	if (HsiOrSysclk ==  HSI){
		SET_HSI();
	}
	else if (HsiOrSysclk == SYSCLK){
		SET_SYSCLK();
	}
}


/**************************************************************************************
 * @fn					- I2C_Clock_Config
 *
 * @brief				- Function to set all required registers to ensure that SCL runs at the desired speed
 *
 * @param[in]			- i2c_freq: Frequency that the I2C periperhal clock is set as
 * @param[in]			- mode: Desired mode of I2C SCL clock line (standard (10 kHz), standard (100 kHz), fast (400 kHz), and fast plus (500 kHz)
 *
 * @return				- void
 *
 * @Note				- As of now,  the only i2c_freq allowed will be 8 MhZ. I am developing the driver to only function when that clock is being used.
 * 						- Doing this because (1) I dont need anything faster for now (I think) and (2) I am not sure how to get clock higher than 8 MHz (need PLL i believe)
 *
 **************************************************************************************/
// UNDERSTAND WHY THIS IS PASSED AS POINTER. DONE IN GPIO BUT CANT REMEMBER WHY EXACTLY
void I2C_Clock_Config(I2C_Handle_t* pI2CHandle){
	if (pI2CHandle->I2C_PinConfig.I2C_FREQ == I2C_FREQ_8MHZ){
		if (pI2CHandle->I2C_PinConfig.I2C_MODE == STANDARD_10K){
			// Based on mode and f_i2cclk set:
			// 1. Set PSC
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 28);
			pI2CHandle->pI2C1->TIMINGR |= (PRESC_STAN_8 << 28);

			// 2. Set SCLL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFFU << 0);
			pI2CHandle->pI2C1->TIMINGR |= (SCLL_STAN10_8  << 0);

			// 3. Set SCLH
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFFU << 8);
			pI2CHandle->pI2C1->TIMINGR |= (SCLH_STAN10_8  << 8);

			// 4. Set SDADEL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 16);
			pI2CHandle->pI2C1->TIMINGR |= (SDADEL_STAN_8  << 16);

			// 5. Set SCLDEL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 20);
			pI2CHandle->pI2C1->TIMINGR |= (SCLDEL_STAN_8  << 20);
		}

		else if (pI2CHandle->I2C_PinConfig.I2C_MODE == STANDARD_100K){
			// 1. Set PSC
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 28);
			pI2CHandle->pI2C1->TIMINGR |= (PRESC_STAN_8 << 28);

			// 2. Set SCLL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFFU << 0);
			pI2CHandle->pI2C1->TIMINGR |= (SCLL_STAN100_8  << 0);

			// 3. Set SCLH
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFFU << 8);
			pI2CHandle->pI2C1->TIMINGR |= (SCLH_STAN100_8  << 8);

			// 4. Set SDADEL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 16);
			pI2CHandle->pI2C1->TIMINGR |= (SDADEL_STAN_8  << 16);

			// 5. Set SCLDEL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 20);
			pI2CHandle->pI2C1->TIMINGR |= (SCLDEL_STAN_8  << 20);
		}

		else if (pI2CHandle->I2C_PinConfig.I2C_MODE == FAST){
			// 1. Set PSC
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 28);
			pI2CHandle->pI2C1->TIMINGR |= (PRESC_FAST_8 << 28);

			// 2. Set SCLL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFFU << 0);
			pI2CHandle->pI2C1->TIMINGR |= (SCLL_FAST_8  << 0);

			// 3. Set SCLH
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFFU << 8);
			pI2CHandle->pI2C1->TIMINGR |= (SCLH_FAST_8  << 8);

			// 4. Set SDADEL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 16);
			pI2CHandle->pI2C1->TIMINGR |= (SDADEL_FAST_8  << 16);

			// 5. Set SCLDEL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 20);
			pI2CHandle->pI2C1->TIMINGR |= (SCLDEL_FAST_8  << 20);
		}

		else if (pI2CHandle->I2C_PinConfig.I2C_MODE == FAST_PLUS){
			// 1. Set PSC
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 28);
			pI2CHandle->pI2C1->TIMINGR |= (PRESC_FAST_PLUS_8 << 28);

			// 2. Set SCLL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFFU << 0);
			pI2CHandle->pI2C1->TIMINGR |= (SCLL_FAST_PLUS_8  << 0);

			// 3. Set SCLH
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFFU << 8);
			pI2CHandle->pI2C1->TIMINGR |= (SCLH_FAST_PLUS_8  << 8);

			// 4. Set SDADEL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 16);
			pI2CHandle->pI2C1->TIMINGR |= (SDADEL_FAST_PLUS_8  << 16);

			// 5. Set SCLDEL
			pI2CHandle->pI2C1->TIMINGR &= ~(0xFU << 20);
			pI2CHandle->pI2C1->TIMINGR |= (SCLDEL_FAST_PLUS_8  << 20);
		}
	}
	// TODO: Implement driver for 16 MHz. Not using for now, will implement if I ever need to

}


/**************************************************************************************
 * @fn					- I2C_Peri_Control
 *
 * @brief				- Function to enable or disable/reset the I2C peripheral through the PE bit in CR1.
 * 						- This bit must be at 0 to set any configs for I2C and msut be 1 for the peripheral to work
 * 						- Setting this bit to 0 after configuration will affectively reset everything back to a normal state. (See 27.4.6 in RM for more info on this)
 *
 * @param[in]			- pI2Cx: Pointer to a I2C register definition struct which holds all registers for I2C peripheral. Allows access to PE bit in CR1.
 * @param[in]			- EnorRst: Value will be either ENABLE or DISABLE
 *
 * @return				- void
 *
 * @Note				- NOTE 1: Must remember that setting PE to 0 will affectively reset the periperhal!
 * 						- NOTE 2: As of now, pI2Cx will only be I2C1. Could have hard coded that into here but leaving it more versatile by passing a param for which I2C
 * 						-         register set/peripheral will be used (Could be I2C2 in future; need to look into this).
 * 						- NOTE 3: Possibly make this static inline and call in another function. GPT reccomended but need to understand it better before I do this
 *
 **************************************************************************************/
void I2C_Peri_Control(I2C_RegDef_t* pI2Cx, uint8_t EnorDi){
	//	TODO: Implement logic
	if (EnorDi == ENABLE){
		pI2Cx->CR1 |= (1U << 0);
	}
	else{
		pI2Cx->CR1 &= ~(1U << 0);
	}

}




/**************************************************************************************
 * @fn					- I2C_Init
 *
 * @brief				- Will initialize all config settings that are required to be set in CR1. Will essentially be all
 * 						  config settings independent of state (master/slave, TX/RX)

 *
 * @param[in]			- pI2CHandle: Struct to take in user input for config settings and set them appropriately in the register definition for I2C
 *
 * @return				- void
 *
 * @Note				- As of now, this function only handles configuration of bits in CR1. Edit this if that changes.
 * 						- See handles comment below for configs
 * 						- This must be called before I2C_Peri_Control() is called as these can only be set when PE = 0
 *
 **************************************************************************************/
void I2C_Init(I2C_Handle_t* pI2CHandle){
	/*
	 * TODO: Might add error checking with the enum I made to certain configs
	 *
	 * Handles:
	 * (1) Analog filter config
	 * (2) Digital filter config
	 * (3) Error interrupts
	 * (4) SMBus config
	 * (5) Clock strecthing config
	 * */

	// (1) Analog filtering is handled by CR1 bit 12
	if (pI2CHandle->I2C_PinConfig.I2C_ANLG_FILTER_CTRL == ANLG_FILTER_EN){
		pI2CHandle->pI2C1->CR1 &= ~(1U << 12);
	}
	else{
		pI2CHandle->pI2C1->CR1 |= (1U << 12);
	}

	// (2) Digital filtering handled by bits 8-11
	if (pI2CHandle->I2C_PinConfig.I2C_DIGI_FILTER_CTRL == DIGI_FILTER_DI){
		pI2CHandle->pI2C1->CR1 &= ~(0xFU << 8);
	}
	else{
		uint32_t tmp_CR1 = pI2CHandle->pI2C1->CR1;
		tmp_CR1 &= ~(0xFU << 8);
		tmp_CR1 |= (pI2CHandle->I2C_PinConfig.I2C_DIGI_FILTER_CTRL);

		pI2CHandle->pI2C1->CR1 = tmp_CR1;
	}

	/*(3) Error Interrupts - Bit 7 in CR1 (Disabled by default)
	 * NOTE: May need to add aditional code here. this just enables error interrupts to occur. More work needed
	 * */
	if (pI2CHandle->I2C_PinConfig.I2C_ERR_INTERRUPT_EN == ENABLE){
		pI2CHandle->pI2C1->CR1 |= (1U << 7);
	}
	else{
		pI2CHandle->pI2C1->CR1 &= ~(1U << 7);
	}

	/* (4) SMBus Config - TODO: Should set bus as (1) I2C_Mode, (2) SMBUS_HOST - Set , (3) SMBUS_DEVICE
	 * Do not need anything besides I2C_Mode for now which is default. Will just clear all bits that may set to SMBUS mode for I2C mode. Implement if ever using SMBUS
	 */

	/*
	 * (5) Config clock stretching. Only really matters when in slave mode. Handle here, may use later if using STM32 in slave mode
	 * Handled by CR1 Bit 17
	 * */

	if (pI2CHandle->I2C_PinConfig.I2C_CLK_STRETCH == ENABLE){
		pI2CHandle->pI2C1->CR1 &= ~(1U << 17);
	}
	else{
		pI2CHandle->pI2C1->CR1 |= (1U << 17);
	}

}




/**************************************************************************************
 * @fn					- I2C_Config_Master
 *
 * @brief				- Will initialize all config settings that are required to be set in CR2 to configure STM32 as master device

 *
 * @param[in]			- pI2CHandle: Struct to take in user input for config settings and set them appropriately in the register definition for I2C
 *
 * @return				- void
 *
 * @Note				- As of now, this function only handles configuration of bits in CR2. Edit this if that changes.
 * 						- See handles comment below for configs
 *
 **************************************************************************************/
I2C_Status_t I2C_Config_Master(I2C_Handle_t* pI2CHandle){
	/*
		 * Handles:
		 * (1) MUST SET: Adressing mode (7-bit or 10-bit)
		 * (2) MUST SET: Slave address selection
		 * (3) MUST SET: Transfer direction (TX/RX)
		 * (4) Program HEAD10R bit (Only matters if 10-bit addr used)
		 * (5) MUST SET: Set number of bytes to transfer
		 * (6) Config AUTOEND
		 * */


	/* (1) Adressing mode (7-bit or 10-bit) and (2) Slave address selection
	 * In 7-bit mode: SADD[7:1] should bre written with address. SADD[9], SADD[8], and SADD[0] are dont care
	 * 10-bit Mode: Uses SADD[9:0]
	 * */
	// Start by validating the address
	if ((pI2CHandle->I2C_PinConfig.I2C_ADDR_MODE == ADDR_MODE_7BIT) && (pI2CHandle->I2C_PinConfig.I2C_SLAVE_ADDR > 0x7F))
	    return I2C_ERR_INVALID_ADDR;

	if ((pI2CHandle->I2C_PinConfig.I2C_ADDR_MODE == ADDR_MODE_10BIT) && (pI2CHandle->I2C_PinConfig.I2C_SLAVE_ADDR > 0x3FF))
	    return I2C_ERR_INVALID_ADDR;

	if (pI2CHandle->I2C_PinConfig.I2C_ADDR_MODE == ADDR_MODE_7BIT){
		pI2CHandle->pI2C1->CR2 &= ~(1U << 11); // Set to 7-bit addr mode

	    uint16_t slv_addr = pI2CHandle->I2C_PinConfig.I2C_SLAVE_ADDR & 0x7F; // Ensures first 7 bits are only values set. Kind of overkill when using memset() on struct as
	    																	//  memset() ensures register value is stored in gets set to 0. (AT LEAST FOR EARLY TESTING; VERIFY)

	    pI2CHandle->pI2C1->CR2 &= ~(0x3FFU << 0); // Clears first 10-bits of CR2 which is SADD[9:0]
		pI2CHandle->pI2C1->CR2 |= (slv_addr << 1);
	}

	else if (pI2CHandle->I2C_PinConfig.I2C_ADDR_MODE == ADDR_MODE_10BIT){
		pI2CHandle->pI2C1->CR2 |= (1U << 11); // Set to 10-bit addr mode

		uint16_t slv_addr = pI2CHandle->I2C_PinConfig.I2C_SLAVE_ADDR & 0x3FF;

		pI2CHandle->pI2C1->CR2 &= ~(0x3FFU << 0); // Clears first 10-bits of CR2 which is SADD[9:0]
		pI2CHandle->pI2C1->CR2 |= slv_addr;

		/* (4) Program HEAD10R bit (Only matters if 10-bit addr used)
		 * Common to never set HEAD10R. Here if needed, but also gurantees that it is set to 0
		 * */
		if (pI2CHandle->I2C_PinConfig.I2C_HEAD10R_EN == ENABLE){
			pI2CHandle->pI2C1->CR2 |= (1U << 12);
		}
		else{
			pI2CHandle->pI2C1->CR2 &= ~(1U << 12);
		}
	}

	else{
		return I2C_ERR_INVALID_ADDR_MODE; // Alerts in driver, handled in main
	}



	/* (3) Transfer direction (TX/RX). Handled by CR2 bit 10 */
	if (pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR == WRITE){
		pI2CHandle->pI2C1->CR2 &= ~(1U << 10);
	}

	else if(pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR == READ){
		pI2CHandle->pI2C1->CR2 |= (1U << 10);
	}

	else{
		return I2C_ERR_INVALID_RD_WR;
	}

	/*(5) MUST SET: Set number of bytes to transfer.
	 * Irrelevant when in slave mode.
	 * TODO: If I ever want to transfer more than 255 bytes, I must use RELOAD register and update NBYTES. Will need to research this if necesarry
	 *  */
	uint32_t num_bytes = ((uint32_t)pI2CHandle->I2C_PinConfig.I2C_NBYTES) & (0xFFU); // Ensures only first 8-bits are set. Prevents values over 8-bits from being set

	pI2CHandle->pI2C1->CR2 &= ~(0xFFU << 16); // Clear bits 16-23 to ensure value gets set correctly
	pI2CHandle->pI2C1->CR2 |= (num_bytes << 16);


	/* (6) Config AUTOEND
	 * AUTOEND enabled means a STOP condition is automatically sent when NBYTES data are transferred.
	 * Has no effect in slave mode or when RELOAD bit is set
	 * */
	if (pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN == ENABLE){
		pI2CHandle->pI2C1->CR2 |= (1U << 25);
	}
	else{
		pI2CHandle->pI2C1->CR2 &= ~(1U << 25);
	}

	return I2C_OK;
}


// Might call this at beginning of master transmitter function
void I2C_Start(I2C_RegDef_t* pI2Cx){
	/*Set the START bit to begin data transfer*
	 * TODO: Does this bit automatically get reset after NBYTES is sent ? Understand this!
	 */

	pI2Cx->CR2 |= (1 << 13);
}

void I2C_Stop(I2C_RegDef_t* pI2Cx){
	/* Set I2C stop bit.
	 * This bit is cleared by hardware
	 * */
	pI2Cx->CR2 |= (1 << 14);
}


void I2C_Clear_Flags(I2C_RegDef_t* pI2Cx){
	// Clear the flags that are set in ISR register
	// For now just clear NACKF (bit 4) and STOPF (bit 5). Will make better late
    pI2Cx->ICR |= (1U << 4); // NACKCF
    pI2Cx->ICR |= (1U << 5); // STOPCF

}



/**************************************************************************************
 * @fn					- I2C_Master_TX
 *
 * @brief				- Calls start() and manages the transfer of data when master wants to transmit

 *
 * @param[in]			- pI2CHandle: Struct to take in user input for config settings and use them to implement logic in this function. Also allows access to I2C registers
 *						- pData_buf: Array (pointer) that contains all the data that should be transferred
 *						- repeated_start: If 1 then dont set STOP bit, else continue on and set STOP bit using current transfer completion code. This param used to allow for
 *						  this function to be used in I2C_Master_Reg_Read() which requires that STOP not set so RX can happen in same I2C event as initialization of slave reg
 *
 *
 * @return				- I2C_Status_t
 *
 * @Note				- NOTE: This function assumes that I2C_Config_Master() has been called prior to this function. This function will not function correctly if it is not
 * 						- called first.
 * 						- Also, I2C_NBYTES must be configured. This is a requirement for data TX
 * 						- This is a polling based function. WIll implement one with interrupts later and probably just use that in the future. But this is a good start
 *
 **************************************************************************************/
I2C_Status_t  I2C_Master_TX(I2C_Handle_t* pI2CHandle, uint8_t* pData_buf, uint8_t repeated_start){
	// (1) Call I2C_Start()
	// (2) Watch TXIS flag, if 1 (empty) then write data to TXDR. Else recheck(might need to better this)
	// (3) Repeat this until NBYTES is transferred and if TC = 1 (if AUTOEND = 0) or STOPF = 1 (if AUTOEND = 1)
	/* TODO:
	 * (1) Add timeout protection when polling
	 * */

	if ((pI2CHandle == NULL) || (pI2CHandle->pI2C1 == NULL) || (pData_buf == NULL) || (pI2CHandle->I2C_PinConfig.I2C_NBYTES == 0U) || (pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR != WRITE
			)) {
	    return I2C_ERR_INVALID_PARAM;
	}

	if ((repeated_start == 1U) && (pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN != 0U)) {
	    return I2C_ERR_INVALID_PARAM;
	}


	// Wait for bus to be free (not BUSY)
	// TODO: Add timeout in this so it doesnt hang forever if an error occurss (STOPF never set). Fine for now since doing early testing
	while (pI2CHandle->pI2C1->ISR & (1U << 15));

	I2C_Start(pI2CHandle->pI2C1);
	uint8_t ind = 0;



	// First handle data transfer
	while (ind < pI2CHandle->I2C_PinConfig.I2C_NBYTES){

		// Wait till TXIS is set
		while (!(pI2CHandle->pI2C1->ISR & (1U << 1))){
			// if NACK returned, return err enum
			if ((pI2CHandle->pI2C1->ISR & (1U << 4))){
				pI2CHandle->pI2C1->ICR |= (1U << 4); // Clear NACKF before returning so if called again functions correctly
                return I2C_ERR_NACK;
			}
		}

		 pI2CHandle->pI2C1->TXDR = pData_buf[ind];
		ind++;
	}


	// Then handle transfer completion
	if (pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN == 0){

		// Condition to handle if this function should be used to simply send an address to initialize slave for reading
		if (repeated_start == 1){
			while (!(pI2CHandle->pI2C1->ISR & (1U << 6))){ // Wait till TC bit is set
				if ((pI2CHandle->pI2C1->ISR & (1U << 4))){
					pI2CHandle->pI2C1->ICR |= (1U << 4); // Clear NACKF before returning so if called again functions correctly
					return I2C_ERR_NACK;
				}
			}

			return I2C_OK;
		}


		while (!(pI2CHandle->pI2C1->ISR & (1U << 6))){ // Wait till TC bit is set
			if ((pI2CHandle->pI2C1->ISR & (1U << 4))){
				pI2CHandle->pI2C1->ICR |= (1U << 4); // Clear NACKF before returning so if called again functions correctly
				return I2C_ERR_NACK;
			}
		}

		// Stop I2C after I2C TC bit is set. Then wait until STOPF flag set to ensure its actually set before code moves on
		I2C_Stop(pI2CHandle->pI2C1);
		while (!(pI2CHandle->pI2C1->ISR & (1U << 5))) {
		    if (pI2CHandle->pI2C1->ISR & (1U << 4)) {
		        pI2CHandle->pI2C1->ICR |= (1U << 4);
		        return I2C_ERR_NACK;
		    }
		}

	}

	else{
		while (!(pI2CHandle->pI2C1->ISR & (1U << 5))){ // Wait till STOPF bit is set
			if ((pI2CHandle->pI2C1->ISR & (1U << 4))){
				pI2CHandle->pI2C1->ICR |= (1U << 4); // Clear NACKF before returning so if called again functions correctly
				return I2C_ERR_NACK;
			}
		}
	}

	// Clear STOPF (using STOPCF) so another transfer can be successfully initialized
	if ((pI2CHandle->pI2C1->ISR & (1U << 5))){
		pI2CHandle->pI2C1->ICR |= (1U << 5);

	}

	return I2C_OK;

}



/**************************************************************************************
 * @fn					- I2C_Master_RX
 *
 * @brief				- Calls start() and manages the transfer of data when master wants to recieve

 *
 * @param[in]			- pI2CHandle: Struct to take in user input for config settings and use them to implement logic in this function. Also allows access to I2C registers
 *						- pData_buf: Array (pointer) that contains all the data received
 *
 * @return				- I2C_Status_t
 *
 * @Note				- NOTE: This function assumes that I2C_Config_Master() has been called prior to this function. This function will not function correctly if it is not
 * 						- called first.
 * 						- Also, I2C_NBYTES must be configured. This is a requirement for data TX
 * 						- This is a polling based function. WIll implement one with interrupts later and probably just use that in the future. But this is a good start
 *
 **************************************************************************************/
I2C_Status_t I2C_Master_RX(I2C_Handle_t* pI2CHandle, uint8_t* pData_buf, uint8_t repeated_start){
		if ((pI2CHandle == NULL) || (pI2CHandle->pI2C1 == NULL) || (pData_buf == NULL)
				|| (pI2CHandle->I2C_PinConfig.I2C_NBYTES == 0U)  || (pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR != READ)) {
			return I2C_ERR_INVALID_PARAM;
		}

		// Wait for bus to be free (not BUSY)
		// TODO: Add timeout in this so it doesnt hang forever if an error occurs (STOPF never set). Fine for now since doing early testing
		if (repeated_start != 1){ // If it = 1 then this while loop shouldnt execute because line will be busy forever
			while (pI2CHandle->pI2C1->ISR & (1U << 15));
		}

		I2C_Start(pI2CHandle->pI2C1);
		uint8_t ind = 0;

		// (1) Wait until RXNE = 1. Then read data into pData_buf
		// (2) Repeat step one NBYTES amount of times
		// (3) Handle transfer completion for AUTOEND = 0 and = 1
		// First handle data transfer
		while (ind < pI2CHandle->I2C_PinConfig.I2C_NBYTES){

			// Wait till RXNE is set
			while (!(pI2CHandle->pI2C1->ISR & (1U << 2))){
				// if NACK returned, return err enum
				if ((pI2CHandle->pI2C1->ISR & (1U << 4))){
					pI2CHandle->pI2C1->ICR |= (1U << 4); // Clear NACKF before returning so if called again functions correctly
	                return I2C_ERR_NACK;

	                // TODO: Possibly add error handles for OVR, ARLO, BERR
				}
			}

			pData_buf[ind] = (uint8_t)pI2CHandle->pI2C1->RXDR;
			ind++;
		}


		// Then handle receive completion
		// In master receive mode, hardware handles the final NACK.
		// If AUTOEND = 1, hardware also generates STOP automatically
		if (pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN == 0){

			while (!(pI2CHandle->pI2C1->ISR & (1U << 6))){ // Wait till TC bit is set

				if ((pI2CHandle->pI2C1->ISR & (1U << 4))){
					pI2CHandle->pI2C1->ICR |= (1U << 4); // Clear NACKF before returning so if called again functions correctly
					return I2C_ERR_NACK;
				}
			}

			// Stop I2C after I2C TC bit is set
			I2C_Stop(pI2CHandle->pI2C1);
			while (!(pI2CHandle->pI2C1->ISR & (1U << 5))) {
				if (pI2CHandle->pI2C1->ISR & (1U << 4)) {
					pI2CHandle->pI2C1->ICR |= (1U << 4);
					return I2C_ERR_NACK;
				}
			}
		}

		else{
			while (!(pI2CHandle->pI2C1->ISR & (1U << 5))){ // Wait till STOPF bit is set
				if ((pI2CHandle->pI2C1->ISR & (1U << 4))){
					pI2CHandle->pI2C1->ICR |= (1U << 4); // Clear NACKF before returning so if called again functions correctly
					return I2C_ERR_NACK;
				}
			}
		}

		// Clear STOPF (using STOPCF) so another transfer can be successfully initialized
		if ((pI2CHandle->pI2C1->ISR & (1U << 5))){
			pI2CHandle->pI2C1->ICR |= (1U << 5);

		}

		return I2C_OK;

}


/**************************************************************************************
 * @fn					- I2C_Master_Reg_Read
 *
 * @brief				- Will call I2C_Master_TX(repeated_start) function to initiate the register pointer of slave that I wish to read from. Then I2C_Master_RX() will be called to
 * 						- read the desired amount of bytes from slave register.
 * 						- This is a hihger-level helper function to I2C_Master_RX() that will actually allow for an proper reception of data from a slave register

 *
 * @param[in]			- pI2CHandle: Will hold the configs for TX function to set the address for RX
 *						- start_reg_addr: The address of the register that needs to be sent using the TX function to initiate the starting pointer in slave device
 *						- pData_buf: Data buffer to hold all data read from I2C
 *						- num_bytes: This value will specify the amount of bytes that should be read from the slave pointer address specified by start_reg_addr. If this number
 *						  larger than the size of start_reg_addr then it will read bytes from the next contiguous register
 *
 * @return				- I2C_Status_t: Returns a status enum, if I2C_OK outputs all is good else program stops
 *
 *
 * @Note				- IMPORTANT NOTE 1: This function writes one starting register address to slave, then reads num_bytes beginning at that address.
 *						  If the slave supports auto-increment, the multiple consecutive registers may be read in one call
 *						  If various non-contigous registers need to be read from then multiple calls to this function will need to be made!
 *
 *						- IMPORTANT NOTE 2: Configs are handled inside this function. Do not worry about setting configs and calling I2C_Config_Master() prior to this function
 *						  call as it is not needed and will most likely cause issues.
 *
 **************************************************************************************/
I2C_Status_t I2C_Master_Reg_Read(I2C_Handle_t* pI2CHandle, uint8_t start_reg_addr, uint8_t* pRx_data_buf, uint8_t num_bytes){
	/*
	 * (0): Reconfigure pI2CHandle so it is configured for WRITE, NBYTES=1, AUTOEND = 0. I believe every other config does not matter
	 * (1): Start by modifying I2C_Master_TX() so it can take a param for not generating a STOP bit
	 * (2): Call I2C_Master_TX() and pass start_reg_addr as p_dataBuf so it gets written to slave. Ensure to call repeated_start
	 * (3): Rewrite the configs so they are ready for RX
	 * (4): Then call I2C_Master_RX() to read the data and pass new configs
	 *
	 * */
	if ((pI2CHandle == NULL) || (pRx_data_buf == NULL) || (num_bytes == 0)) {
	    return I2C_ERR_INVALID_PARAM;
	}

	// Save state of all PinConfig configs that I am changing so I can restore at the end of the program
	uint8_t transfer_dir_state = pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR;
	uint8_t nbytes_state = pI2CHandle->I2C_PinConfig.I2C_NBYTES;
	uint8_t autoend_state = pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN;


	// Configure for TX of 1 byte containing register I want to read from
	pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR = WRITE;
	pI2CHandle->I2C_PinConfig.I2C_NBYTES = 1;
	pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN = DISABLE;
	I2C_Config_Master(pI2CHandle);


	uint8_t repeated_start_en = 1;
	// I2C_Master_TX() just being used to transmit an register address to initialize the pointer of the slave so its at that address
	I2C_Status_t tx_status = I2C_Master_TX(pI2CHandle, &start_reg_addr, repeated_start_en);
	if (tx_status != I2C_OK){
		pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR = transfer_dir_state;
		pI2CHandle->I2C_PinConfig.I2C_NBYTES = nbytes_state;
		pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN = autoend_state;
		I2C_Config_Master(pI2CHandle);

		return tx_status;
	}

	// Will need to verify and check these. May need to add more
	pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR = READ;
	pI2CHandle->I2C_PinConfig.I2C_NBYTES = num_bytes;
	pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN = ENABLE; // Ok to enable now since transfer done after this
	I2C_Config_Master(pI2CHandle);

	I2C_Status_t rx_status = I2C_Master_RX(pI2CHandle, pRx_data_buf, repeated_start_en);

	// Data should now be read into pRx_data_buf which can be accessed in main where function called
	if (rx_status != I2C_OK){
		pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR = transfer_dir_state;
		pI2CHandle->I2C_PinConfig.I2C_NBYTES = nbytes_state;
		pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN = autoend_state;
		I2C_Config_Master(pI2CHandle);

		return rx_status;
	}

	// Restore state of pinConfig before returning so it is in the same state as it was when the user sent it to this function
	pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR = transfer_dir_state;
	pI2CHandle->I2C_PinConfig.I2C_NBYTES = nbytes_state;
	pI2CHandle->I2C_PinConfig.I2C_AUTOEND_EN = autoend_state;
	I2C_Config_Master(pI2CHandle);

	return I2C_OK;
}

