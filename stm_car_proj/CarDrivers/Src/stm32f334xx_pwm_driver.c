/*
 * stm32f334xx_pwm_driver.c
 *
 *  Created on: Nov 5, 2025
 *      Author: amaxw
 */

#include "stm32f334xx_pwm_driver.h"



void pwm_PeriClockControl(uint8_t EnorDi, uint8_t tmr_select){
	if (EnorDi == ENABLE && tmr_select == TIM_SELECT_2){
		TIM2_PCLK_EN();
	}
	else if(EnorDi == ENABLE && tmr_select == TIM_SELECT_3){
		TIM3_PCLK_EN();
	}
	else if(EnorDi == DISABLE && tmr_select == TIM_SELECT_2){
		TIM2_PCLK_DI();
	}
	else{
		TIM3_PCLK_DI();
	}
}


// IMPORTANT NOTE: THIS DRIVER CURRENTLY ONLY SUPPORTS CHANNEL 1. ONCE I TEST AND VERIFY IT FUNCTIONS FOR CHANNEL 1, I WILL IMPROVE IT SO THAT IT WORKS FOR ALL 4 CHANNELS OF TIM2 AND TIM3
void pwm_init(PWM_Handle_t* pTIMHandle){
	// TODO
//	(a) Set counting direction
	if (pTIMHandle->PWM_PinConfig.PWM_Cnt_Dir == UPCOUNTING){
		pTIMHandle->pTIMx->CR1 &= ~(1 << TIM_CR1_DIR_POS); // Clears bit position 4 which when 0, in upcounting mode
	}
	else{
		pTIMHandle->pTIMx->CR1 |= (1 << TIM_CR1_DIR_POS);
	}

//	(b) Set prescalar value
	pTIMHandle->pTIMx->PSC = pTIMHandle->PWM_PinConfig.PWM_PSC; // What ever value determined in main will get set here

//	(c) Set ARR value
	pTIMHandle->pTIMx->ARR = pTIMHandle->PWM_PinConfig.PWM_ARR; // What ever value determined in main will get set here

//	(d) Ensure CC1E of TIMx_CCER is off (0) so CC1S of CCMR is writeable and set CC1S bits to output mode so Channel 1 is in output mode (00)
	pTIMHandle->pTIMx->CCER &= ~(1 << TIM_CCER_CC1E_POS);
	// TODO: MAKE MORE VERSATILE SO ANY CHANNEL CAN BE SELECTED; IMPROVE THIS LATER ONCE IT WORKS ON CHANNEL 1
	pTIMHandle->pTIMx->CCMR1 &= ~(0x3U << TIM_CCMR_CC1S_POS);
	pTIMHandle->pTIMx->CCMR1 |= (pTIMHandle->PWM_PinConfig.PWM_Chn_Op_Mode << TIM_CCMR_CC1S_POS);

//	(e) Clear output compare mode bits for respective channel (1,2,3,4)
	pTIMHandle->pTIMx->CCMR1 &= ~(0x7U << TIM_CCMR_OC1M_POS); // Setting 7 here cause 7 = 0b111
	pTIMHandle->pTIMx->CCMR1 &= ~(0x1U << 16); // Clearing bit 16 since it is 4 bit of bit field of OC1M (See rf documentation for more info) and needs to be 0 so PWM modes can be selected

//	(f) Select PWM mode on respective channel by modifying CCMR regs OC1M bit field
	pTIMHandle->pTIMx->CCMR1 |= (pTIMHandle->PWM_PinConfig.PWM_Mode << TIM_CCMR_OC1M_POS); // Set PWM to mode 1 (0110) or 2 (0111)

//	(g) Configure Preload for ARR and CCR
	if (pTIMHandle->PWM_PinConfig.PWM_Preload == TIM_PRELOAD_EN){
		pTIMHandle->pTIMx->CCMR1 |= (1 << TIM_CCMR_OC1PE_POS);
		pTIMHandle->pTIMx->CR1 |= (1 << TIM_CR1_ARPE_POS);
	}
	else{
		pTIMHandle->pTIMx->CCMR1 &= ~(1 << TIM_CCMR_OC1PE_POS);
		pTIMHandle->pTIMx->CR1 &= ~(1 << TIM_CR1_ARPE_POS);
	}

//	(h) Configure output polarity. Will always be in output mode in PWM so don't need to deal with CC1NP and input mode configs here
	if (pTIMHandle->PWM_PinConfig.PWM_Op_Pol == TIM_POL_ACT_LOW){
		pTIMHandle->pTIMx->CCER |= (1 << TIM_CCER_CC1P_POS);
		pTIMHandle->pTIMx->CCER &= ~(1 << TIM_CCER_CC1NP_POS); // When in output mode (always will be for PWM), need to ensure this bit is 0
	}
	else{
		pTIMHandle->pTIMx->CCER &= ~(1 << TIM_CCER_CC1P_POS);
		pTIMHandle->pTIMx->CCER &= ~(1 << TIM_CCER_CC1NP_POS); // When in output mode (always will be for PWM), need to ensure this bit is 0
	}

// (i) Configure output compare register by setting an initial value for the duty cycle (MAY MOVE THIS TO ITS OWN FUNCTION)
	pTIMHandle->pTIMx->CCR1 = 5; // Starting off with a duty cycle of 500; THIS IS FAIRLY SPECIFIC TO DIMMING LED EXAMPLE, MAY NEED TO CHANGE THIS OR ACCEPT INPUT FROM MAIN

// (j) Force an update using EGR reg bit field UG to latch PSC/ARR/CCR instantly. Helps ensure no glitches when running
	pTIMHandle->pTIMx->EGR |= (1 << TIM_EGR_UG_POS);

}

//MAY NEED TO MODIFY THIS
void pwm_start(PWM_Handle_t* pTIMHandle){
	// (a) Enable main Output in CCER bits CC1E
	pTIMHandle->pTIMx->CCER |= (1 << TIM_CCER_CC1E_POS);

	// (b) Enable Counter
	pTIMHandle->pTIMx->CR1 |= (1 << TIM_CR1_CEN_POS);
}

void pwm_stop(PWM_Handle_t* pTIMHandle){
	// (a) Disnable main Output in CCER bits CC1E
	pTIMHandle->pTIMx->CCER &= ~(1 << TIM_CCER_CC1E_POS);

	// (b) Disable Counter
	pTIMHandle->pTIMx->CR1 &= ~(1 << TIM_CR1_CEN_POS);
}



