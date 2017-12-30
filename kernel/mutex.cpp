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
===========================================================================*/
/*!
    \file mutex.cpp

    \brief Mutual-exclusion object
*/

#include "kerneltypes.h"
#include "mark3cfg.h"

#include "blocking.h"
#include "mutex.h"

#define _CAN_HAS_DEBUG
//--[Autogenerated - Do Not Modify]------------------------------------------
#include "dbg_file_list.h"
#include "buffalogger.h"
#if defined(DBG_FILE)
#error "Debug logging file token already defined!  Bailing."
#else
#define DBG_FILE _DBG___KERNEL_MUTEX_CPP
#endif
//--[End Autogenerated content]----------------------------------------------

#include "kerneldebug.h"

#if KERNEL_USE_MUTEX

#if KERNEL_USE_TIMEOUTS
namespace {
//---------------------------------------------------------------------------
/*!
 * \brief TimedMutex_Calback
 *
 * This function is called from the timer-expired context to trigger a timeout
 * on this mutex.  This results in the waking of the thread that generated
 * the mutex claim call that was not completed in time.
 *
 * \param pclOwner_ Pointer to the thread to wake
 * \param pvData_   Pointer to the mutex object that the thread is blocked on
 */
void TimedMutex_Callback(Thread* pclOwner_, void* pvData_)
{
    auto* pclMutex = static_cast<Mutex*>(pvData_);

    // Indicate that the semaphore has expired on the thread
    pclOwner_->SetExpired(true);

    // Wake up the thread that was blocked on this semaphore.
    pclMutex->WakeMe(pclOwner_);

    if (pclOwner_->GetCurPriority() >= Scheduler::GetCurrentThread()->GetCurPriority()) {
        Thread::Yield();
    }
}
} // anonymous namespace

//---------------------------------------------------------------------------
Mutex::~Mutex()
{
    // If there are any threads waiting on this object when it goes out
    // of scope, set a kernel panic.
    if (m_clBlockList.GetHead() != nullptr) {
        Kernel::Panic(PANIC_ACTIVE_MUTEX_DESCOPED);
    }
}

//---------------------------------------------------------------------------
void Mutex::WakeMe(Thread* pclOwner_)
{
    // Remove from the semaphore waitlist and back to its ready list.
    UnBlock(pclOwner_);
}

#endif

//---------------------------------------------------------------------------
uint8_t Mutex::WakeNext()
{
    // Get the highest priority waiter thread
    auto* pclChosenOne = m_clBlockList.HighestWaiter();

    // Unblock the thread
    UnBlock(pclChosenOne);

    // The chosen one now owns the mutex
    m_pclOwner = pclChosenOne;

    // Signal a context switch if it's a greater than or equal to the current priority
    if (pclChosenOne->GetCurPriority() >= Scheduler::GetCurrentThread()->GetCurPriority()) {
        return 1;
    }
    return 0;
}

//---------------------------------------------------------------------------
void Mutex::Init()
{
    // Cannot re-init a mutex which has threads blocked on it
#if KERNEL_EXTRA_CHECKS
    KERNEL_ASSERT(!m_clBlockList.GetHead());
#endif

    // Reset the data in the mutex
    m_bReady    = true;    // The mutex is free.
    m_u8MaxPri  = 0;    // Set the maximum priority inheritence state
    m_pclOwner  = NULL; // Clear the mutex owner
    m_u8Recurse = 0;    // Reset recurse count

#if KERNEL_EXTRA_CHECKS
    SetInitialized();
#endif
}

