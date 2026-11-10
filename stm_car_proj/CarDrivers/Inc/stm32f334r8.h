/*
 * stm32f334r8.h
 *
 *  Created on: Oct 1, 2025
 *      Author: amaxw
 *
 * This header file describes the MCU.It is the MCU specific header file for my board Nucelo-F334R8
 * Will include things like base addresses for peripherals/memories/busses, IRQ, defs and more.
 * Includes:
 * (1) Macro Definitions
 * (2) Memory Component Base Addresses
 * (3) Bus Peripheral Base Address
 * Refer to Must Have Header Files for everything that is and should be included here.
 */


#ifndef INC_STM32F334R8_H_
#define INC_STM32F334R8_H_



#include <stdint.h>
#include <stdio.h> // Only for NUll as of now

/* *********** (0) BEGINNING OF PROCESSOR SPECIFIC DETAILS *********** */
/*
 * ARM Cortex M4 Processor NVIC ISERx Base Register Addresses (Interrupt Set Enable)
 * */
# define NVIC_ISER0		((volatile uint32_t*)0xE000E100)
# define NVIC_ISER1		((volatile uint32_t*)0xE000E104)
# define NVIC_ISER2		((volatile uint32_t*)0xE000E108)

/*
 * ARM Cortex M4 Processor NVIC ICERx Base Register Addresses (Interrupt Clear)
 * */
# define NVIC_ICER0		((volatile uint32_t*)0xE000E180)
# define NVIC_ICER1		((volatile uint32_t*)0xE000E184)
# define NVIC_ICER2		((volatile uint32_t*)0xE000E188)

/*
 * ARM Cortex M4 Processor NVIC Priority Register Base Addresses
 * */
# define NVIC_PR_BASE_ADDR 		((volatile uint32_t*)0xE000E400)


#define  NUM_PR_BITS_IMPLEMENTED			4 					// THIS MACRO SO BITS GET SHIFTED CORERCTLY WHEN ENTERED INTO IRQ PRIORITY REGISTERS (UNDERSTAND THIS BETTER)
/* *********** (0) END OF PROCESSOR SPECIFIC DETAILS *********** */



/* *********** (1) BEGINNING OF MACRO DEFINITIONS *********** */
#define __vo volatile
#define ENABLE 				1
#define DISABLE				0
#define SET 				ENABLE
#define RESET				DISABLE
#define GPIO_PIN_SET		SET			// Not sure why i cant just use set? Udemy guy did this so leave for now
#define GPIO_PIN_RESET		RESET

/*
 * IRQ (Interrupt Request) Numbers
 * */
#define IRQ_NO_EXTI0		6
#define IRQ_NO_EXTI1		7
#define IRQ_NO_EXTI2		8
#define IRQ_NO_EXTI3		9
#define IRQ_NO_EXTI4		10
#define IRQ_NO_EXTI9_5		23
#define IRQ_NO_EXTI15_10	40

/*
 * IRQ (Interrupt Request) Numbers
 * NEED TO KNOW/UNDERSTAND IF THERE ARE ONLY 15 LEVEL OF PRIORITIES; I THINK MORE BUT ONLY DEFINING 15 FOR NOW
 * */
#define NVIC_IRQ_PRIO0		0
#define NVIC_IRQ_PRIO1		1
#define NVIC_IRQ_PRIO2		2
#define NVIC_IRQ_PRIO3		3
#define NVIC_IRQ_PRIO4		4
#define NVIC_IRQ_PRIO5		5
#define NVIC_IRQ_PRIO6		6
#define NVIC_IRQ_PRIO7		7
#define NVIC_IRQ_PRIO8		8
#define NVIC_IRQ_PRIO9		9
#define NVIC_IRQ_PRIO10		10
#define NVIC_IRQ_PRIO11		11
#define NVIC_IRQ_PRIO12		12
#define NVIC_IRQ_PRIO13		13
#define NVIC_IRQ_PRIO14		14
#define NVIC_IRQ_PRIO15		15

#define TIM_SELECT_2		2
#define TIM_SELECT_3		3


/* *********** (1) END OF MACRO DEFINITIONS *********** */



/* *********** (2) BEGINING OF MEMORY COMPONENT BASE ADDRESSES *********** */

