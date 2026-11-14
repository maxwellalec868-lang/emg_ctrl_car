/*
 * emdm_device_lvl_driver.c
 *
 *  Created on: Jun 2, 2026
 *      Author: amaxw
 *
 *  All functions return EMDM_OK or EMDM_ERR_I2C. Errors are handled by the caller (demo / app layer).
 *  Assumes the I2C peripheral and pins are already set up and EMDM_Set_I2C_Slave() has been called.
 */

#include "emdm_device_driver.h"



/**************************************************************************************
 * @fn					- EMDM_Write (private helper)
 *
 * @brief				- Writes [reg, data[0..len-1]] to the EMDM in one I2C transfer
 *
 * @param[in]			- pI2CHandle: I2C handle (slave address already set to the EMDM)
 * @param[in]			- reg: EMDM register address (EMDM_REG_xxx)
 * @param[in]			- pData / len: bytes to write after the register address (len 1-4)
 *
 * @return				- EMDM_Status_t
 *
 * @Note				- Forces TRANSFER_DIR = WRITE and NBYTES = len + 1 so callers don't need to
 *
 **************************************************************************************/
static EMDM_Status_t EMDM_Write(I2C_Handle_t* pI2CHandle, uint8_t reg, const uint8_t* pData, uint8_t len){
	uint8_t data_payload[5]; // register address + up to 4 data bytes (4 = one byte per motor channel)

	if ((len == 0) || (len > 4)){
		return EMDM_ERR_I2C;
	}

	data_payload[0] = reg;
	for (uint8_t i = 0; i < len; i++){
		data_payload[i + 1] = pData[i];
	}

	pI2CHandle->I2C_PinConfig.I2C_TRANSFER_DIR = WRITE;
	pI2CHandle->I2C_PinConfig.I2C_NBYTES = len + 1;
	if (I2C_Config_Master(pI2CHandle) != I2C_OK){
		return EMDM_ERR_I2C;
	}

	uint8_t repeated_start_en = 0; // normal write, send STOP at the end
	if (I2C_Master_TX(pI2CHandle, data_payload, repeated_start_en) != I2C_OK){
		return EMDM_ERR_I2C;
	}

	return EMDM_OK;
}



// Sets EMDM device as the slave in I2C transfer
void EMDM_Set_I2C_Slave(I2C_Handle_t* pI2CHandle){
	pI2CHandle->I2C_PinConfig.I2C_SLAVE_ADDR = EMDM_I2C_ADDR;
	I2C_Config_Master(pI2CHandle);
}



/**************************************************************************************
 * @fn					- EMDM_Get_Battery_Raw
 *
 * @brief				- Reads the 2-byte battery value of the EMDM over I2C
 *
 * @param[in]			- pI2CHandle
 * @param[out]			- pBattery: battery value (little-endian low byte first)
 *
 * @return				- EMDM_Status_t
 *
 * @Note				- Assumed to be millivolts (not documented by Hiwonder). With a 3S pack expect ~9900-12600.
 * 						- Good first test of I2C reads since nothing moves.
 *
 **************************************************************************************/
EMDM_Status_t EMDM_Get_Battery_Raw(I2C_Handle_t* pI2CHandle, uint16_t* pBattery){
	uint8_t rx_data[2];

	if (I2C_Master_Reg_Read(pI2CHandle, EMDM_REG_BATTERY, rx_data, 2) != I2C_OK){
		return EMDM_ERR_I2C;
	}

	*pBattery = (uint16_t)(rx_data[0] | (rx_data[1] << 8));
	return EMDM_OK;
}


// Reads nbytes starting at reg into pBuf. Slightly easier then using my I2C read reg function everytime
EMDM_Status_t EMDM_Get_Reg_Value(uint8_t reg, uint8_t* pBuf, uint8_t nbytes, I2C_Handle_t* pI2CHandle){
	if (I2C_Master_Reg_Read(pI2CHandle, reg, pBuf, nbytes) != I2C_OK){
		return EMDM_ERR_I2C;
	}
	return EMDM_OK;
}



