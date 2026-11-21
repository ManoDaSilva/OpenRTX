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

/*
 * Drive the PTT output towards the radio. The line is open drain: the radio
 * pulls it up to 5V, so the MCU must never drive it high. ptt_out_level is the
 * level of the line while transmitting.
 * Wiring: https://github.com/ManoDaSilva/Micro17/tree/main/icom_ICF3_4_5_6XXX
 */
static void setPtt(const bool tx)
{
    const bool active = (micro17CalData.ptt_out_level == 1);

    if (tx == active)
        gpio_setPin(PTT_OUT);
    else
        gpio_clearPin(PTT_OUT);
}

void radio_init(const rtxStatus_t *rtxState)
{
    (void)rtxState;

    // Set the PTT output to its idle state (TX off) before enabling it, so
    // that the line is never asserted while the GPIO is being configured
    radio_disableRtx();
    gpio_setMode(PTT_OUT, OPEN_DRAIN);
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
    setPtt(false);
}

void radio_enableTx()
{
    radioStatus = TX;
    setPtt(true);
}

void radio_disableRtx()
{
    radioStatus = OFF;
    setPtt(false);
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
