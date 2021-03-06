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
#include "ksemaphore.h"
#include "thread.h"
#include "memutil.h"
#include "driver.h"

//===========================================================================
// Local Defines
//===========================================================================
namespace
{
using namespace Mark3;

Thread           clThread;
K_WORD           aucStack[PORT_KERNEL_DEFAULT_STACK_SIZE];
Semaphore        clSem1;
Semaphore        clSem2;
volatile uint8_t u8Counter = 0;
} // anonymous namespace

namespace Mark3
{
//===========================================================================
// Define Test Cases Here
//===========================================================================
TEST(ut_semaphore_count)
{
    // Test - verify that we can only increment a counting semaphore to the
    // maximum count.

    // Verify that a counting semaphore with an initial count of zero and a
    // maximum count of 10 can only be posted 10 times before saturaiton and
    // failure.

    Semaphore clTestSem;
    clTestSem.Init(0, 10);

    for (int i = 0; i < 10; i++) { EXPECT_TRUE(clTestSem.Post()); }
    EXPECT_FALSE(clTestSem.Post());
}

//===========================================================================
TEST(ut_semaphore_post_pend)
{
    auto lPostPend = [](void* param_) {
        auto* pclSem = static_cast<Semaphore*>(param_);
        while (1) {
            pclSem->Pend();
            u8Counter++;
        }
    };

    // Test - Make sure that pending on a semaphore causes a higher-priority
    // waiting thread to block, and that posting that semaphore from a running
    // lower-priority thread awakens the higher-priority thread

    clSem1.Init(0, 1);

    clThread.Init(aucStack, sizeof(aucStack), 7, lPostPend, (void*)&clSem1);
    clThread.Start();
    for (int i = 0; i < 10; i++) { clSem1.Post(); }

    // Verify all 10 posts have been acknowledged by the high-priority thread
    EXPECT_EQUALS(u8Counter, 10);

    // After the test is over, kill the test thread.
    clThread.Exit();

    // Test - same as above, but with a counting semaphore instead of a
    // binary semaphore.  Also using a default value.
    clSem2.Init(10, 10);

    // Restart the test thread.
    u8Counter = 0;
    clThread.Init(aucStack, sizeof(aucStack), 7, lPostPend, (void*)&clSem2);
    clThread.Start();

    // We'll kill the thread as soon as it blocks.
    clThread.Exit();

    // semaphore should have pended 10 times before returning.
    EXPECT_EQUALS(u8Counter, 10);
}

//===========================================================================
TEST(ut_semaphore_timed)
{
    auto lTimedSem = [](void* param_) {
        auto* pclSem = static_cast<Semaphore*>(param_);
        Thread::Sleep(20);
        pclSem->Post();
        Scheduler::GetCurrentThread()->Exit();
    };

    Semaphore clTestSem;
    Semaphore clTestSem2;

    clTestSem.Init(0, 1);

    clThread.Init(aucStack, sizeof(aucStack), 7, lTimedSem, (void*)&clTestSem);
    clThread.Start();

    EXPECT_FALSE(clTestSem.Pend(10));
    Thread::Sleep(20);

    // Pretty nuanced - we can only re-init the semaphore under the knowledge
    // that there's nothing blocking on it already...  don't do this in
    // production
    clTestSem2.Init(0, 1);

    clThread.Init(aucStack, sizeof(aucStack), 7, lTimedSem, (void*)&clTestSem2);
    clThread.Start();

    EXPECT_TRUE(clTestSem2.Pend(30));
}

//===========================================================================
// Test Whitelist Goes Here
//===========================================================================
TEST_CASE_START
TEST_CASE(ut_semaphore_count), TEST_CASE(ut_semaphore_post_pend), TEST_CASE(ut_semaphore_timed), TEST_CASE_END
} // namespace Mark3
