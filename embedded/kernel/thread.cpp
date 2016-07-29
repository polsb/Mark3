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
===========================================================================*/
/*!

    \file   thread.cpp

    \brief  Platform-Independent thread class Definition

*/

#include "kerneltypes.h"
#include "mark3cfg.h"

#include "thread.h"
#include "scheduler.h"
#include "kernelswi.h"
#include "timerlist.h"
#include "ksemaphore.h"
#include "quantum.h"
#include "kernel.h"
#include "priomap.h"

#define _CAN_HAS_DEBUG
//--[Autogenerated - Do Not Modify]------------------------------------------
#include "dbg_file_list.h"
#include "buffalogger.h"
#if defined(DBG_FILE)
#error "Debug logging file token already defined!  Bailing."
#else
#define DBG_FILE _DBG___KERNEL_THREAD_CPP
#endif
//--[End Autogenerated content]----------------------------------------------

#include "kerneldebug.h"
//---------------------------------------------------------------------------
Thread::~Thread()
{
    // On destruction of a thread located on a stack,
    // ensure that the thread is either stopped, or exited.
    // If the thread is stopped, move it to the exit state.
    // If not in the exit state, kernel panic -- it's catastrophic to have
    // running threads on stack suddenly disappear.
    if (m_eState == THREAD_STATE_STOP) {
        CS_ENTER();
        m_pclCurrent->Remove(this);
        m_pclCurrent = 0;
        m_pclOwner   = 0;
        m_eState     = THREAD_STATE_EXIT;
        CS_EXIT();
    } else if (m_eState != THREAD_STATE_EXIT) {
#if KERNEL_AWARE_SIMULATION
        KernelAware::Trace(0, 0, m_u8ThreadID, m_eState);
#endif
        Kernel::Panic(PANIC_RUNNING_THREAD_DESCOPED);
    }
}

//---------------------------------------------------------------------------
void Thread::Init(
    K_WORD* pwStack_, uint16_t u16StackSize_, PRIO_TYPE uXPriority_, ThreadEntry_t pfEntryPoint_, void* pvArg_)
{
    static uint8_t u8ThreadID = 0;

    KERNEL_ASSERT(pwStack_);
    KERNEL_ASSERT(pfEntryPoint_);

    ClearNode();

    m_u8ThreadID = u8ThreadID++;
#if KERNEL_USE_IDLE_FUNC
    if (u8ThreadID == 255) {
        u8ThreadID = 0;
    }
#endif

    KERNEL_TRACE_1("Stack Size: %d", u16StackSize_);
    KERNEL_TRACE_1("Thread Pri: %d", (uint8_t)uXPriority_);
    KERNEL_TRACE_1("Thread Id: %d", (uint16_t)m_u8ThreadID);
    KERNEL_TRACE_1("Entrypoint: %x", (uint16_t)pfEntryPoint_);

    // Initialize the thread parameters to their initial values.
    m_pwStack    = pwStack_;
    m_pwStackTop = TOP_OF_STACK(pwStack_, u16StackSize_);

    m_u16StackSize = u16StackSize_;

#if KERNEL_USE_QUANTUM
    m_u16Quantum = THREAD_QUANTUM_DEFAULT;
#endif

    m_uXPriority    = uXPriority_;
    m_uXCurPriority = m_uXPriority;
    m_pfEntryPoint  = pfEntryPoint_;
    m_pvArg         = pvArg_;
    m_eState        = THREAD_STATE_STOP;

#if KERNEL_USE_THREADNAME
    m_szName = NULL;
#endif
#if KERNEL_USE_TIMERS
    m_clTimer.Init();
#endif

    // Call CPU-specific stack initialization
    ThreadPort::InitStack(this);

    // Add to the global "stop" list.
    CS_ENTER();
    m_pclOwner   = Scheduler::GetThreadList(m_uXPriority);
    m_pclCurrent = Scheduler::GetStopList();
    m_pclCurrent->Add(this);
    CS_EXIT();

#if KERNEL_USE_THREAD_CALLOUTS
    ThreadCreateCallout_t pfCallout = Kernel::GetThreadCreateCallout();
    if (pfCallout) {
        pfCallout(this);
    }
#endif
}