// By default treated as signed; Must by type cast to (uint32_t) or simply right U at end of address
#define FLASH_BASEADDR			0x08000000U
#define SRAM1_BASEADDR			0x20000000U
#define ROM 					0x1FFFD800U  // Not labeled as ROM in documentation. It is System memory section of the information block in flash memory.Section contains boot loader and is ROM already, so can be used as user ROM
#define SRAM 					SRAM1_BASEADDR
/* *********** (2) END OF MEMORY COMPONENT BASE ADDRESSES *********** */



/* *********** (3) BEGINING OF BUS PERIPHERAL BASE ADDRESSES *********** */
#define PERIPH_BASEADDR				0x40000000		// This is also the APB1 base Address; Stored in Macro below (NOT SURE WHY HE DID THIS IN VID, MIGHT FIND OUT)
#define APB1PERIPH_BASEADDR			PERIPH_BASEADDR
#define APB2PERIPH_BASEADDR			0x40010000
#define AHB1PERIPH_BASEADDR			0x40020000
#define AHB2PERIPH_BASEADDR			0x48000000
/* *********** (3) END OF BUS PERIPHERAL BASE ADDRESSES *********** */



/* *********** (4) BEGINING OF BASE ADDRESS OF ALL PERIPHERALS ON EACH BUS *********** */
// AHB1 Peripheral Base Addresses
#define DMA1_BASEADDR				(AHB1PERIPH_BASEADDR + 0x0000)
#define RCC_BASEADDR				(AHB1PERIPH_BASEADDR + 0x1000)
#define FLSI_BASEADDR				(AHB1PERIPH_BASEADDR + 0x2000)
#define CRC_BASEADDR				(AHB1PERIPH_BASEADDR + 0x3000)
#define TSC_BASEADDR				(AHB1PERIPH_BASEADDR + 0x4000)


// AHB2 Bus Peripheral Base Addresses
#define GPIOA_BASEADDR				(AHB2PERIPH_BASEADDR + 0x0000) // All these addr ranges store 1 KB of data
#define GPIOB_BASEADDR				(AHB2PERIPH_BASEADDR + 0x0400)
#define GPIOC_BASEADDR				(AHB2PERIPH_BASEADDR + 0x0800)
#define GPIOD_BASEADDR				(AHB2PERIPH_BASEADDR + 0x0C00)
#define GPIOF_BASEADDR				(AHB2PERIPH_BASEADDR + 0x1400)


// APB1 Bus Peripheral Base Addresses
#define TIM2_BASEADDR				(APB1PERIPH_BASEADDR + 0x0000)
#define TIM3_BASEADDR				(APB1PERIPH_BASEADDR + 0x0400)
#define TIM6_BASEADDR				(APB1PERIPH_BASEADDR + 0x1000)
#define TIM7_BASEADDR				(APB1PERIPH_BASEADDR + 0x1400)

#define RTC_BASEADDR				(APB1PERIPH_BASEADDR + 0x2800)
#define WWDG_BASEADDR				(APB1PERIPH_BASEADDR + 0x2C00)		// Not sure what this is; Define Here Later
#define IWDG_BASEADDR				(APB1PERIPH_BASEADDR + 0x3000)		// Not sure what this is; Define Here Later

#define USART2_BASEADDR				(APB1PERIPH_BASEADDR + 0x4400)
#define USART3_BASEADDR				(APB1PERIPH_BASEADDR + 0x4800)

#define I2C1_BASEADDR				(APB1PERIPH_BASEADDR + 0x5400)

#define bxCAN_BASEADDR				(APB1PERIPH_BASEADDR + 0x6400)		// Not sure what this is; Define Here Later

#define PWR_BASEADDR				(APB1PERIPH_BASEADDR + 0x7000)		// Not sure what this is; Define Here Later

#define DAC1_BASEADDR				(APB1PERIPH_BASEADDR + 0x7400)
#define DAC2_BASEADDR				(APB1PERIPH_BASEADDR + 0x9800)


// APB2 Bus Peripheral Base Addresses
#define SYSCFG_BASEADDR				(APB2PERIPH_BASEADDR + 0x0000)		// Also peripheral for COMP and OP-AMP; Need to learn these more
#define EXTI_BASEADDR				(APB2PERIPH_BASEADDR + 0x0400)		// Extended Interrupts and Events Controller

