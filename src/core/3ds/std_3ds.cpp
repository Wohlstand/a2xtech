#include <ctime>
#include <cstdio>
#include <3ds.h>

#include "core/std.h"

uint32_t __stacksize__ = 0x00020000;

uint64_t originalTime = -1u;
uint64_t originalMicroTime = -1u;

osTimeRef_s tr;

uint64_t XStd::GetMicroTicks()
{
    if(originalMicroTime == -1u)
    {
        originalMicroTime = svcGetSystemTick();
        tr = osGetTimeRef();
    }
    return (uint64_t)(svcGetSystemTick() - originalTime) * 1000000 / tr.sysclock_hz;
}

uint32_t XStd::GetTicks()
{
    if(originalTime == -1u)
        originalTime = osGetTime();
    return (uint32_t)((osGetTime() - originalTime));
}