#if KERNEL_USE_AUTO_ALLOC
//---------------------------------------------------------------------------
Thread* Thread::Init(uint16_t u16StackSize_, PRIO_TYPE uXPriority_, ThreadEntry_t pfEntryPoint_, void* pvArg_)
{
    Thread* pclNew  = (Thread*)AutoAlloc::Allocate(sizeof(Thread));
    K_WORD* pwStack = (K_WORD*)AutoAlloc::Allocate(u16StackSize_);
    pclNew->Init(pwStack, u16StackSize_, uXPriority_, pfEntryPoint_, pvArg_);
    return pclNew;
}
#endif

//---------------------------------------------------------------------------
void Thread::Start(void)
{
    // Remove the thread from the scheduler's "stopped" list, and add it
    // to the scheduler's ready list at the proper priority.
    KERNEL_TRACE_1("Starting Thread %d", (uint16_t)m_u8ThreadID);

    CS_ENTER();
    Scheduler::GetStopList()->Remove(this);
    Scheduler::Add(this);
    m_pclOwner   = Scheduler::GetThreadList(m_uXPriority);
    m_pclCurrent = m_pclOwner;
    m_eState     = THREAD_STATE_READY;

#if KERNEL_USE_QUANTUM
    if (Kernel::IsStarted()) {
        if (GetCurPriority() >= Scheduler::GetCurrentThread()->GetCurPriority()) {
            // Deal with the thread Quantum
            Quantum::RemoveThread();
            Quantum::AddThread(this);
        }
    }
#endif

    if (Kernel::IsStarted()) {
        if (GetCurPriority() >= Scheduler::GetCurrentThread()->GetCurPriority()) {
            Thread::Yield();
        }
    }
    CS_EXIT();
}

//---------------------------------------------------------------------------
void Thread::Stop()
{
    bool bReschedule = 0;

    CS_ENTER();

    // If a thread is attempting to stop itself, ensure we call the scheduler
    if (this == Scheduler::GetCurrentThread()) {
        bReschedule = true;
    }

    // Add this thread to the stop-list (removing it from active scheduling)
    // Remove the thread from scheduling
    if (m_eState == THREAD_STATE_READY) {
        Scheduler::Remove(this);
    } else if (m_eState == THREAD_STATE_BLOCKED) {
        m_pclCurrent->Remove(this);
    }

    m_pclOwner   = Scheduler::GetStopList();
    m_pclCurrent = m_pclOwner;
    m_pclOwner->Add(this);
    m_eState = THREAD_STATE_STOP;

#if KERNEL_USE_TIMERS
    // Just to be safe - attempt to remove the thread's timer
    // from the timer-scheduler (does no harm if it isn't
    // in the timer-list)
    TimerScheduler::Remove(&m_clTimer);
#endif

    CS_EXIT();

    if (bReschedule) {
        Thread::Yield();
    }
}

#if KERNEL_USE_DYNAMIC_THREADS
//---------------------------------------------------------------------------
void Thread::Exit()
{
    bool bReschedule = 0;

    KERNEL_TRACE_1("Exit Thread %d", m_u8ThreadID);

#if KERNEL_USE_THREAD_CALLOUTS
    ThreadExitCallout_t pfCallout = Kernel::GetThreadExitCallout();
    if (pfCallout) {
        pfCallout(this);
    }
#endif

    CS_ENTER();

    // If this thread is the actively-running thread, make sure we run the
    // scheduler again.
    if (this == Scheduler::GetCurrentThread()) {
        bReschedule = 1;
    }

    // Remove the thread from scheduling
    if (m_eState == THREAD_STATE_READY) {
        Scheduler::Remove(this);
    } else if ((m_eState == THREAD_STATE_BLOCKED) || (m_eState == THREAD_STATE_STOP)) {
        m_pclCurrent->Remove(this);
    }

    m_pclCurrent = 0;
    m_pclOwner   = 0;
    m_eState     = THREAD_STATE_EXIT;

    // We've removed the thread from scheduling, but interrupts might
    // trigger checks against this thread's currently priority before
    // we get around to scheduling new threads.  As a result, set the
    // priority to idle to ensure that we always wind up scheduling
    // new threads.
    m_uXCurPriority = 0;
    m_uXPriority    = 0;

#if KERNEL_USE_TIMERS
    // Just to be safe - attempt to remove the thread's timer
    // from the timer-scheduler (does no harm if it isn't
    // in the timer-list)
    TimerScheduler::Remove(&m_clTimer);
#endif

    CS_EXIT();

    if (bReschedule) {
        // Choose a new "next" thread if we must
        Thread::Yield();
    }
}
#endif

