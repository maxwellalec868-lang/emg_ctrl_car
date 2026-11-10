/*
 * emdm_device_lvl_driver.c
 *
 *  Created on: Jun 2, 2026
 *      Author: amaxw
 */

#include "emdm_device_driver.h"



/**************************************************************************************
 * @fn					- EMDM_Get_Battery_Raw
 *
 * @brief				- Obtains battery level of EMDM over I2C
 *
 * @param[in]			- pI2CHandle
 * @param[in]			-
 *
 * @return				- void
 *
 * @Note				- TODO: EITHER MODIFY THIS FUNCTION TO CONVERT RAW VALUE TO A VOLTAGE OR PERCENTAGE OR MAKE A NEW FUNCTION THAT CALLS THIS ONE
 * 						- TODO: MODIFY THIS SO I IT RETURNS RX_DATA TO MAIN WHERE I CAN THEN PRINT IT
 * 						- I WILL ONLY DO THESE 2 TODOS IF I FIGURE OUT HOW TO PRINT TO CONSOLE; IF NOT, THEN THEY DONT MATTER
 *
 **************************************************************************************/
EMDM_Status_t EMDM_Get_Battery_Raw(I2C_Handle_t* pI2CHandle){
	// Use I2C to read the battery level of the EMDM. To do this I will need
	// (1) EMDM slave address = 0x34
	// (2) Battery level register = 0x00

//	pI2CHandle->I2C_PinConfig.I2C_SLAVE_ADDR = 0x34; // Addr of EMDM; Set here just to be safe.
//	I2C_Config_Master(pI2CHandle);



	I2C_Status_t rx_status;
	uint8_t battery_reg = 0x00;

	uint8_t nbytes = 2; // assuming that this register is 2 bytes, so I am reading both of them for now; POSITIVE ITS NOT 8-BIT AND 99% SURE ITS NOT 32-BIT; THIS SHOULD BE FINE
	uint8_t rx_data[nbytes]; // IF I NEED TO READ THIS DATA I CAN SINCE I WRITE TO IT IN I2C_Master_Reg_Read

	// Read battery level
	rx_status = I2C_Master_Reg_Read(pI2CHandle, battery_reg, rx_data, nbytes);

	if (rx_status != I2C_OK){
		while(1);
	}

	return rx_status;
}

// Sets EMDM device as the slave in I2C transfer
void EMDM_Set_I2C_Slave(I2C_Handle_t* pI2CHandle){
	pI2CHandle->I2C_PinConfig.I2C_SLAVE_ADDR = 0x34; // Addr of EMDM; Set here just to be safe.
	I2C_Config_Master(pI2CHandle);
}


// Will literally just read the value in the parameter. Slightly easier then using my I2C read reg function everytime
EMDM_Status_t EMDM_Get_Reg_Value(uint8_t reg, uint8_t nbytes, I2C_Handle_t* pI2CHandle){

	I2C_Status_t rx_status;

	uint8_t rx_data[nbytes]; // IF I NEED TO READ THIS DATA I CAN SINCE I WRITE TO IT IN I2C_Master_Reg_Read

	// Read battery level
	rx_status = I2C_Master_Reg_Read(pI2CHandle, reg, rx_data, nbytes);

	return rx_status;

}



/**************************************************************************************
 * @fn					- EMDM_Set_Motor_Type
 *
 * @brief				- Set motor type on the EMDM
 *
 * @param[in]			- pI2CHandle: I2C struct to handle setting of I2C configs (slave address, transfer bytes, etc)
 * @param[in]			- pEMDMHandle: EMDM struct that will be set in main to configure the EMDM device for motor type
 *
 * @return				- EMDM_Status_t: Status of the function execution. Errors should be handled accordingly in main
 *
 * @Note				- PH!
 *
 **************************************************************************************/
