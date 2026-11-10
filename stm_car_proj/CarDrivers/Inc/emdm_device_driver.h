/*
 * emdm_driver.h
 *
 *  Created on: May 26, 2026
 *      Author: amaxw
 *
 *      This file is the driver for the EMDM (Encoder Motor Driver Module). This driver will be responsible for moving the vehicle forward and reversed in a controlled manner
 *      by using the closed-loop speed register.
 *      This layer will mainly be used by the next layer up
 *      TODO: MAKE MACROS FOR EMDM REGISTERS: POLARITY, MOTOR TYPE, AND CLOSED LOOP SPEED
 *
 */

#ifndef INC_EMDM_DEVICE_DRIVER_H_
#define INC_EMDM_DEVICE_DRIVER_H_

#include "stm32f334xx_i2c_driver.h" // I2C driver will be used to communicate with EMDM



// This struct will contain variables that will be used by the layer above and passed to API functions to set values at required address (same way as previous drivers)
typedef struct{
	uint8_t  EMDM_POLARITY;				/*<Possible Values from @EMDM_POLARITY>*/
	uint8_t  EMDM_MOTOR_TYPE;			/*<Possible Values from @EMDM_MOTOR_TYPE>*/
	int8_t  EMDM_SPEED[4];				/*<Possible Values from @EMDM_SPEED>*/
} EMDM_RegConfig_t;


typedef enum
{
    EMDM_OK = 0,
	EMDM_BATTERY_READ_ERR
} EMDM_Status_t;


/* *********** START OF MACROS FOR EMDM_RegConfig_t STRUCT *********** */
// @EMDM_POLARITY
#define DEFAULT							  0
#define OPPOSITE						  1


// @EMDM_MOTOR_TYPE
#define MOTOR_TYPE_WITHOUT_ENCODER        0
#define MOTOR_TYPE_TT                     1
#define MOTOR_TYPE_N20                    2
#define MOTOR_TYPE_JGB                    3  //USE THIS ONE; THESE ARE FOR THE MOTORS I HAVE FOR THIS CHASSIS; PUT OTHERS INCASE THEY ARE NEEDED LATER



/* *********** END OF MACROS FOR EMDM_RegConfig_t STRUCT *********** */


/* *********** START OF API FUNCTIONS *********** */

EMDM_Status_t EMDM_Get_Battery_Raw(I2C_Handle_t* pI2CHandle); // WILL BE READING WITH LOGIC ANALYZER; NEED TO LEARN PRINTF OR WHAT EVER IT IS TO PRINT TO CONSOLSE
void EMDM_Set_I2C_Slave(I2C_Handle_t* pI2CHandle); // Sets EMDM device as the slave in I2C transfer
EMDM_Status_t EMDM_Get_Reg_Value(uint8_t reg, uint8_t nbytes, I2C_Handle_t* pI2CHandle);
EMDM_Status_t EMDM_Set_Motor_Type(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle);
EMDM_Status_t EMDM_Set_Polarity(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle);
EMDM_Status_t EMDM_Set_Fixed_Speed_Reg(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle);
EMDM_Status_t EMDM_Stop(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle);


/* *********** END OF API FUNCTIONS *********** */


#endif /* INC_EMDM_DEVICE_DRIVER_H_ */
