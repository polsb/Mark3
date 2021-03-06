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
    @file kernelprofile.cpp

    @brief Profiling timer implementation
*/

#include "kerneltypes.h"
#include "mark3cfg.h"
#include "profile.h"
#include "kernelprofile.h"
#include "threadport.h"

#include "m3_core_cm3.h"

namespace Mark3
{
uint32_t Profiler::m_u32Epoch;

//---------------------------------------------------------------------------
void Profiler::Init() {}

//---------------------------------------------------------------------------
void Profiler::Start() {}

//---------------------------------------------------------------------------
void Profiler::Stop() {}
//---------------------------------------------------------------------------
uint16_t Profiler::Read()
{
    return 0;
}

//---------------------------------------------------------------------------
void Profiler::Process() {}

} // namespace Mark3
