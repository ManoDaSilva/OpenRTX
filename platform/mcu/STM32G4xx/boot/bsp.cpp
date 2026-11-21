/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/***********************************************************************
* bsp.cpp Part of the Miosix Embedded OS.
* Board support package, this file initializes hardware.
************************************************************************/

#include "interfaces/bsp.h"
#include <kernel/kernel.h>
#include <kernel/sync.h>
#include "hwconfig.h"

namespace miosix
{

//
// Initialization
//

void IRQbspInit()
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN
                  | RCC_AHB2ENR_GPIOCEN | RCC_AHB2ENR_GPIODEN
                  | RCC_AHB2ENR_GPIOEEN | RCC_AHB2ENR_GPIOFEN
                  | RCC_AHB2ENR_GPIOGEN;
    RCC_SYNC();

    GPIOA->OSPEEDR = 0xaaaaaaaa; //Default to 50MHz speed for all GPIOS
    GPIOB->OSPEEDR = 0xaaaaaaaa;
    GPIOC->OSPEEDR = 0xaaaaaaaa;
    GPIOD->OSPEEDR = 0xaaaaaaaa;
    GPIOE->OSPEEDR = 0xaaaaaaaa;
    GPIOF->OSPEEDR = 0xaaaaaaaa;
    GPIOG->OSPEEDR = 0xaaaaaaaa;

    // Configure SysTick
    SysTick->LOAD = SystemCoreClock / miosix::TICK_FREQ;
}

void bspInit2()
{
}

//
// Shutdown and reboot
//

void shutdown()
{
    reboot();
}

void reboot()
{
    disableInterrupts();
    miosix_private::IRQsystemReboot();
}

} //namespace miosix
