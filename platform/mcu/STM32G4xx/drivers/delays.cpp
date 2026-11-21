/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "interfaces/delays.h"
#include <miosix.h>

/**
 * Implementation of the delay functions for STM32G4 MCUs.
 *
 * The busy loops below take four CPU cycles per iteration. The iteration
 * counts are scaled from the STM32F405 ones (42 per microsecond at 168MHz) and
 * have not been measured on hardware yet.
 */

#if defined(SYSCLK_FREQ_170MHz)
#define LOOPS_PER_US 43
#define LOOPS_PER_MS 42500
#elif defined(SYSCLK_FREQ_100MHz)
#define LOOPS_PER_US 25
#define LOOPS_PER_MS 25000
#elif defined(SYSCLK_FREQ_84MHz)
#define LOOPS_PER_US 21
#define LOOPS_PER_MS 21000
#else
#error "Delays are uncalibrated for this clock frequency"
#endif

/*
 * Spin for the given number of loop iterations. Written in assembler to be
 * independent of compiler optimization.
 */
static inline void spin(unsigned int count)
{
    asm volatile("           mov   r1, #0     \n"
                 "1:         cmp   r1, %0     \n"
                 "           itt   lo         \n"
                 "           addlo r1, r1, #1 \n"
                 "           blo   1b         \n"
                 :
                 : "r"(count)
                 : "r1");
}

void delayUs(unsigned int useconds)
{
    spin(useconds * LOOPS_PER_US);
}

void delayMs(unsigned int mseconds)
{
    for (unsigned int i = 0; i < mseconds; i++)
        spin(LOOPS_PER_MS);
}

void sleepFor(unsigned int seconds, unsigned int mseconds)
{
    unsigned int time = (seconds * 1000) + mseconds;
    miosix::Thread::sleep(time);
}

void sleepUntil(long long timestamp)
{
    miosix::Thread::sleepUntil(timestamp);
}

long long getTick()
{
    return miosix::getTick();
}
