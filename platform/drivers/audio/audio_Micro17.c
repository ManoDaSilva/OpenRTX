/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "interfaces/audio.h"
#include "peripherals/gpio.h"
#include "hwconfig.h"
#include "stm32_dac.h"
#include "stm32_adc.h"

/*
 * Audio paths of Micro17, the same as Module17: the baseband from and to the
 * radio goes through ADC1 channel 2 and DAC channel 1, the microphone and
 * speaker audio through ADC1 channel 3 and DAC channel 2. Both inputs share
 * ADC1, so only one input path can be open at a time.
 */

// clang-format off
static const uint8_t pathCompatibilityMatrix[9][9] =
{
    // MIC-SPK MIC-RTX MIC-MCU RTX-SPK RTX-RTX RTX-MCU MCU-SPK MCU-RTX MCU-MCU
    {    0   ,   0   ,   0   ,   1   ,   0   ,   1   ,   1   ,   0   ,   1   },  // MIC-RTX
    {    0   ,   0   ,   0   ,   0   ,   1   ,   1   ,   0   ,   1   ,   1   },  // MIC-SPK
    {    0   ,   0   ,   0   ,   1   ,   1   ,   0   ,   1   ,   1   ,   0   },  // MIC-MCU
    {    0   ,   1   ,   1   ,   0   ,   0   ,   0   ,   0   ,   1   ,   1   },  // RTX-SPK
    {    1   ,   0   ,   1   ,   0   ,   0   ,   0   ,   1   ,   0   ,   1   },  // RTX-RTX
    {    1   ,   1   ,   0   ,   0   ,   0   ,   0   ,   1   ,   1   ,   0   },  // RTX-MCU
    {    0   ,   1   ,   1   ,   0   ,   1   ,   1   ,   0   ,   0   ,   0   },  // MCU-SPK
    {    1   ,   0   ,   1   ,   1   ,   0   ,   1   ,   0   ,   0   ,   0   },  // MCU-RTX
    {    1   ,   1   ,   0   ,   1   ,   1   ,   0   ,   0   ,   0   ,   0   },  // MCU-MCU
};
// clang-format on

const struct audioDevice outputDevices[] = {
    {
        .driver = NULL,
        .config = NULL,
        .instance = 0,
        .endpoint = SINK_MCU,
    },
    {
        .driver = &stm32_dac_audio_driver,
        .config = NULL,
        .instance = STM32_DAC_CH1,
        .endpoint = SINK_RTX,
    },
    {
        .driver = &stm32_dac_audio_driver,
        .config = NULL,
        .instance = STM32_DAC_CH2,
        .endpoint = SINK_SPK,
    },
};

const struct audioDevice inputDevices[] = {
    {
        .driver = NULL,
        .config = NULL,
        .instance = 0,
        .endpoint = SOURCE_MCU,
    },
    {
        .driver = &stm32_adc_audio_driver,
        .config = (const void *)ADC_BASEBAND_IN_CH,
        .instance = STM32_ADC_ADC1,
        .endpoint = SOURCE_RTX,
    },
    {
        .driver = &stm32_adc_audio_driver,
        .config = (const void *)ADC_AUDIO_IN_CH,
        .instance = STM32_ADC_ADC1,
        .endpoint = SOURCE_MIC,
    },
};

void audio_init()
{
    gpio_setMode(AUDIO_IN, ANALOG);
    gpio_setMode(AUDIO_OUT, ANALOG);
    gpio_setMode(BASEBAND_IN, ANALOG);
    gpio_setMode(BASEBAND_OUT, ANALOG);

    stm32dac_init(STM32_DAC_CH1, 2048);
    stm32dac_init(STM32_DAC_CH2, 2048);
    stm32adc_init(STM32_ADC_ADC1);
}

void audio_terminate()
{
    stm32dac_terminate();
    stm32adc_terminate();
}

void audio_connect(const enum AudioSource source, const enum AudioSink sink)
{
    (void)source;
    (void)sink;
}

void audio_disconnect(const enum AudioSource source, const enum AudioSink sink)
{
    (void)source;
    (void)sink;
}

bool audio_checkPathCompatibility(const enum AudioSource p1Source,
                                  const enum AudioSink p1Sink,
                                  const enum AudioSource p2Source,
                                  const enum AudioSink p2Sink)
{
    const uint8_t p1Index = (p1Source * 3) + p1Sink;
    const uint8_t p2Index = (p2Source * 3) + p2Sink;

    return pathCompatibilityMatrix[p1Index][p2Index] == 1;
}
