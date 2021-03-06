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
=========================================================================== */
/**

    @file   blocking.h

    @brief  Blocking object base class declarations

    A Blocking object in Mark3 is essentially a thread list.  Any blocking
    object implementation (being a semaphore, mutex, event flag, etc.) can
    be built on top of this class, utilizing the provided functions to
    manipu32ate thread location within the Kernel.

    Blocking a thread results in that thread becoming de-scheduled, placed
    in the blocking object's own private list of threads which are waiting
    on the object.

    Unblocking a thread results in the reverse:  The thread is moved back to
    its original location from the blocking list.

    The only difference between a blocking object based on this class is the
    logic used to determine what consitutes a Block or Unblock condition.

    For instance, a semaphore Pend operation may result in a call to the
    Block() method with the currently-executing thread in order to make that
    thread wait for a semaphore Post.  That operation would then invoke the
    UnBlock() method, removing the blocking thread from the semaphore's list,
    and back into the the appropriate thread inside the scheduler.

    Care must be taken when implementing blocking objects to ensure that
    critical sections are used judiciously, otherwise asynchronous events
    like timers and interrupts could result in non-deterministic and often
    catastrophic behavior.
 */
#pragma once

#include "kerneltypes.h"
#include "mark3cfg.h"

#include "ll.h"
#include "threadlist.h"

namespace Mark3
{
//---------------------------------------------------------------------------
// Cookies used to determine whether or not an object has been initialized
#define BLOCKING_INVALID_COOKIE (0x3C)
#define BLOCKING_INIT_COOKIE (0xC3)

class Thread;

//---------------------------------------------------------------------------
/**
 *  Class implementing thread-blocking primatives.  used for implementing
 *  things like semaphores, mutexes, message queues, or anything else that
 *  could cause a thread to suspend execution on some external stimulus.
 */
class BlockingObject
{
public:
    BlockingObject() { m_u8Initialized = BLOCKING_INVALID_COOKIE; }
    ~BlockingObject() { m_u8Initialized = BLOCKING_INVALID_COOKIE; }

protected:
    /**
     *  @brief Block
     *
     *  Blocks a thread on this object.  This is the fundamental operation
     *  performed by any sort of blocking operation in the operating system.
     *  All semaphores/mutexes/sleeping/messaging/etc ends up going through
     *  the blocking code at some point as part of the code that manages a
     *  transition from an "active" or "waiting" thread to a "blocked" thread.
     *
     *  The steps involved in blocking a thread (which are performed in the
     *  function itself) are as follows;
     *
     *  1)  Remove the specified thread from the current owner's list (which
     *      is likely one of the scheduler's thread lists)
     *  2)  Add the thread to this object's thread list
     *  3)  Setting the thread's "current thread-list" point to reference this
     *      object's threadlist.
     *
     *  @param pclThread_ Pointer to the thread object that will be blocked.
     */
    void Block(Thread* pclThread_);

    /**
     * @brief BlockPriority
     *
     * Same as Block(), but ensures that threads are added to the block-list
     * in priority-order, which optimizes the unblock procedure.
     *
     * @param pclThread_ Pointer to the Thread to Block.
     */
    void BlockPriority(Thread* pclThread_);

    /**
     *  @brief UnBlock
     *
     *  Unblock a thread that is already blocked on this object, returning it
     *  to the "ready" state by performing the following steps:
     *
     *  @param pclThread_ Pointer to the thread to unblock.
     *
     *  1)  Removing the thread from this object's threadlist
     *  2)  Restoring the thread to its "original" owner's list
     */
    void UnBlock(Thread* pclThread_);

    /**
     * @brief SetInitialized
     */
    void SetInitialized(void) { m_u8Initialized = BLOCKING_INIT_COOKIE; }

    /**
     * @brief IsInitialized
     * @return
     */
    bool IsInitialized(void) { return (m_u8Initialized == BLOCKING_INIT_COOKIE); }

    /**
     *  ThreadList which is used to hold the list of threads blocked
     *  on a given object.
     */
    ThreadList m_clBlockList;

    /**
     * Token used to check whether or not the object has been initialized
     * prior to use.
     */
    uint8_t m_u8Initialized;
};
} // namespace Mark3