// Writes pEMDMHandle->EMDM_MOTOR_TYPE (@EMDM_MOTOR_TYPE) to the EMDM. MOTOR_TYPE_JGB for this chassis
EMDM_Status_t EMDM_Set_Motor_Type(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle){
	return EMDM_Write(pI2CHandle, EMDM_REG_MOTOR_TYPE, &pEMDMHandle->EMDM_MOTOR_TYPE, 1);
}


// Writes pEMDMHandle->EMDM_POLARITY (@EMDM_POLARITY) to the EMDM. If a motor runs away at full speed or won't move, flip this
EMDM_Status_t EMDM_Set_Polarity(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle){
	return EMDM_Write(pI2CHandle, EMDM_REG_POLARITY, &pEMDMHandle->EMDM_POLARITY, 1);
}


// Writes the 4 raw channel speeds in pEMDMHandle->EMDM_SPEED[] (M1-M4) to the closed-loop speed register
EMDM_Status_t EMDM_Set_Fixed_Speed_Reg(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle){
	return EMDM_Write(pI2CHandle, EMDM_REG_FIXED_SPEED, (const uint8_t*)pEMDMHandle->EMDM_SPEED, 4);
}



/**************************************************************************************
 * @fn					- EMDM_Set_Drive_Speed
 *
 * @brief				- Sets one signed speed for the whole car (+ = forward)
 *
 * @param[in]			- pEMDMHandle: speed gets stored in EMDM_DRIVE_SPEED on success
 * @param[in]			- pI2CHandle
 * @param[in]			- speed: clamped to +/- EMDM_SPEED_MAX
 *
 * @return				- EMDM_Status_t
 *
 * @Note				- Each channel gets speed * EMDM_DRIVE_DIR_Mx, so mirrored motors turn the right way
 * 						  and non-drive channels (dir 0) get 0
 *
 **************************************************************************************/
EMDM_Status_t EMDM_Set_Drive_Speed(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle, int8_t speed){
	static const int8_t drive_dir[4] = {EMDM_DRIVE_DIR_M1, EMDM_DRIVE_DIR_M2, EMDM_DRIVE_DIR_M3, EMDM_DRIVE_DIR_M4};

	if (speed >  EMDM_SPEED_MAX) speed =  EMDM_SPEED_MAX;
	if (speed < -EMDM_SPEED_MAX) speed = -EMDM_SPEED_MAX;

	for (uint8_t i = 0; i < 4; i++){
		pEMDMHandle->EMDM_SPEED[i] = (int8_t)(speed * drive_dir[i]);
	}

	EMDM_Status_t status = EMDM_Set_Fixed_Speed_Reg(pEMDMHandle, pI2CHandle);
	if (status == EMDM_OK){
		pEMDMHandle->EMDM_DRIVE_SPEED = speed; // only remember it if the EMDM actually got it
	}

	return status;
}


// Changes the drive speed by delta (+ = faster forward). Result is clamped to +/- EMDM_SPEED_MAX
EMDM_Status_t EMDM_Change_Drive_Speed(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle, int8_t delta){
	int16_t target = (int16_t)pEMDMHandle->EMDM_DRIVE_SPEED + delta; // int16 so e.g. 120 + 20 can't overflow

	if (target >  EMDM_SPEED_MAX) target =  EMDM_SPEED_MAX;
	if (target < -EMDM_SPEED_MAX) target = -EMDM_SPEED_MAX;

	return EMDM_Set_Drive_Speed(pEMDMHandle, pI2CHandle, (int8_t)target);
}


// Stops all motors
EMDM_Status_t EMDM_Stop(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle){
	return EMDM_Set_Drive_Speed(pEMDMHandle, pI2CHandle, 0);
}
