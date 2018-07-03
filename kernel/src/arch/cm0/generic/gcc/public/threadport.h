/*===========================================================================
     _____        _____        _____        _____
 ___|    _|__  __|_    |__  __|__   |__  __| __  |__  ______
|    \  /  | ||    \      ||     |     ||  |/ /     ||___   |
|     \/   | ||     \     ||     \     ||     \     ||___   |
|__/\__/|__|_||__|\__\  __||__|\__\  __||__|\__\  __||______|
    |_____|      |_____|      |_____|      |_____|

--[Mark3 Realtime Platform]--------------------------------------------------

Copyright (c) 2012 - 2017 Funkenstein Software Consulting, all rights reserved.
See license.txt for more information
=========================================================================== */
/*!

    \file   threadport.h

    \brief  Cortex M-0 Multithreading support.
 */
#pragma once

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
#ifndef xDMB
    #define xDMB()                    ASM(" dmb \n");
#endif
#ifndef xdisable_irq
    #define xdisable_irq()            ASM(" cpsid i \n");
#endif
#ifndef xenable_irq
    #define xenable_irq()            ASM(" cpsie i \n");
#endif

#define ENABLE_INTS()        { xDMB(); xenable_irq(); }
#define DISABLE_INTS()        { xdisable_irq(); xDMB(); }

//------------------------------------------------------------------------
//! Enter critical section (copy current PRIMASK register value, disable interrupts)
#define CS_ENTER()    \
{ \
    DISABLE_INTS(); \
    Mark3::g_ulCriticalCount++;\
}
//------------------------------------------------------------------------
//! Exit critical section (restore previous PRIMASK status register value)
#define CS_EXIT() \
{ \
    Mark3::g_ulCriticalCount--; \
    if( 0 == Mark3::g_ulCriticalCount ) { \
        ENABLE_INTS(); \
    } \
}

namespace Mark3 {
//------------------------------------------------------------------------
extern volatile uint32_t g_ulCriticalCount;

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
} // namespace Mark3