#define SPI1_BASEADDR				(APB2PERIPH_BASEADDR + 0x3000)
#define USART1_BASEADDR				(APB2PERIPH_BASEADDR + 0x3800)

#define TIM1_BASEADDR				(APB2PERIPH_BASEADDR + 0x2C00)
#define TIM15_BASEADDR				(APB2PERIPH_BASEADDR + 0x4000)
#define TIM16_BASEADDR				(APB2PERIPH_BASEADDR + 0x4400)
#define TIM17_BASEADDR				(APB2PERIPH_BASEADDR + 0x4800)
#define HRTIM1_BASEADDR				(APB2PERIPH_BASEADDR + 0x7400)
/* *********** (4) END OF BASE ADDRESS OF ALL PERIPHERALS ON EACH BUS *********** */



/* *********** (5) BEGINNING OF STRUCTS FOR PERIPHERAL REGISTERS *********** */
/* Starting with GPIO struct. This struct can be used for any port because all that needs to be done is define a pointer of this type
 * and have it point to the base addr of the required port.
 * This works because each port has the same register structure and offsetting so all that needs to change to use a different port is the base addr.
 *
 * Good to declare all registers in a peripheral register struct as volatile for now. Some many not need it but registers such as IDR which, if in input mode, would get written to
 * be written to each clock cycle. So it is crucial to be volatile as we want this register to be updated every single clock cycle and to not be optimized.
 * Declaring all volatile will not hurt performance in this project. I should understand is what registers should and should not be
 * volatile because it could hurt performance in larger applications if registers are unnecessarily declared volatile.
 *
 * STRUCT PROPERTIES IS HOW THESE REGISTERS GET ASSIGNED OFFSET ADDRESSES. PUTTING ONE AFTER THE OTHER INCREMENTS THE ADDRESS AND ASSIGNS THE REGISTER THE CORRECT OFFSET
 * SOME CRUCIAL POINTS HERE ARE:
 * (1) EXTREMELY IMPORTANT TO INCLUDE RESERVED REGISTERS TO PRESERVE OFFSET ORDERING
 * (2) SINCE ALL UINT32_T 4 BYTES ALLOCATED EACH, SO EACH ADDRESS 4 BYTES APART
 * (3) EXTREMELY IMPORTANT THAT BASE ADDRESES THESE STRUCTS POINT TO ARE CORRECT OR EVERYTHING WILL BREAK HERE
 * */
typedef struct{
	__vo uint32_t MODER;		/*Port mode register; select input, output, AF, or analog:	ADDRESS OFFSET: 0x00*/
	__vo uint32_t OTYPER;		/*Output type; 0 = push-pull, 1 = open-drain:				ADDRESS OFFSET: 0x04*/
	__vo uint32_t OSPEEDR;		/*Output speed register; 									ADDRESS OFFSET: 0x08*/
	__vo uint32_t PUPDR;		/*Pull-up/pull-down register								ADDRESS OFFSET: 0x0C*/
	__vo uint32_t IDR;			/*Input Data Register										ADDRESS OFFSET: 0x10*/
	__vo uint32_t ODR;			/*Output Data Register										ADDRESS OFFSET: 0x14*/
	__vo uint32_t BSRR;			/*Bit Set/Reset Register 									ADDRESS OFFSET: 0x18*/
	__vo uint32_t LCKR;			/*Configuration Lock Register								ADDRESS OFFSET: 0x1C*/
	__vo uint32_t AFRL;			/*Alternate Function Low Register 							ADDRESS OFFSET: 0x20*/
	__vo uint32_t AFRH;			/*Alternate Function High Register 							ADDRESS OFFSET: 0x24*/
	__vo uint32_t BRR;			/*Bit Reset Register 										ADDRESS OFFSET: 0x28*/
}GPIO_RegDef_t;



/*
 * Peripheral Register Definition Structure for RCC
 * */
