/*
 * stm32f334xx_pwm_driver.h
 *
 *  Created on: Nov 5, 2025
 *      Author: amaxw
 */

#ifndef INC_STM32F334XX_PWM_DRIVER_H_
#define INC_STM32F334XX_PWM_DRIVER_H_

// Header file that contains MCU specific data (See driver notes for details)
#include "stm32f334r8.h"

/* This header will contain pwm driver specific information*/

/*Create a PWM handle structure for a PWM pin.
 * Handle structure used to gather configs and params from user (main) that will be used in driver source file to set actual values on bits of
 * respective regs.
 * */

typedef struct{
	uint8_t PWM_Cnt_Dir;			/*<Possible Values from @PWM_CNT_DIR>*/
	uint16_t PWM_PSC;				/*<Possible Values from @PWM_PRESCALARS>*/
	uint32_t PWM_ARR;				/*<Possible Values from @PWM_ARR>*/
	uint8_t PWM_Chn_Op_Mode;		/*<Possible Values from @PWM_CHANNEL_OUTPUT_MODES>*/
	uint8_t PWM_Mode;				/*<Possible Values from @PWM_MODES>*/
	uint8_t PWM_Preload;			/*<Possible Values from @PWM_PRELOAD>*/
	uint8_t PWM_Op_Pol;				/*<Possible Values from @PWM_OP_POLARITY>*/
	uint8_t PWM_Op_Cmp_Val;			/*<Possible Values from @PWM_OUTPUT_COMPARE> TODO: NOT USED YET HARD CODED IN DRIVER */
} PWM_PinConfig_t;






typedef struct
{
	// Pointer to hold the base address of the GPIO peripheral
	TIM23_RegDef_t* pTIMx; // Holds base addr of the timer (2 or 3) that is being used so that all bits in respective registers are set
	PWM_PinConfig_t PWM_PinConfig; // Holds the PWM pin config settings from user. This used in driver to pass to register locations stored in TIMx
}PWM_Handle_t;



/* *********** START OF MACROS FOR PWM_PinConfig_t STRUCT *********** */

/*
 * @PWM_CNT_DIR
 * Config options for PWM counting direction
 * NOT HANDLING CENTER-ALIGNED COUNTING HERE; NOT NEEDED YET
 * */
#define UPCOUNTING					0
#define DOWNCOUNTING				1

/*
 * @PWM_PRESCALARS
 * Config options for PWM prescalars
 * TODO: DEFINE LATER ONCE I KNOW COMMON PRESCALARS I WILL BE USING
 * */

/*
 * @PWM_ARR
 * Config options for PWM ARR values
 * TODO: DEFINE LATER ONCE I KNOW COMMON ARR VALUES I WILL BE USING
 * */

/*
 * @PWM_CHANNEL_OUTPUT_MODES
 * Config options for PWM channel output modes in the CCMR reg bit field CCxS
 * Fairly certain that only one that will be used for PWM is TIM_CCXS_CHN_OUT but not positive! ***
 * */
#define TIM_CCXS_CHN_OUT				0
#define TIM_CCXS_CHN_IN_TI1				1
#define TIM_CCXS_CHN_IN_TI2				2
#define TIM_CCXS_CHN_IN_TRC				3

/*
 * @PWM_MODES
 * Config PWM mode to either 1 or 2
 * Many output modes here but only need PWM mode 1 and mode 2 for now
 * IN DOC THESE ARE: 0110 and 0111 respectively. IGNORING 4th bit in c file, always setting it to 0 until needed
 * */
#define TIM_OP_MODE_1					6
#define TIM_OP_MODE_2					7

/*
 * @PWM_PRELOAD
 * Config PWM to perform preloading/buffering
 * */
#define TIM_PRELOAD_DI					0
#define TIM_PRELOAD_EN					1


/*
 * @PWM_OP_POLARITY
 * Config PWM polarity (Active high or active low)
 * */
#define TIM_POL_ACT_HI					0
#define TIM_POL_ACT_LOW					1

/*
 * TODO
 * @PWM_COMPLEMENTARY_OUTPUT
 * Config PWM to provide complementary output
 * */

/*
 * TODO**
 * @PWM_MAIN_OUTPUT_ENABLE
 * Config PWM to provide main output
 * */

/*
 * TODO
 * @PWM_OUTPUT_COMPARE
 * NOT EXACTLY SURE WHAT THIS DOES; LEARN MORE LATER
 * */


/* *********** END OF MACROS FOR PWM_PinConfig_t STRUCT *********** */


/* *********** START OF BIT POSITION MACROS *********** */
// DISCLAIMER: DOING THIS FOR NOW; HEARD OF SOMETHING CALLED CMSIS WHICH CONTAINS MACROS FOR THIS AMONGST OTHER STUFF
// I NEED TO UNDERSTAND IT BETTER AND DECIDE IF IT WOULD BE SMART TO USE THOSE OR WRITE MY OWN MACROS

#define TIM_CR1_DIR_POS					4
#define TIM_CR1_CEN_POS					0
#define TIM_CR1_ARPE_POS				7

#define TIM_CCMR_OC1M_POS				4
#define TIM_CCMR_CC1S_POS				0
#define TIM_CCMR_OC1PE_POS				3

#define TIM_CCER_CC1E_POS				0
#define TIM_CCER_CC1P_POS				1
#define TIM_CCER_CC1NP_POS				3

#define TIM_EGR_UG_POS					0
/* *********** END OF BIT POSITION MACROS *********** */



/**************************************************************************************
 * 							APIs Supported By This Driver
 * 				For More Information About the APIs Check the Function Definitions
 **************************************************************************************/

void pwm_init(PWM_Handle_t* pTIMx);

void pwm_start(PWM_Handle_t* pTIMHandle); //MAY NEED TO MODIFY THIS

void pwm_stop(PWM_Handle_t* pTIMHandle);

void pwm_PeriClockControl(uint8_t EnorDi, uint8_t tmr_select);

// IMPROVE ALL ASPECTS OF THESE !!!
// THIS FUNCTION WILL SET THE PRESCALAR TO OBTAIN COUNTER_FREQ PARAM
// TODO: DO THIS AFTER I ENSURE PREVIOUS 3 FUNCTIONS WORK PROPERLY
//uint32_t pwm_psc_calc(uint32_t counter_freq);

// THIS FUNCTION WILL SET THE counter_freq TO OBTAIN PWM_FREQ
// TODO: DO THIS AFTER I ENSURE PREVIOUS 3 FUNCTIONS WORK PROPERLY
//uint32_t pwm_freq_calc(uint32_t counter_freq);

#endif /* INC_STM32F334XX_PWM_DRIVER_H_ */
