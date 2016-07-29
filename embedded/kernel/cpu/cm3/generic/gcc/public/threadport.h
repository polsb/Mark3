/*===========================================================================
     _____        _____        _____        _____
 ___|    _|__  __|_    |__  __|__   |__  __| __  |__  ______
|    \  /  | ||    \      ||     |     ||  |/ /     ||___   |
|     \/   | ||     \     ||     \     ||     \     ||___   |
|__/\__/|__|_||__|\__\  __||__|\__\  __||__|\__\  __||______|
    |_____|      |_____|      |_____|      |_____|

--[Mark3 Realtime Platform]--------------------------------------------------

Copyright (c) 2012-2016 Funkenstein Software Consulting, all rights reserved.
See license.txt for more information
=========================================================================== */
/*!

    \file   threadport.h

    \brief  Cortex M3 Multithreading support.
 */

#ifndef __THREADPORT_H_
#define __THREADPORT_H_

#include "kerneltypes.h"
#include "thread.h"

// clang-format off
//---------------------------------------------------------------------------
//! ASM Macro - simplify the use of ASM directive in C
#define ASM      asm volatile

//---------------------------------------------------------------------------
//! Macro to find the top of a stack given its size and top address
#define TOP_OF_STACK(x, y)        (K_WORD*) ( ((uint32_t)x) + (y - sizeof(K_WORD)) )
//! Push a value y to the stack pointer x and decrement the stack pointer
#define PUSH_TO_STACK(x, y)        *x = y; x--;
#define STACK_GROWS_DOWN           (1)

//------------------------------------------------------------------------
//! These macros *must* be used in matched-pairs !
//! Nesting *is* supported !

//------------------------------------------------------------------------
#ifndef xDMB
    #define xDMB()                    ASM(" dmb \n");
#endif
#ifndef xdisable_irq
    #define xdisable_irq()            ASM(" cpsid i \n");
#endif
#ifndef xenable_irq
    #define xenable_irq()             ASM(" cpsie i \n");
#endif

//------------------------------------------------------------------------
//! Enter critical section (copy current PRIMASK register value, disable interrupts)
#define CS_ENTER()                      \
do {                                    \
    volatile uint8_t __sr;              \
    ASM (                               \
    " mrs   r0, PRIMASK\n "             \
    " cpsid i \n"                       \
    " strb r0, %[output] \n"            \
    : [output] "=m" (__sr) :: "r0");

//------------------------------------------------------------------------
//! Exit critical section (restore previous PRIMASK status register value)
#define CS_EXIT()                       \
    ASM (                               \
    " ldrb r0, %[input]\n "             \
    " msr PRIMASK, r0 \n "              \
    ::[input] "m" (__sr) : "r0");       \
} while(0);

//------------------------------------------------------------------------
class Thread;
/*!
 *  Class defining the architecture specific functions required by the 
 *  kernel.  
 *  
 *  This is limited (at this point) to a function to start the scheduler,
 *  and a function to initialize the default stack-frame for a thread.
 */
class ThreadPort
{
public:
    /*!        
     *  \brief StartThreads
     *
     *  Function to start the scheduler, initial threads, etc.
     */
    static void StartThreads();
    friend class Thread;
private:

    /*!
     *  \brief InitStack
     *
     *  Initialize the thread's stack.
     *  
     *  \param pstThread_ Pointer to the thread to initialize
     */
    static void InitStack(Thread *pstThread_);
};

#endif //__ThreadPORT_H_