typedef struct{
	__vo uint32_t CR;			/*Clock control register									ADDRESS OFFSET: 0x00*/
	__vo uint32_t CFGR;			/*Clock configuration register								ADDRESS OFFSET: 0x04*/
	__vo uint32_t CIR;			/*clock interrupt register									ADDRESS OFFSET: 0x08*/
	__vo uint32_t APB2RSTR;		/*APB2 peripheral reset register							ADDRESS OFFSET: 0x0C*/
	__vo uint32_t APB1RSTR; 	/*APB1 peripheral reset register							ADDRESS OFFSET: 0x10*/
	__vo uint32_t AHBENR;		/*AHB peripheral clock enable register						ADDRESS OFFSET: 0x14*/
	__vo uint32_t APB2ENR;		/*APB2 peripheral clock enable								ADDRESS OFFSET: 0x18*/
	__vo uint32_t APB1ENR;		/*APB1 peripheral clock enable								ADDRESS OFFSET: 0x1C*/
	__vo uint32_t BDCR;			/*Domain control register									ADDRESS OFFSET: 0x20*/
	__vo uint32_t CSR;			/*Control status register									ADDRESS OFFSET: 0x24*/
	__vo uint32_t AHBRSTR;		/*AHB Peripheral reset register								ADDRESS OFFSET: 0x28*/
	__vo uint32_t CFGR2;		/*Clock config register 2									ADDRESS OFFSET: 0x2C*/
	__vo uint32_t CFGR3;		/*Clock config register 3									ADDRESS OFFSET: 0x30*/
}RCC_RegDef_t;



/*
 * Peripheral Register Definition Structure for EXTI
 * */
typedef struct{
	__vo uint32_t EXTI_IMR1;	/*Interrupt mask register									ADDRESS OFFSET: 0x00*/
	__vo uint32_t EXTI_EMR1;	/*Event mask register										ADDRESS OFFSET: 0x04*/
	__vo uint32_t EXTI_RTSR1;	/*Rising trigger selection register							ADDRESS OFFSET: 0x08*/
	__vo uint32_t EXTI_FTSR1;	/*Falling trigger selection register						ADDRESS OFFSET: 0x0C*/
	__vo uint32_t EXTI_SWIER1; 	/*Software interrupt event register							ADDRESS OFFSET: 0x10*/
	__vo uint32_t EXTI_PR1;		/*Pending register											ADDRESS OFFSET: 0x14*/
}EXTI_RegDef_t;



/*
 * Peripheral Register Definition Structure for SYSCFG
 * */
typedef struct{
	__vo uint32_t CFGR1;			/*SYSCFG config register								ADDRESS OFFSET: 0x00*/
	__vo uint32_t RCR;				/*CCM SRAM protection register							ADDRESS OFFSET: 0x04*/
//	__vo uint32_t SYSCFG_EXTICR1;	/*SYSCFG external interrupt config register				ADDRESS OFFSET: 0x08*/
//	__vo uint32_t SYSCFG_EXTICR2;	/*SYSCFG external interrupt config register				ADDRESS OFFSET: 0x0C*/
//	__vo uint32_t SYSCFG_EXTICR3; 	/*SYSCFG external interrupt config register				ADDRESS OFFSET: 0x10*/
//	__vo uint32_t SYSCFG_EXTICR4;	/*SYSCFG external interrupt config register				ADDRESS OFFSET: 0x14*/
	__vo uint32_t EXTICR[4];		/*SYSCFG external interrupt config register				ADDRESS OFFSET: 0x08-0x14*/
	__vo uint32_t CFGR2;			/*SYSCFG config register								ADDRESS OFFSET: 0x18*/
}SYSCFG_RegDef_t;


/*
 * Peripheral Register Definition Structure for TIM2/3
 * Only doing TIM2/3 here since all other timers have different register structures so they will need there own structures
 * ALEC DEFINED STRUCT; HIGHER CHANCE OF BEING WRONG
 * [alternate] = Register functions differently depending on other register configurations
 * SEE DOCUMENTATION FOR MORE INFO ON THIS
 * */
