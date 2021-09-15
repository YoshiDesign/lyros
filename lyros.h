/****************************************
* License information


****************************************/

#ifndef SKIOS_H
#define SKIOS_H
#include <stdint.h>

typedef void (*OSThreadHandler)();
void OS_run(void);

/* Thread Control Block (TCB) */
typedef struct {
    
    void *sp;   // Stack Pointer
    
} OSThread;

void OS_init(void);
void OS_sched(void);

void OSThread_start(
    OSThread *me,
    OSThreadHandler threadHandler,
    void *stkSto, uint32_t stkSize
);
#endif
    