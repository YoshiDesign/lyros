/****************************************
* License information


****************************************/

#include "lyros.h"
#include "qassert.h"
#include "bsp.h"
#include <stdint.h>

// Required for Q_ASSERT macro
Q_DEFINE_THIS_FILE
/**
 * Q_ASSERT - Assert Function
 * Q_DIM - Function macro to get array length
 */
// Volatile pointers - Current OS thread and next OS thread for scheduling
OSThread *volatile OS_curr;
OSThread *volatile OS_next;
OSThread *OS_threads[32 + 1 ];    /* Array of working threads */
uint8_t OS_threadNum;           /* Total number of working thread */
uint8_t OS_currIndex;           /* Current working thread index */


/**
* REMEMBER: Some of the hardest parts about an RTOS kernel are
* race conditions due to scheduling.
*
* Also, you should never preempt an interrupt with another interrupt!
*/

// Initialize our RTOS
void OS_init(void) {
    // Set PendSV interrupt priority to lowest level. 0xFF seems verbose
    // but it works because memory around this location is read-only
    // and we want PendSV to have the highest value (lowest priority)
    *(uint32_t volatile *)0xE000ED20 |= (0xFFU << 16);
}

// Thread scheduler - CRITICAL SECTION - always wrap this f'n in a critical section
void OS_sched(void) {

    // Warning, you're starting this program on the thread at index[1], not 0
    ++OS_currIndex;

    // Return to our first index
    if (OS_currIndex == OS_threadNum) {
        OS_currIndex = 0U;
    }

    // 
    OS_next = OS_threads[OS_currIndex];

    if (OS_next != OS_curr) {
        // Sets the PendSV bit
        *(uint32_t volatile*)0xE000ED04 = (1U << 28);
    }

}

// Our main OS execution context loop
void OS_run(void) {
    
    // Callback to enable and configure interrupts (Part of BSP)
    OS_onStartup();
    
    // Call the OS Scheduler.
    __disable_irq();
    OS_sched();
    __enable_irq();
    // PendSV will be triggered right here, so control will actually never
    // return to OS_run()
    // Consequently, any code after this point should never execute
    
    // All that being said, if this runs, you have an error somewhere and are dropping threads.
    Q_ERROR();
    
    // REMEMBER: Context switching can only occur after an interrupt. BC the stack layout
    // assumes the stack has been switched as a return from an exception.
    // This is why it's ok to call OS_sched here, even though it is also used by SysTick,
    // because it triggers an interrupt (PendSV) which performs context switching for threads.
    
}

void OSThread_start(
    OSThread *me,
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize
){

    /**
    * Starting from the end of stack memory, aligned at the 8-byte boundary.
    * Dividing our location by 8 and then subsequently mult by 8 is a way of 
    * rounding down to the closest address.
    * THIS IS CORTEX-M SPECIFIC
    */

    // Beginning of stack (Remembering it grows to lower addresses)
    uint32_t *sp = (uint32_t *)((((uint32_t)stkSto + stkSize) / 8) *8);
    // End of stack - Last valid stack address
    uint32_t *stk_limit;

    // Pre-fill each fabricated stack with a Cortex ISR stack frame
    // THE ALIGNER is not necessary to account for because we 8-byte aligned from the beginning- i.e. [40]
    *(--sp) = (1U << 24);    /* [xPSR] - Setting the 24th bit for historical purposes [THUMB] */
    *(--sp) = (uint32_t)threadHandler;    // [PC] Pointer to this thread's executing function
    *(--sp) = 0x0000000EU;   /* LR */
    *(--sp) = 0x0000000CU;   /* R12 */
    *(--sp) = 0x00000003U;   /* R3 */
    *(--sp) = 0x00000002U;   /* R2 */
    *(--sp) = 0x00000001U;   /* R1 */
    *(--sp) = 0x00000000U;   /* R0 */
    /* Include fake registers [R11 -> R4], unused by the Exception Stack Frame */
    *(--sp) = 0x0000000BU;
    *(--sp) = 0x0000000AU;
    *(--sp) = 0x00000009U;
    *(--sp) = 0x00000008U;
    *(--sp) = 0x00000007U;
    *(--sp) = 0x00000006U;
    *(--sp) = 0x00000005U;
    *(--sp) = 0x00000004U;

    /* save the top of the stack in the thread's attribute */
    me->sp = sp;

    /* Round up the bottom of the stack to the 8byte boundary */
    stk_limit = (uint32_t *)(((((uint32_t)stkSto - 1U) / 8) + 1U ) * 8);

    // Prefill stack so we know what we're looking at
    for(sp = sp - 1U; sp>=stk_limit; --sp) {
        *sp = 0xDEADBEEF;
    }

    /* BSP Q_onAssert - Monitoring the thread count */
    Q_ASSERT(OS_threadNum < Q_DIM(OS_threads));

    /* Register the thread with the OS */
    OS_threads[OS_threadNum] = me;
    ++OS_threadNum;

}

// Compiler specific attribute. This function is our context switch after interrupts
__asm void PendSV_Handler(){
    
    /** Original Code - Used to copy disassembly **/
    
    /**
    // Temporary allocator to be replaced by the real sp
    void *sp;
    
    __disable_irq();
    if (OS_curr != (OSThread *)0) {
        // Push registers r4-r11 onto stack
        OS_curr->sp = sp;
    }
    
    // Restore the context
    sp = OS_next->sp;
    OS_curr = OS_next;
    
    // pop registers r4-r11
    __enable_irq();
    
    */
    /** End Original Code **/
    
    IMPORT OS_curr;
    IMPORT OS_next;
    
    /* __disable_irq(); */
    CPSID         I
    
    /*     if (OS_curr != (OSThread *)0) {  */
    LDR           r1,=OS_curr          // R1 gets address of OS_curr
    LDR           r1,[r1,#0x00]        // R1 gets dereferenced OS_curr
    CBZ           r1,PendSV_restore    // Conditional R1 == 0 then GOTO PendSV_restore label
    
    /*     Else: Push registers r4-r11 onto stack */
    PUSH          {r4-r11}             // ARM specific instruction
    LDR           r1,=OS_curr           // @0x000005E8
    LDR           r1,[r1,#0x00]         
    
    /*     OS_curr->sp = sp;       */
    STR           sp,[r1,#0x00]
    /* }                           */
    
PendSV_restore
    /*     sp = OS_next->sp;       */
    LDR           r1,=OS_next  
    LDR           r1,[r1,#0x00]
    LDR           sp,[r1,#0x00]     // REMEMBER: sp is loaded from OS_next
    
    /*     OS_curr = OS_next;      */
    LDR           r1,=OS_next
    LDR           r1,[r1,#0x00]
    LDR           r2,=OS_curr  
    STR           r1,[r2,#0x00]
    
    /*     // pop registers r4-r11 */
    POP          {r4-r11}
    
    /*     __enable_irq();         */
    CPSIE         I
    
    /* Return to the next thread   */
    BX            lr
    
    // Return to the next thread, happily
}