typedef struct{
	__vo uint32_t CR1;				/*Control register 1									ADDRESS OFFSET: 0x00*/
	__vo uint32_t CR2;				/*Control register 2									ADDRESS OFFSET: 0x04*/
	__vo uint32_t SMCR;				/*Slave mode control register							ADDRESS OFFSET: 0x08*/
	__vo uint32_t DIER;				/*DMA/Interrupt enable register							ADDRESS OFFSET: 0x0C*/
	__vo uint32_t SR; 				/*Status register										ADDRESS OFFSET: 0x10*/
	__vo uint32_t EGR;				/*Event generation register								ADDRESS OFFSET: 0x14*/
	__vo uint32_t CCMR1;			/*Capture/compare mode register 1 [alternate]			ADDRESS OFFSET: 0x18*/
	__vo uint32_t CCMR2;			/*Capture/compare mode register 2 [alternate]			ADDRESS OFFSET: 0x1C*/
	__vo uint32_t CCER;				/*Capture/compare enable register						ADDRESS OFFSET: 0x20*/
	__vo uint32_t CNT;				/*Counter register [alternate]							ADDRESS OFFSET: 0x24*/
	__vo uint32_t PSC;				/*Prescalar register									ADDRESS OFFSET: 0x28*/
	__vo uint32_t ARR;				/*Auto-reload register									ADDRESS OFFSET: 0x2C*/
	__vo uint32_t RESERVED;			/*RESERVED MEMORY										ADDRESS OFFSET: 0x30*/
	__vo uint32_t CCR1;				/*Capture/compare register 1							ADDRESS OFFSET: 0x34*/
	__vo uint32_t CCR2;				/*Capture/compare register 2							ADDRESS OFFSET: 0x38*/
	__vo uint32_t CCR3;				/*capture/compare register 3							ADDRESS OFFSET: 0x3C*/
	__vo uint32_t CCR4;				/*capture/compare register 4							ADDRESS OFFSET: 0x40*/
	__vo uint32_t RESERVED2;		/*2nd RESERVED MEMORY									ADDRESS OFFSET: 0x44*/
	__vo uint32_t DCR;				/*DMA Control register									ADDRESS OFFSET: 0x48*/
	__vo uint32_t DMAR;				/*DMA address for full transfer register				ADDRESS OFFSET: 0x4C*/
}TIM23_RegDef_t;


typedef struct{
	__vo uint32_t CR1;			/* I2C control register									ADDRESS OFFSET: 0x00*/
	__vo uint32_t CR2;			/* I2C control register	2								ADDRESS OFFSET: 0x04*/
	__vo uint32_t OAR1;			/* Own address 1 										ADDRESS OFFSET: 0x08*/
	__vo uint32_t OAR2;			/* Own address 2										ADDRESS OFFSET: 0x0C*/
	__vo uint32_t TIMINGR; 		/* I2C Timing Register 									ADDRESS OFFSET: 0x10*/
	__vo uint32_t TIMEOUTR;		/* I2C Timeout Register									ADDRESS OFFSET: 0x14*/
	__vo uint32_t ISR;			/* I2C Interrupt and Status Register					ADDRESS OFFSET: 0x18*/
	__vo uint32_t ICR;			/* I2C Interrupt and Clear Register 					ADDRESS OFFSET: 0x1C*/
	__vo uint32_t PECR;			/* I2C PEC Register 									ADDRESS OFFSET: 0x20*/
	__vo uint32_t RXDR;			/* I2C Recieve Data Register 							ADDRESS OFFSET: 0x24*/
	__vo uint32_t TXDR;			/* I2C Transmit Data Register 							ADDRESS OFFSET: 0x28*/
}I2C_RegDef_t;


/* *********** (5) END OF STRUCTS FOR PERIPHERAL REGISTERS *********** */



/* *********** (6) BEGINNING OF PERIPHERAL DEFINITIONS *********** */
/* NOTES:
 * Peripheral Definitions (Peripheral base addresses type casted to xxx_RegDef_t)
 * */
#define GPIOA			((GPIO_RegDef_t*)GPIOA_BASEADDR)		/*Essentially a pointer to the ports base addr of type GPIO_RegDef_t */
#define GPIOB			((GPIO_RegDef_t*)GPIOB_BASEADDR)
#define GPIOC			((GPIO_RegDef_t*)GPIOC_BASEADDR)
#define GPIOD			((GPIO_RegDef_t*)GPIOD_BASEADDR)
#define GPIOF			((GPIO_RegDef_t*)GPIOF_BASEADDR)

#define RCC				((RCC_RegDef_t*)RCC_BASEADDR)

#define EXTI			((EXTI_RegDef_t*)EXTI_BASEADDR)

#define SYSCFG			((SYSCFG_RegDef_t*)SYSCFG_BASEADDR)

// MAY NEED TO VERIFY THESE
#define TIM2			((TIM23_RegDef_t*)TIM2_BASEADDR)
#define TIM3			((TIM23_RegDef_t*)TIM3_BASEADDR)

