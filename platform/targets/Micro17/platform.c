/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "interfaces/platform.h"
#include "peripherals/gpio.h"
#include "calibration/calibInfo_Micro17.h"
#include "hwconfig.h"

extern struct micro17Calib micro17CalData;

static const hwInfo_t hwInfo = {
    .vhf_maxFreq = 0,
    .vhf_minFreq = 0,
    .vhf_band = 0,
    .uhf_maxFreq = 0,
    .uhf_minFreq = 0,
    .uhf_band = 0,
    .hw_version = 0,
    .flags = 0,
    .name = "Micro17",
};

void platform_init()
{
    gpio_setMode(AIN_HWVER, ANALOG);
    gpio_setMode(PTT_IN, INPUT);
}

void platform_terminate()
{
}

uint16_t platform_getVbat()
{
    return 0;
}

uint8_t platform_getMicLevel()
{
    return 0;
}

uint8_t platform_getVolumeLevel()
{
    return 0;
}

int8_t platform_getChSelector()
{
    return 0;
}

bool platform_getPttStatus()
{
    // Return true if gpio status matches the PTT in active level
    uint8_t ptt_status = gpio_readPin(PTT_IN);
    if (ptt_status == micro17CalData.ptt_in_level)
        return true;

    return false;
}

bool platform_pwrButtonStatus()
{
    return true;
}

void platform_ledOn(led_t led)
{
    (void)led;
}

void platform_ledOff(led_t led)
{
    (void)led;
}

void platform_beepStart(uint16_t freq)
{
    (void)freq;
}

void platform_beepStop()
{
}

const hwInfo_t *platform_getHwInfo()
{
    return &hwInfo;
}
