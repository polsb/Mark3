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

//---------------------------------------------------------------------------

#include "kerneltypes.h"
#include "kernel.h"
#include "../ut_platform.h"
#include "thread.h"
#include "mutex.h"
#include "lockguard.h"

namespace
{
using namespace Mark3;
K_WORD aucTestStack[PORT_KERNEL_DEFAULT_STACK_SIZE];
Thread clMutexThread;

K_WORD           aucTestStack2[PORT_KERNEL_DEFAULT_STACK_SIZE];
Thread           clTestThread2;
volatile uint8_t u8Token;
} // anonymous namespace

namespace Mark3
{
//===========================================================================
// Local Defines
//===========================================================================

//===========================================================================
// Define Test Cases Here
//===========================================================================
TEST(ut_typical_mutex)
{
    auto lMutexTest = [](void* mutex_) {
        auto* pclMutex = static_cast<Mutex*>(mutex_);
        pclMutex->Claim();
        u8Token = 0x69;
        pclMutex->Release();
        // Exit the thread when we're done this operation.
        Scheduler::GetCurrentThread()->Exit();
    };

    // Test - Typical mutex usage, ensure that two threads can synchronize
    // access to a single resource
    Mutex clMutex;

    clMutex.Init();

    // Create a higher-priority thread that will immediately pre-empt u16.
    // Verify that while we have the mutex held, that the high-priority thread
    // is blocked waiting for u16 to relinquish access.
    clMutexThread.Init(aucTestStack, sizeof(aucTestStack), 7, lMutexTest, &clMutex);

    clMutex.Claim();

    u8Token = 0x96;
    clMutexThread.Start();

    // Spend some time sleeping, just to drive the point home...
    Thread::Sleep(100);

    // Test Point - Verify that the token value hasn't changed (which would
    // indicate the high-priority thread held the mutex...)
    EXPECT_EQUALS(u8Token, 0x96);

    // Relese the mutex, see what happens.
    clMutex.Release();

    // Test Point - Verify that after releasing the mutex, the higher-priority
    // thread immediately resumes, claiming the mutex, and adjusting the
    // token value to its value.  Check the new token value here.
    EXPECT_EQUALS(u8Token, 0x69);

    clMutexThread.Exit();
}

//===========================================================================
TEST(ut_timed_mutex)
{
    // Test - Enusre that when a thread fails to obtain a resource in a
    // timeout scenario, that the timeout is reported correctly
    auto lTimedTest = [](void* mutex_) {
        auto* pclMutex = static_cast<Mutex*>(mutex_);
        pclMutex->Claim();
        Thread::Sleep(20);
        pclMutex->Release();
        Scheduler::GetCurrentThread()->Exit();
    };

    Mutex clMutex;
    clMutex.Init();

    clMutexThread.Init(aucTestStack, sizeof(aucTestStack), 7, lTimedTest, &clMutex);
    clMutexThread.Start();

    EXPECT_FALSE(clMutex.Claim(10));

    Thread::Sleep(20);

    clMutexThread.Init(aucTestStack, sizeof(aucTestStack), 7, lTimedTest, &clMutex);
    clMutexThread.Start();

    EXPECT_TRUE(clMutex.Claim(30));

    clMutexThread.Exit();
}

//===========================================================================
TEST(ut_priority_mutex)
{
    auto lMutexThread = [](void* mutex_) {
        auto* pclMutex = static_cast<Mutex*>(mutex_);
        pclMutex->Claim();
        Thread::Sleep(100);
        pclMutex->Release();
        while (1) { Thread::Sleep(1000); }
    };

    // Test - Priority inheritence protocol.  Ensure that the priority
    // inversion problem is correctly avoided by our semaphore implementation
    // In the low/med/high scenario, we play the "med" priority thread
    Mutex clMutex;
    clMutex.Init();

    Scheduler::GetCurrentThread()->SetPriority(3);

    clMutexThread.Init(aucTestStack, sizeof(aucTestStack), 2, lMutexThread, &clMutex);
    clTestThread2.Init(aucTestStack2, sizeof(aucTestStack2), 4, lMutexThread, &clMutex);

    // Start the low-priority thread and give it the mutex
    clMutexThread.Start();
    Thread::Sleep(20);

    // Start the high-priority thread, which will block, waiting for the
    // low-priority action to complete...
    clTestThread2.Start();
    Thread::Sleep(20);

    // Test point - Low-priority thread boost:
    // Check the priorities of the threads.  The low-priority thread
    // should now have the same priority as the high-priority thread
    EXPECT_EQUALS(clMutexThread.GetCurPriority(), 4);
    EXPECT_EQUALS(clTestThread2.GetCurPriority(), 4);

    Thread::Sleep(2000);

    // Test point - Low-priority thread drop:
    // After the threads have relinquished their mutexes, ensure that
    // they are placed back at their correct priorities

    EXPECT_EQUALS(clMutexThread.GetCurPriority(), 2);
    EXPECT_EQUALS(clTestThread2.GetCurPriority(), 4);

    clMutexThread.Exit();
    clTestThread2.Exit();
}

//===========================================================================
TEST(ut_raii)
{
    Mutex clMutex;
    clMutex.Init();

    auto lRAIIThread = [](void* mutex_) {
        u8Token        = 0;
        auto* pclMutex = static_cast<Mutex*>(mutex_);
        {
            LockGuard lockGuard{ pclMutex };
            Thread::Sleep(100);
            u8Token = 1;
        }
        Scheduler::GetCurrentThread()->Exit();
    };

    Scheduler::GetCurrentThread()->SetPriority(3);
    clMutexThread.Init(aucTestStack, sizeof(aucTestStack), 2, lRAIIThread, &clMutex);
    clMutexThread.Start();

    Thread::Sleep(50);
    EXPECT_EQUALS(u8Token, 0);
    {
        LockGuard lockGuard{ &clMutex };
        EXPECT_EQUALS(u8Token, 1);
    }

    Thread::Sleep(100);

    clMutexThread.Exit();
}

//===========================================================================
TEST(ut_raii_timeout)
{
    u8Token          = 0;
    auto lRAIIThread = [](void* mutex_) {
        auto* pclMutex = static_cast<Mutex*>(mutex_);
        {
            LockGuard lockGuard{ pclMutex, 50 };
            if (lockGuard.isAcquired()) {
                u8Token = 1;
            } else {
                u8Token = 2;
            }
            Thread::Sleep(100);
        }
        Scheduler::GetCurrentThread()->Exit();
    };

    Mutex clMutex;
    clMutex.Init();

    Scheduler::GetCurrentThread()->SetPriority(3);
    clMutexThread.Init(aucTestStack, sizeof(aucTestStack), 2, lRAIIThread, &clMutex);
    clTestThread2.Init(aucTestStack2, sizeof(aucTestStack2), 2, lRAIIThread, &clMutex);

    EXPECT_EQUALS(u8Token, 0);
    clMutexThread.Start();

    Thread::Sleep(30);
    EXPECT_EQUALS(u8Token, 1);
    clTestThread2.Start();

    Thread::Sleep(30);
    EXPECT_EQUALS(u8Token, 1);

    Thread::Sleep(50);
    EXPECT_EQUALS(u8Token, 2);

    Thread::Sleep(100);

    clMutexThread.Exit();
    clTestThread2.Exit();
}

//===========================================================================
// Test Whitelist Goes Here
//===========================================================================
TEST_CASE_START
TEST_CASE(ut_typical_mutex), TEST_CASE(ut_timed_mutex), TEST_CASE(ut_priority_mutex), TEST_CASE(ut_raii),
    TEST_CASE(ut_raii_timeout), TEST_CASE_END
} // namespace Mark3
