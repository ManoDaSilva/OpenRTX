/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PINMAP_H
#define PINMAP_H

#include "stm32g4xx.h"

/* PTT control */
#define PTT_IN GPIOC, 13
#define PTT_OUT GPIOA, 6

/* Audio */
#define AUDIO_IN GPIOA, 2     // ADC1_IN3
#define AUDIO_OUT GPIOA, 5    // DAC1_OUT2
#define BASEBAND_IN GPIOA, 1  // ADC12_IN2
#define BASEBAND_OUT GPIOA, 4 // DAC1_OUT1
#define BASEBAND_BYP GPIOA, 7
#define AUDIOOUT_MUTE GPIOB, 1

/* I2C for MCP4451 */
#define I2C_SDA GPIOB, 9
#define I2C_SCL GPIOA, 15

/* Serial ports */
#define MCU_TX GPIOA, 9
#define MCU_RX GPIOA, 10
#define USART3_TX GPIOB, 10
#define USART3_RX GPIOB, 11

/* Hardware version detection, ADC1_IN4 */
#define AIN_HWVER GPIOA, 3

#endif /* PINMAP_H */
