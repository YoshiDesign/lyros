/* Board Support Package (BSP) for the EK-TM4C123GXL board */
#include <stdint.h>  /* Standard integers. WG14/N843 C99 Standard */
#include "skios.h"
#include "bsp.h"
#include "TM4C123GH6PM.h" /* the TM4C MCU Peripheral Access Layer (TI) */

/* on-board LEDs */
#define LED_RED   (1U << 1)
#define LED_BLUE  (1U << 2)
#define LED_GREEN (1U << 3)

// l_tickCtr is our means of communication between our foreground and background - it's a critical section
/**
* To avoid race conditions due to the preemption of the background loop
	by the foreground, these kinds of variables must be volatile, and also
	have interrupts disabled upon any access to them from the background, re-enabling them when
	they're good to go.
*/
static uint32_t volatile l_tickCtr;

/**
 * Any routines with time constraints on them can only 
 * execute reliably in the foreground. This, however can cause
 * hangups in the background, so use them sparingly
 */

void SysTick_Handler(void) {
    ++l_tickCtr;
    
    __disable_irq();
    OS_sched();
    __enable_irq();
}

void BSP_init(void) {
    SYSCTL->RCGCGPIO  |= (1U << 5); /* enable Run mode for GPIOF */
    SYSCTL->GPIOHBCTL |= (1U << 5); /* enable AHB for GPIOF */
    GPIOF_AHB->DIR |= (LED_RED | LED_BLUE | LED_GREEN);
    GPIOF_AHB->DEN |= (LED_RED | LED_BLUE | LED_GREEN);   
}

uint32_t BSP_tickCtr(void) {
    uint32_t tickCtr;

    __disable_irq();
    tickCtr = l_tickCtr;
    __enable_irq();

    return tickCtr;
}

void BSP_delay(uint32_t ticks) {
    uint32_t start = BSP_tickCtr();
    while ((BSP_tickCtr() - start) < ticks) {
    }
}

void BSP_ledRedOn(void) {
    GPIOF_AHB->DATA_Bits[LED_RED] = LED_RED;
}

void BSP_ledRedOff(void) {
    GPIOF_AHB->DATA_Bits[LED_RED] = 0U;
}

void BSP_ledBlueOn(void) {
    GPIOF_AHB->DATA_Bits[LED_BLUE] = LED_BLUE;
}

void BSP_ledBlueOff(void) {
    GPIOF_AHB->DATA_Bits[LED_BLUE] = 0U;
}

void BSP_ledGreenOn(void) {
    GPIOF_AHB->DATA_Bits[LED_GREEN] = LED_GREEN;
}

void BSP_ledGreenOff(void) {
    GPIOF_AHB->DATA_Bits[LED_GREEN] = 0U;
}

// @function CRITICAL SECTION
void OS_onStartup(void){
    
    // The core clock frequency
    SystemCoreClockUpdate();
    
    // Configure the SysTick's frequency
    SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);
    
    // Set the SysTick Interrupt to highest priority - CMSIS function
    NVIC_SetPriority(SysTick_IRQn, 0U);

}

void Q_onAssert(char const *module, int loc) {
    /* TBD: damage control */
    (void)module; /* avoid the "unused parameter" compiler warning */
    (void)loc;    /* avoid the "unused parameter" compiler warning */
    NVIC_SystemReset();
}
