/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "interfaces/nvmem.h"
#include "calibration/calibInfo_Micro17.h"
#include "core/cps.h"

/*
 * Micro17 has no storage for the calibration data yet: use fixed values.
 * - The 8-bit MCP4451 has 257 wiper steps, 0x000 to 0x100: start at mid
 *   scale. https://www.microchip.com/en-us/product/MCP4451
 * - The radio PTT lines are both active low: the PTT button shorts PTTIN to
 *   ground, and PTTO is pulled up to 5V by the radio. See the wiring notes in
 *   https://github.com/ManoDaSilva/Micro17/tree/main/icom_ICF3_4_5_6XXX
 * - The baseband is inverted in both directions on the ICOM IC-F radios.
 */
struct micro17Calib micro17CalData = {
    .tx_wiper = 0x080,
    .rx_wiper = 0x080,
    .mic_wiper = 0x080,
    .spk_wiper = 0x080,
    .bb_tx_invert = 1,
    .bb_rx_invert = 1,
    .ptt_in_level = 0,
    .ptt_out_level = 0,
};

const struct nvmTable nvmTab = {
    .areas = NULL,
    .nbAreas = 0,
};

void nvm_init()
{
}

void nvm_terminate()
{
}

void nvm_readCalibData(void *buf)
{
    (void)buf;
}

void nvm_readHwInfo(hwInfo_t *info)
{
    (void)info;
}

int nvm_readVfoChannelData(channel_t *channel)
{
    // Micro17 does not tune the radio: the frequency is set by the channel
    // selected on the radio itself, and the operating mode is always M17.
    *channel = cps_getDefaultChannel();
    channel->mode = OPMODE_M17;
    channel->rx_frequency = 0;
    channel->tx_frequency = 0;

    return 0;
}

int nvm_readSettings(settings_t *settings)
{
    (void)settings;

    return -1;
}

int nvm_writeSettings(const settings_t *settings)
{
    (void)settings;

    return -1;
}

int nvm_writeSettingsAndVfo(const settings_t *settings, const channel_t *vfo)
{
    (void)settings;
    (void)vfo;

    return -1;
}