EMDM_Status_t EMDM_Set_Motor_Type(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle){
	// Write motor type for EMDM; IT WILL ONLY BE MOTOR_TYPE_JGB. NOT HARD CODING IN CASE I USE EMDM WITH ANOTHER MOTOR LATER

	// Goal is to write MOTOR_TYPE_JGB to address 0x14
	uint8_t repeated_start_en = 0; // set so function operates correctly
	I2C_Status_t tx_status;
	uint8_t motor_type_addr = 0x14;

	pI2CHandle->I2C_PinConfig.I2C_NBYTES = 2;
	I2C_Config_Master(pI2CHandle);

//	uint8_t motor_type[2] = {0x14, 3};   // register 0x14, value 3
	uint8_t motor_type = pEMDMHandle->EMDM_MOTOR_TYPE;
	uint8_t data_payload[2] = {motor_type_addr, motor_type};

	tx_status = I2C_Master_TX(pI2CHandle, data_payload, repeated_start_en);
	if (tx_status != I2C_OK){
		while(1);
	}

	return tx_status;
}


/**************************************************************************************
 * @fn					- EMDM_Set_Polarity
 *
 * @brief				- Sets polarity on the EMDM
 *
 * @param[in]			- pI2CHandle: I2C struct to handle setting of I2C configs (slave address, transfer bytes, etc)
 * @param[in]			- pEMDMHandle: EMDM struct that will be set in main to configure the EMDM device for polarity
 *
 * @return				- EMDM_Status_t: Status of the function execution. Errors should be handled accordingly in main
 *
 * @Note				- PH!
 *
 **************************************************************************************/
EMDM_Status_t EMDM_Set_Polarity(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle){
	// Goal is to set polarity to what value is in pEMDMHandle at address 0x14
	uint8_t repeated_start_en = 0; // set so function operates correctly
	I2C_Status_t tx_status;
	uint8_t polarity_addr = 0x15;

	pI2CHandle->I2C_PinConfig.I2C_NBYTES = 2; // TODO: FIGURE OUT HOW MANY BYTES SHOULD BE WRITTEN HERE; I REALLY NEED TO FUCIN DO THIS; DONT PASS OVER THIS !!!
	I2C_Config_Master(pI2CHandle);			  // IF SO, USE I2C_NBYTES IN DATA_PAYLOADS[]

	uint8_t polarity = pEMDMHandle->EMDM_POLARITY;
	uint8_t data_payload[2] = {polarity_addr, polarity};

	tx_status = I2C_Master_TX(pI2CHandle, data_payload, repeated_start_en);
	if (tx_status != I2C_OK){
		while(1);
	}

	return tx_status;
}


EMDM_Status_t EMDM_Set_Fixed_Speed_Reg(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle){
	// PH!
	// Goal is to set polarity to what value is in pEMDMHandle at address 0x14
	uint8_t repeated_start_en = 0; // set so function operates correctly
	I2C_Status_t tx_status;
	uint8_t fixed_spd_addr = 0x33; // closed-loop motor address

	pI2CHandle->I2C_PinConfig.I2C_NBYTES = 5;
	I2C_Config_Master(pI2CHandle);

//	uint8_t speed_arr[4] = pEMDMHandle->EMDM_SPEED;
	uint8_t data_payload[5] = {fixed_spd_addr,
			(uint8_t)pEMDMHandle->EMDM_SPEED[0],
			(uint8_t)pEMDMHandle->EMDM_SPEED[1],
			(uint8_t)pEMDMHandle->EMDM_SPEED[2],
			(uint8_t)pEMDMHandle->EMDM_SPEED[3]
	};

	tx_status = I2C_Master_TX(pI2CHandle, data_payload, repeated_start_en);
	if (tx_status != I2C_OK){
		while(1);
	}

	return tx_status;
}

EMDM_Status_t EMDM_Stop(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle){
	pEMDMHandle->EMDM_SPEED[0] = 0;
	pEMDMHandle->EMDM_SPEED[1] = 0;
	pEMDMHandle->EMDM_SPEED[2] = 0;
	pEMDMHandle->EMDM_SPEED[3] = 0;

	EMDM_Status_t tx_status = EMDM_Set_Fixed_Speed_Reg(pEMDMHandle, pI2CHandle);

	return tx_status;


}

