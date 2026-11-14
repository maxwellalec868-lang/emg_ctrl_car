/*
 * emdm_driver.h
 *
 *  Created on: May 26, 2026
 *      Author: amaxw
 *
 *      This file is the driver for the EMDM (Encoder Motor Driver Module). This driver will be responsible for moving the vehicle forward and reversed in a controlled manner
 *      by using the closed-loop speed register.
 *      This layer will mainly be used by the next layer up
 *
 *      Register map from the Hiwonder Ackermann chassis tutorial (addresses are decimal in their code).
 *
 */

#ifndef INC_EMDM_DEVICE_DRIVER_H_
#define INC_EMDM_DEVICE_DRIVER_H_

#include "stm32f334xx_i2c_driver.h" // I2C driver will be used to communicate with EMDM



// This struct will contain variables that will be used by the layer above and passed to API functions to set values at required address (same way as previous drivers)
typedef struct{
	uint8_t  EMDM_POLARITY;				/*<Possible Values from @EMDM_POLARITY>*/
	uint8_t  EMDM_MOTOR_TYPE;			/*<Possible Values from @EMDM_MOTOR_TYPE>*/
	int8_t   EMDM_SPEED[4];				/*<Raw per-channel speed M1-M4, last value written to EMDM_REG_FIXED_SPEED>*/
	int8_t   EMDM_DRIVE_SPEED;			/*<Current drive speed set by EMDM_Set_Drive_Speed(); + = forward>*/
} EMDM_RegConfig_t;


typedef enum
{
    EMDM_OK = 0,
	EMDM_ERR_I2C			// Any I2C failure (NACK, bad config, ...)
} EMDM_Status_t;


/* *********** START OF EMDM REGISTER MACROS *********** */
#define EMDM_I2C_ADDR					  0x34
#define EMDM_REG_BATTERY				  0   // 2 bytes, little-endian. Assumed mV (3S pack -> ~9900-12600); verify on hardware
#define EMDM_REG_MOTOR_TYPE				  20  // 1 byte, @EMDM_MOTOR_TYPE
#define EMDM_REG_POLARITY				  21  // 1 byte, @EMDM_POLARITY (encoder direction)
#define EMDM_REG_FIXED_PWM				  31  // 4 x int8, open loop, -100..100 (not used yet)
#define EMDM_REG_FIXED_SPEED			  51  // 4 x int8, closed loop, encoder pulses per 10 ms (hardware max ~±50)
#define EMDM_REG_ENCODER_TOTAL			  60  // 4 x int32 total pulse counts (not used yet)
/* *********** END OF EMDM REGISTER MACROS *********** */


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


/* *********** START OF DRIVE MACROS *********** */
// Software speed limit for EMDM_Set_Drive_Speed()/EMDM_Change_Drive_Speed(). Hardware max is ~50; kept low for testing
#define EMDM_SPEED_MAX					  30

// Direction each channel must turn for the car to go FORWARD: +1, -1, or 0 = not a drive motor.
// Rear motors are on M1 and M3 and are mounted mirrored, so their signs are opposite.
// Confirmed on hardware with demo_emdm_bench_run() (Nov 2026): M1 = +1, M3 = -1 drives forward
#define EMDM_DRIVE_DIR_M1				  (+1)
#define EMDM_DRIVE_DIR_M2				  0
#define EMDM_DRIVE_DIR_M3				  (-1)
#define EMDM_DRIVE_DIR_M4				  0
/* *********** END OF DRIVE MACROS *********** */


/* *********** START OF API FUNCTIONS *********** */

// Setup / reads
void EMDM_Set_I2C_Slave(I2C_Handle_t* pI2CHandle); // Sets EMDM device as the slave in I2C transfer
EMDM_Status_t EMDM_Get_Battery_Raw(I2C_Handle_t* pI2CHandle, uint16_t* pBattery);
EMDM_Status_t EMDM_Get_Reg_Value(uint8_t reg, uint8_t* pBuf, uint8_t nbytes, I2C_Handle_t* pI2CHandle);
EMDM_Status_t EMDM_Set_Motor_Type(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle);
EMDM_Status_t EMDM_Set_Polarity(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle);

// Raw per-channel control: writes pEMDMHandle->EMDM_SPEED[0..3] as-is
EMDM_Status_t EMDM_Set_Fixed_Speed_Reg(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle);

// Drive-level control: one signed speed for the whole car (+ = forward), mapped to M1-M4 with EMDM_DRIVE_DIR_Mx
EMDM_Status_t EMDM_Set_Drive_Speed(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle, int8_t speed);    // absolute
EMDM_Status_t EMDM_Change_Drive_Speed(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle, int8_t delta); // relative
EMDM_Status_t EMDM_Stop(EMDM_RegConfig_t* pEMDMHandle, I2C_Handle_t* pI2CHandle);


/* *********** END OF API FUNCTIONS *********** */


#endif /* INC_EMDM_DEVICE_DRIVER_H_ */