//---------------------------------------------------------------------------
#if KERNEL_USE_TIMEOUTS
bool Mutex::Claim_i(uint32_t u32WaitTimeMS_)
#else
void Mutex::Claim_i(void)
#endif
{
#if KERNEL_EXTRA_CHECKS
    KERNEL_ASSERT(IsInitialized());
#endif

    KERNEL_TRACE_1("Claiming Mutex, Thread %d", (uint16_t)g_pclCurrent->GetID());

#if KERNEL_USE_TIMEOUTS
    Timer clTimer;
    auto  bUseTimer = false;
#endif

    // Disable the scheduler while claiming the mutex - we're dealing with all
    // sorts of private thread data, can't have a thread switch while messing
    // with internal data structures.
    Scheduler::SetScheduler(false);

    // Check to see if the mutex is claimed or not
    if (static_cast<int>(m_bReady) != 0) {
        // Mutex isn't claimed, claim it.
        m_bReady    = false;
        m_u8Recurse = 0;
        m_u8MaxPri  = g_pclCurrent->GetPriority();
        m_pclOwner  = g_pclCurrent;

        Scheduler::SetScheduler(true);

#if KERNEL_USE_TIMEOUTS
        return true;
#else
        return;
#endif
    }

    // If the mutex is already claimed, check to see if this is the owner thread,
    // since we allow the mutex to be claimed recursively.
    if (g_pclCurrent == m_pclOwner) {
        // Ensure that we haven't exceeded the maximum recursive-lock count
        KERNEL_ASSERT((m_u8Recurse < 255));
        m_u8Recurse++;

        // Increment the lock count and bail
        Scheduler::SetScheduler(true);
#if KERNEL_USE_TIMEOUTS
        return true;
#else
        return;
#endif
    }

// The mutex is claimed already - we have to block now.  Move the
// current thread to the list of threads waiting on the mutex.
#if KERNEL_USE_TIMEOUTS
    if (u32WaitTimeMS_ != 0u) {
        g_pclCurrent->SetExpired(false);
        clTimer.Init();
        clTimer.Start(false, u32WaitTimeMS_, (TimerCallback_t)TimedMutex_Callback, (void*)this);
        bUseTimer = true;
    }
#endif
    BlockPriority(g_pclCurrent);

    // Check if priority inheritence is necessary.  We do this in order
    // to ensure that we don't end up with priority inversions in case
    // multiple threads are waiting on the same resource.
    if (m_u8MaxPri <= g_pclCurrent->GetPriority()) {
        m_u8MaxPri = g_pclCurrent->GetPriority();

        auto* pclTemp = static_cast<Thread*>(m_clBlockList.GetHead());
        while (pclTemp != nullptr) {
            pclTemp->InheritPriority(m_u8MaxPri);
            if (pclTemp == static_cast<Thread*>(m_clBlockList.GetTail())) {
                break;
            }
            pclTemp = static_cast<Thread*>(pclTemp->GetNext());
        }
        m_pclOwner->InheritPriority(m_u8MaxPri);
    }

    // Done with thread data -reenable the scheduler
    Scheduler::SetScheduler(true);

    // Switch threads if this thread acquired the mutex
    Thread::Yield();

#if KERNEL_USE_TIMEOUTS
    if (bUseTimer) {
        clTimer.Stop();
        return (static_cast<int>(g_pclCurrent->GetExpired()) == 0);
    }
    return true;
#endif
}

//---------------------------------------------------------------------------
void Mutex::Claim(void)
{
#if KERNEL_USE_TIMEOUTS
    Claim_i(0);
#else
    Claim_i();
#endif
}

//---------------------------------------------------------------------------
#if KERNEL_USE_TIMEOUTS
bool Mutex::Claim(uint32_t u32WaitTimeMS_)
{
    return Claim_i(u32WaitTimeMS_);
}
#endif

//---------------------------------------------------------------------------
void Mutex::Release()
{
#if KERNEL_EXTRA_CHECKS
    KERNEL_ASSERT(IsInitialized());
#endif

    KERNEL_TRACE_1("Releasing Mutex, Thread %d", (uint16_t)g_pclCurrent->GetID());

    auto bSchedule = false;

    // Disable the scheduler while we deal with internal data structures.
    Scheduler::SetScheduler(false);

    // This thread had better be the one that owns the mutex currently...
    KERNEL_ASSERT((g_pclCurrent == m_pclOwner));

    // If the owner had claimed the lock multiple times, decrease the lock
    // count and return immediately.
    if (m_u8Recurse != 0u) {
        m_u8Recurse--;
        Scheduler::SetScheduler(true);
        return;
    }

    // Restore the thread's original priority
    if (g_pclCurrent->GetCurPriority() != g_pclCurrent->GetPriority()) {
        g_pclCurrent->SetPriority(g_pclCurrent->GetPriority());

        // In this case, we want to reschedule
        bSchedule = true;
    }

    // No threads are waiting on this semaphore?
    if (m_clBlockList.GetHead() == NULL) {
        // Re-initialize the mutex to its default values
        m_bReady   = true;
        m_u8MaxPri = 0;
        m_pclOwner = NULL;
    } else {
        // Wake the highest priority Thread pending on the mutex
        if (WakeNext() != 0u) {
            // Switch threads if it's higher or equal priority than the current thread
            bSchedule = true;
        }
    }

    // Must enable the scheduler again in order to switch threads.
    Scheduler::SetScheduler(true);
    if (bSchedule) {
        // Switch threads if a higher-priority thread was woken
        Thread::Yield();
    }
}

#endif // KERNEL_USE_MUTEX
