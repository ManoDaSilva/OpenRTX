/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CALIBINFO_MICRO17_H
#define CALIBINFO_MICRO17_H

#include <stdint.h>

/**
 * Calibration data for Micro17.
 */
// clang-format off
struct micro17Calib {
    uint16_t tx_wiper;              ///< Baseband TX potentiometer
    uint16_t rx_wiper;              ///< Baseband RX potentiometer
    uint16_t mic_wiper;             ///< Microphone input potentiometer
    uint16_t spk_wiper;             ///< Speaker output potentiometer
    uint8_t  bb_tx_invert  : 1,     ///< Invert TX baseband
             bb_rx_invert  : 1,     ///< Invert RX baseband
             ptt_in_level  : 1,     ///< PTT in active level
             ptt_out_level : 1,     ///< PTT out active level
             _reserved     : 4;
};
// clang-format on

#endif /* CALIBINFO_MICRO17_H */
