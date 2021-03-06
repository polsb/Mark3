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
/**

    @file   timerlist.cpp

    @brief  Implements timer list processing algorithms, responsible for all
            timer tick and expiry logic.

*/

#include "mark3.h"
namespace Mark3
{
//---------------------------------------------------------------------------
void TimerList::Init(void)
{
    m_bTimerActive  = false;
    m_u32NextWakeup = 0;
    m_clMutex.Init();
}

//---------------------------------------------------------------------------
void TimerList::Add(Timer* pclListNode_)
{
    KERNEL_ASSERT(pclListNode_ != nullptr);
    auto lock = LockGuard{ &m_clMutex };

    pclListNode_->ClearNode();
    DoubleLinkList::Add(pclListNode_);

    // Set the initial timer value
    pclListNode_->m_u32TimeLeft = pclListNode_->m_u32Interval;

    // Set the timer as active.
    pclListNode_->m_u8Flags |= TIMERLIST_FLAG_ACTIVE;
}

//---------------------------------------------------------------------------
void TimerList::Remove(Timer* pclLinkListNode_)
{
    KERNEL_ASSERT(pclLinkListNode_ != nullptr);
    auto lock = LockGuard{ &m_clMutex };

    DoubleLinkList::Remove(pclLinkListNode_);
    pclLinkListNode_->m_u8Flags &= ~TIMERLIST_FLAG_ACTIVE;
}

//---------------------------------------------------------------------------
void TimerList::Process(void)
{
    auto lock = LockGuard{ &m_clMutex };

    auto* pclCurr = static_cast<Timer*>(GetHead());
    // Subtract the elapsed time interval from each active timer.
    while (pclCurr != 0) {
        auto* pclNext = static_cast<Timer*>(pclCurr->GetNext());

        // Active timers only...
        if ((pclCurr->m_u8Flags & TIMERLIST_FLAG_ACTIVE) != 0) {
            pclCurr->m_u32TimeLeft--;
            if (0 == pclCurr->m_u32TimeLeft) {
                // Expired -- run the callback. these callbacks must be very fast...
                if (pclCurr->m_pfCallback != nullptr) {
                    pclCurr->m_pfCallback(pclCurr->m_pclOwner, pclCurr->m_pvData);
                }

                if ((pclCurr->m_u8Flags & TIMERLIST_FLAG_ONE_SHOT) != 0) {
                    // If this was a one-shot timer, deactivate the timer + remove
                    pclCurr->m_u8Flags |= TIMERLIST_FLAG_EXPIRED;
                    pclCurr->m_u8Flags &= ~TIMERLIST_FLAG_ACTIVE;
                    Remove(pclCurr);
                } else {
                    // Reset the interval timer.
                    pclCurr->m_u32TimeLeft = pclCurr->m_u32Interval;
                }
            }
        }
        pclCurr = pclNext;
    }
}

} // namespace Mark3
