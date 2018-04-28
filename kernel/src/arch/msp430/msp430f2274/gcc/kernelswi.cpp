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

    \file   kernelswi.cpp

    \brief  Kernel Software interrupt implementation for ATMega328p

*/

#include "kerneltypes.h"
#include "kernelswi.h"
#include <msp430.h>

namespace Mark3 {
//---------------------------------------------------------------------------
void KernelSWI::Config(void)
{
    P1DIR &= ~0x04;
}

//---------------------------------------------------------------------------
void KernelSWI::Start(void)
{
    P1IE = 0x04;
}

//---------------------------------------------------------------------------
void KernelSWI::Stop(void)
{
    P1IE &= ~0x04;
}

//---------------------------------------------------------------------------
uint8_t KernelSWI::DI()
{
    uint8_t u8Ret = ((P1IE & 0x04) == 0x04);
    P1IE &= ~0x04;
    return u8Ret;
}

//---------------------------------------------------------------------------
void KernelSWI::RI(bool bEnable_)
{
    if (bEnable_) {
        P1IE |= 0x04;
    } else {
        P1IE &= ~0x04;
    }
}

//---------------------------------------------------------------------------
void KernelSWI::Clear(void)
{
    P1IFG &= ~0x04;
}

//---------------------------------------------------------------------------
void KernelSWI::Trigger(void)
{
    P1IFG |= 0x04;
}

} // using namespace Mark3
