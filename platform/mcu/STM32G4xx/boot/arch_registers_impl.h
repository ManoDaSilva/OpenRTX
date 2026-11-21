/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ARCH_REGISTERS_IMPL_H
#define ARCH_REGISTERS_IMPL_H

//Always include stm32g4xx.h before core_cm4.h, there's some nasty dependency
#include "stm32g4xx.h"
#include "core_cm4.h"
#include "system_stm32g4xx.h"

// Workaround for a bug in STM32F42x, not needed on STM32G4
#define RCC_SYNC()

#endif // ARCH_REGISTERS_IMPL_H