#define I2C1			((I2C_RegDef_t*)I2C1_BASEADDR)
/* *********** (6) END OF PERIPHERAL DEFINITIONS *********** */



/* *********** (7) BEGINNING OF CLOCK ENABLE/DISABLE MACROS *********** */
/*
 * Clock enable Macros for GPIOx
 * */
#define GPIOA_PCLK_EN() 	(RCC->AHBENR |= (1 << 17))		// Set bit 17 to 1 to enable clock for GPIOA
#define GPIOB_PCLK_EN() 	(RCC->AHBENR |= (1 << 18))
#define GPIOC_PCLK_EN() 	(RCC->AHBENR |= (1 << 19))
#define GPIOD_PCLK_EN() 	(RCC->AHBENR |= (1 << 20))
#define GPIOF_PCLK_EN() 	(RCC->AHBENR |= (1 << 22))

/*
 * Clock disable Macros for GPIOx
 * */

#define GPIOA_PCLK_DI() 	(RCC->AHBENR &= ~(1 << 17))		// Clear bit 17; Ensure its 0 to disable clock for GPIOA
#define GPIOB_PCLK_DI() 	(RCC->AHBENR &= ~(1 << 18))
#define GPIOC_PCLK_DI() 	(RCC->AHBENR &= ~(1 << 19))
#define GPIOD_PCLK_DI() 	(RCC->AHBENR &= ~(1 << 20))
#define GPIOF_PCLK_DI() 	(RCC->AHBENR &= ~(1 << 22))

/* Clock enable macro for syscfg */
#define SYSCFG_PCLK_EN()	(RCC->APB2ENR |= (1 << 0))

/*Clock enable/disable macros for TIM2 and TIM3*/
#define TIM2_PCLK_EN()		(RCC->APB1ENR |= (1 << 0))
#define TIM3_PCLK_EN()		(RCC->APB1ENR |= (1 << 1))

#define TIM2_PCLK_DI()		(RCC->APB1ENR &= ~(1 << 0))
#define TIM3_PCLK_DI()		(RCC->APB1ENR &= ~(1 << 1))


/*Clock enable/disable macros for TIM2 and TIM3*/
#define I2C1_PCLK_EN()		(RCC->APB1ENR |= (1 << 21))	// VERIFY !!!!!!!!!!!!!!
#define I2C1_PCLK_DI()		(RCC->APB1ENR &= ~(1 << 21))	// VERIFY !!!!!!!!!!!!!!
/* *********** (7) END OF CLOCK ENABLE/DISABLE MACROS *********** */



/* *********** (8) BEGGINING OF RESET MACROS *********** */
// NEED TO UNDERSTAND THIS CODE AGAIN
#define GPIOA_REG_RESET()		do {(RCC->AHBRSTR |= (1 << 17));  (RCC->AHBRSTR &= ~(1 << 17)); }while(0)	// This is a technique to execute multiple C statements in one C-macro (NEED TO ADD TO MACRO NOTES)
#define GPIOB_REG_RESET()		do {(RCC->AHBRSTR |= (1 << 18));  (RCC->AHBRSTR &= ~(1 << 18)); }while(0)
#define GPIOC_REG_RESET()		do {(RCC->AHBRSTR |= (1 << 19));  (RCC->AHBRSTR &= ~(1 << 19)); }while(0)
#define GPIOD_REG_RESET()		do {(RCC->AHBRSTR |= (1 << 20));  (RCC->AHBRSTR &= ~(1 << 20)); }while(0)
#define GPIOF_REG_RESET()		do {(RCC->AHBRSTR |= (1 << 22));  (RCC->AHBRSTR &= ~(1 << 22)); }while(0)
/* *********** (8) END OF RESET MACROS *********** */


/* *********** (9) BEGGINING OF RANDOM MACROS *********** */
#define GPIO_BASEADDR_TO_CODE(x)			((x == GPIOA) ? 0:\
											(x == GPIOB) ? 1:\
											(x == GPIOC) ? 2:\
											(x == GPIOD) ? 3:\
											(x == GPIOC) ? 4:0)
/* *********** (9) END OF RANDOM MACROS *********** */



#endif /* INC_STM32F334R8_H_ */