#if KERNEL_USE_SLEEP
//---------------------------------------------------------------------------
//! This callback is used to wake up a thread once the interval has expired
static void ThreadSleepCallback(Thread* pclOwner_, void* pvData_)
{
    Semaphore* pclSemaphore = static_cast<Semaphore*>(pvData_);
    // Post the semaphore, which will wake the sleeping thread.
    pclSemaphore->Post();
}

//---------------------------------------------------------------------------
void Thread::Sleep(uint32_t u32TimeMs_)
{
    Semaphore clSemaphore;
    Timer*    pclTimer = g_pclCurrent->GetTimer();

    // Create a semaphore that this thread will block on
    clSemaphore.Init(0, 1);

    // Create a one-shot timer that will call a callback that posts the
    // semaphore, waking our thread.
    pclTimer->Init();
    pclTimer->SetIntervalMSeconds(u32TimeMs_);
    pclTimer->SetCallback(ThreadSleepCallback);
    pclTimer->SetData((void*)&clSemaphore);
    pclTimer->SetFlags(TIMERLIST_FLAG_ONE_SHOT);

    // Add the new timer to the timer scheduler, and block the thread
    TimerScheduler::Add(pclTimer);
    clSemaphore.Pend();
}

//---------------------------------------------------------------------------
void Thread::USleep(uint32_t u32TimeUs_)
{
    Semaphore clSemaphore;
    Timer*    pclTimer = g_pclCurrent->GetTimer();

    // Create a semaphore that this thread will block on
    clSemaphore.Init(0, 1);

    // Create a one-shot timer that will call a callback that posts the
    // semaphore, waking our thread.
    pclTimer->Init();
    pclTimer->SetIntervalUSeconds(u32TimeUs_);
    pclTimer->SetCallback(ThreadSleepCallback);
    pclTimer->SetData((void*)&clSemaphore);
    pclTimer->SetFlags(TIMERLIST_FLAG_ONE_SHOT);

    // Add the new timer to the timer scheduler, and block the thread
    TimerScheduler::Add(pclTimer);
    clSemaphore.Pend();
}
#endif // KERNEL_USE_SLEEP

//---------------------------------------------------------------------------
uint16_t Thread::GetStackSlack()
{
    K_ADDR wTop    = (K_ADDR)m_u16StackSize - 1;
    K_ADDR wBottom = (K_ADDR)0;
    K_ADDR wMid    = ((wTop + wBottom) + 1) / 2;

    CS_ENTER();

    // Logarithmic bisection - find the point where the contents of the
    // stack go from 0xFF's to non 0xFF.  Not Definitive, but accurate enough
    while ((wTop - wBottom) > 1) {
#if STACK_GROWS_DOWN
        if (m_pwStack[wMid] != (K_WORD)(-1))
#else
        if (m_pwStack[wMid] == (K_WORD)(-1))
#endif
        {
            //! ToDo : Reverse the logic for MCUs where stack grows UP instead of down
            wTop = wMid;
        } else {
            wBottom = wMid;
        }
        wMid = (wTop + wBottom + 1) / 2;
    }

    CS_EXIT();

    return wMid;
}

//---------------------------------------------------------------------------
void Thread::Yield()
{
    CS_ENTER();
    // Run the scheduler
    if (Scheduler::IsEnabled()) {
        Scheduler::Schedule();

        // Only switch contexts if the new task is different than the old task
        if (Scheduler::GetCurrentThread() != Scheduler::GetNextThread()) {
#if KERNEL_USE_QUANTUM
            // new thread scheduled.  Stop current quantum timer (if it exists),
            // and restart it for the new thread (if required).
            Quantum::RemoveThread();
            Quantum::AddThread((Thread*)g_pclNext);
#endif
            Thread::ContextSwitchSWI();
        }
    } else {
        Scheduler::QueueScheduler();
    }

    CS_EXIT();
}

