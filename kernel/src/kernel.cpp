/*===========================================================================
     _____        _____        _____        _____
 ___|    _|__  __|_    |__  __|__   |__  __| __  |__  ______
|    \  /  | ||    \      ||     |     ||  |/ /     ||___   |
|     \/   | ||     \     ||     \     ||     \     ||___   |
|__/\__/|__|_||__|\__\  __||__|\__\  __||__|\__\  __||______|
    |_____|      |_____|      |_____|      |_____|

--[Mark3 Realtime Platform]--------------------------------------------------

Copyright (c) 2012 - 2018 m0slevin, all rights reserved.
See license.txt for more information
===========================================================================*/
/*!

    \file   kernel.cpp

    \brief  Kernel initialization and startup code
*/

#include "kerneltypes.h"
#include "mark3cfg.h"

#include "kernel.h"
#include "scheduler.h"
#include "thread.h"
#include "threadport.h"
#include "timerlist.h"
#include "message.h"
#include "profile.h"
#include "kernelprofile.h"
#include "autoalloc.h"

#define _HAS_DEBUG_TOKEN
//--[Autogenerated - Do Not Modify]------------------------------------------
#include "dbg_file_list.h"
#include "buffalogger.h"
#if defined(DBG_FILE)
#error "Debug logging file token already defined!  Bailing."
#else
#define DBG_FILE _DBG___KERNEL_KERNEL_CPP
#endif
//--[End Autogenerated content]----------------------------------------------
#include "kerneldebug.h"
#include "tracebuffer.h"

extern "C" {
 int __cxa_guard_acquire(long long int* wha) { return 0; }
 void __cxa_guard_release(long long int* wha) {}
 void atexit(void) {}
}

namespace Mark3
{

bool Kernel::m_bIsStarted; //!< true if kernel is running, false otherwise
bool Kernel::m_bIsPanic;   //!< true if kernel is in panic state, false otherwise
PanicFunc Kernel::m_pfPanic;    //!< set panic function
#if KERNEL_USE_IDLE_FUNC
IdleFunc Kernel::m_pfIdle; //!< set idle function
FakeThread_t Kernel::m_clIdle; //!< Idle thread object (note: not a real thread)
#endif

#if KERNEL_USE_THREAD_CALLOUTS
ThreadCreateCallout  Kernel::m_pfThreadCreateCallout;  //!< Function to call on thread creation
ThreadExitCallout    Kernel::m_pfThreadExitCallout;    //!< Function to call on thread exit
ThreadContextCallout Kernel::m_pfThreadContextCallout; //!< Function to call on context switch
#endif

#if KERNEL_USE_STACK_GUARD
uint16_t Kernel::m_u16GuardThreshold;
#endif

//---------------------------------------------------------------------------
void Kernel::Init(void)
{
#if KERNEL_USE_AUTO_ALLOC
    AutoAlloc::Init();
#endif
#if KERNEL_USE_IDLE_FUNC
    (reinterpret_cast<Thread*>(&m_clIdle))->InitIdle();
#endif
#if KERNEL_USE_DEBUG && !KERNEL_AWARE_SIMULATION
    TraceBuffer::Init();
#endif
    KERNEL_TRACE("Initializing Mark3 Kernel");

    // Initialize the global kernel data - scheduler, timer-scheduler, and
    // the global message pool.
    Scheduler::Init();
#if KERNEL_USE_TIMERS
    TimerScheduler::Init();
#endif
#if KERNEL_USE_STACK_GUARD
    m_u16GuardThreshold = KERNEL_STACK_GUARD_DEFAULT;
#endif
}

//---------------------------------------------------------------------------
void Kernel::Start(void)
{
    KERNEL_TRACE("Starting Mark3 Scheduler");
    m_bIsStarted = true;
    ThreadPort::StartThreads();
    KERNEL_TRACE("Error starting Mark3 Scheduler");
}

//---------------------------------------------------------------------------
void Kernel::Panic(uint16_t u16Cause_)
{
    m_bIsPanic = true;
    if (m_pfPanic != nullptr) {
        m_pfPanic(u16Cause_);
    } else {
#if KERNEL_AWARE_SIMULATION
        KernelAware::Print("Panic\n");
        KernelAware::Trace(0, 0, u16Cause_, g_pclCurrent->GetID());
        KernelAware::ExitSimulator();
#endif
        while (true) { }
    }
}
} //namespace Mark3

