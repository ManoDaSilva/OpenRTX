/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TIMER_H
#define TIMER_H

#include "stm32g4xx.h"

/**
 * Handler class for STM32G4 Timer peripheral, used as trigger source for the
 * ADC and the DAC. Same interface as the STM32F4 version.
 */
class Timer
{
public:
    /**
     * Constructor.
     *
     * @param tim: base address of timer peripheral to manage.
     */
    constexpr Timer(const uint32_t tim) : tim(tim)
    {
    }

    ~Timer() = default;

    /**
     * Configure timer prescaler and auto-reload registers for a given update
     * frequency.
     *
     * @param busFreq: frequency of the timer kernel clock, in Hz.
     * @param updFreq: desidered update frequency, in Hz.
     * @return effective timer update frequency, in Hz.
     */
    uint32_t setUpdateFrequency(const uint32_t busFreq,
                                const uint32_t updFreq) const
    {
        /*
         * Timer update frequency is given by:
         * Fupd = (Fbus / prescaler) / autoreload
         *
         * First of all we fix the prescaler to 1 and compute the autoreload: if
         * the result is greater than the maximum autoreload value, we proceed
         * iteratively.
         */
        uint32_t psc = 1;
        uint32_t arr = busFreq / updFreq;
        while (arr >= 0xFFFF) {
            psc += 1;
            arr = (busFreq / psc) / updFreq;
        }

        // Values put in registers have to be decremented by one, see RM
        regs()->PSC = psc - 1;
        regs()->ARR = arr - 1;
        regs()->CR2 = TIM_CR2_MMS_1; // Update event as TRGO

        return (busFreq / psc) / arr;
    }

    /**
     * Configure timer prescaler and auto-reload registers for a given update
     * frequency.
     *
     * @param updFreq: desidered update frequency, in Hz.
     * @return effective timer update frequency, in Hz.
     */
    uint32_t setUpdateFrequency(const uint32_t updFreq) const
    {
        /*
         * The timers on an APB bus run at the bus frequency when the APB
         * prescaler is 1, at twice the bus frequency otherwise (RM0440,
         * section "Timer clock"). PPREx values of 0b1xx divide by 2^(x + 1).
         */
        const bool apb2 = (tim >= APB2PERIPH_BASE);
        const uint32_t pos = apb2 ? RCC_CFGR_PPRE2_Pos : RCC_CFGR_PPRE1_Pos;
        const uint32_t ppre = (RCC->CFGR >> pos) & 0x07;
        uint32_t busFreq = SystemCoreClock;

        if ((ppre & 0x04) != 0)
            busFreq /= 1 << (ppre & 0x03);

        return setUpdateFrequency(busFreq, updFreq);
    }

    /**
     * Enable or disable the generation of DMA requests on timer update events.
     *
     * @param enable: set to true to enable the generation of DMA requests.
     */
    inline void enableDmaTrigger(const bool enable) const
    {
        regs()->DIER = enable ? TIM_DIER_UDE : 0;
    }

    /**
     * Clear and start the timer's counter.
     */
    inline void start() const
    {
        regs()->CNT = 0;
        regs()->EGR = TIM_EGR_UG;
        regs()->CR1 = TIM_CR1_CEN;
    }

    /**
     * Stop the timer's counter.
     */
    inline void stop() const
    {
        regs()->CR1 = 0;
    }

    /**
     * Get current value of timer's counter.
     *
     * @return value of timer's counter.
     */
    inline uint32_t value() const
    {
        return regs()->CNT;
    }

private:
    inline TIM_TypeDef *regs() const
    {
        return reinterpret_cast<TIM_TypeDef *>(tim);
    }

    const uint32_t tim;
};

#endif /* TIMER_H */