//---------------------------------------------------------------------------
void Thread::SetPriorityBase(PRIO_TYPE uXPriority_)
{
    GetCurrent()->Remove(this);

    SetCurrent(Scheduler::GetThreadList(m_uXPriority));

    GetCurrent()->Add(this);
}

//---------------------------------------------------------------------------
void Thread::SetPriority(PRIO_TYPE uXPriority_)
{
    bool bSchedule = 0;

    CS_ENTER();
    // If this is the currently running thread, it's a good idea to reschedule
    // Or, if the new priority is a higher priority than the current thread's.
    if ((g_pclCurrent == this) || (uXPriority_ > g_pclCurrent->GetPriority())) {
        bSchedule = 1;
    }
    Scheduler::Remove(this);
    CS_EXIT();

    m_uXCurPriority = uXPriority_;
    m_uXPriority    = uXPriority_;

    CS_ENTER();
    Scheduler::Add(this);
    CS_EXIT();

    if (bSchedule) {
        if (Scheduler::IsEnabled()) {
            CS_ENTER();
            Scheduler::Schedule();
#if KERNEL_USE_QUANTUM
            // new thread scheduled.  Stop current quantum timer (if it exists),
            // and restart it for the new thread (if required).
            Quantum::RemoveThread();
            Quantum::AddThread((Thread*)g_pclNext);
#endif
            CS_EXIT();
            Thread::ContextSwitchSWI();
        } else {
            Scheduler::QueueScheduler();
        }
    }
}

//---------------------------------------------------------------------------
void Thread::InheritPriority(PRIO_TYPE uXPriority_)
{
    SetOwner(Scheduler::GetThreadList(uXPriority_));
    m_uXCurPriority = uXPriority_;
}

//---------------------------------------------------------------------------
void Thread::ContextSwitchSWI()
{
    // Call the context switch interrupt if the scheduler is enabled.
    if (Scheduler::IsEnabled() == 1) {
        KERNEL_TRACE_1("Context switch to Thread %d", (uint16_t)((Thread*)g_pclNext)->GetID());
#if KERNEL_USE_STACK_GUARD
        uint16_t u16Slack;
#if KERNEL_USE_IDLE_FUNC
        if (g_pclCurrent->GetID() != 255) {
#endif
            if (g_pclCurrent->GetStackSlack() <= Kernel::GetStackGuardThreshold()) {
                KernelAware::Trace(DBG_FILE, __LINE__, g_pclCurrent->GetID(), g_pclCurrent->GetStackSlack());
                Kernel::Panic(PANIC_STACK_SLACK_VIOLATED);
            }
#if KERNEL_USE_IDLE_FUNC
        }
#endif
#endif

#if KERNEL_USE_THREAD_CALLOUTS
        ThreadContextCallout_t pfCallout = Kernel::GetThreadContextSwitchCallout();
        if (pfCallout) {
            pfCallout(g_pclCurrent);
        }
#endif
        KernelSWI::Trigger();
    }
}

#if KERNEL_USE_TIMEOUTS || KERNEL_USE_SLEEP
//---------------------------------------------------------------------------
Timer* Thread::GetTimer()
{
    return &m_clTimer;
}
#endif
#if KERNEL_USE_TIMEOUTS
//---------------------------------------------------------------------------
void Thread::SetExpired(bool bExpired_)
{
    m_bExpired = bExpired_;
}

//---------------------------------------------------------------------------
bool Thread::GetExpired()
{
    return m_bExpired;
}
#endif

#if KERNEL_USE_IDLE_FUNC
//---------------------------------------------------------------------------
void Thread::InitIdle(void)
{
    ClearNode();

    m_uXPriority    = 0;
    m_uXCurPriority = 0;
    m_pfEntryPoint  = 0;
    m_pvArg         = 0;
    m_u8ThreadID    = 255;
    m_eState        = THREAD_STATE_READY;
#if KERNEL_USE_THREADNAME
    m_szName = "IDLE";
#endif
}
#endif
