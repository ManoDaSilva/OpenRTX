/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HWCONFIG_H
#define HWCONFIG_H

#include "stm32g4xx.h"
#include "pinmap.h"

/* ADC1 input channels, see the pin map */
enum AdcChannel {
    ADC_BASEBAND_IN_CH = 2,
    ADC_AUDIO_IN_CH = 3,
    ADC_HWVER_CH = 4,
};

/* Screen dimensions */
#define CONFIG_SCREEN_WIDTH 128
#define CONFIG_SCREEN_HEIGHT 64

/* Screen pixel format */
#define CONFIG_PIX_FMT_BW

/* Device has no battery */
#define CONFIG_BAT_NONE

/* Device supports M17 mode */
#define CONFIG_M17

#endif
