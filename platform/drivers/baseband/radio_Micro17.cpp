/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "interfaces/radio.h"
#include "peripherals/gpio.h"
#include "calibration/calibInfo_Micro17.h"
#include "hwconfig.h"

static enum opstatus radioStatus;          // Current operating status
extern struct micro17Calib micro17CalData; // Calibration data

void radio_init(const rtxStatus_t *rtxState)
{
    (void)rtxState;

    // Configure PTT output GPIO and set it to idle state (TX off)
    gpio_setMode(PTT_OUT, OUTPUT);
    radio_disableRtx();

    radioStatus = OFF;
}

void radio_terminate()
{
    radioStatus = OFF;
}

void radio_setOpmode(const enum opmode mode)
{
    (void)mode;
}

bool radio_checkRxDigitalSquelch()
{
    return false;
}

void radio_enableAfOutput()
{
}

void radio_disableAfOutput()
{
}

void radio_enableRx()
{
    radioStatus = RX;

    // Drive the PTT output to its idle level, set by the calibration data
    if (micro17CalData.ptt_out_level)
        gpio_setPin(PTT_OUT);
    else
        gpio_clearPin(PTT_OUT);
}

void radio_enableTx()
{
    radioStatus = TX;

    if (micro17CalData.ptt_out_level)
        gpio_clearPin(PTT_OUT);
    else
        gpio_setPin(PTT_OUT);
}

void radio_disableRtx()
{
    if (micro17CalData.ptt_out_level)
        gpio_setPin(PTT_OUT);
    else
        gpio_clearPin(PTT_OUT);
}

void radio_updateConfiguration()
{
}

rssi_t radio_getRssi()
{
    return -123.0f;
}

enum opstatus radio_getStatus()
{
    return radioStatus;
}
