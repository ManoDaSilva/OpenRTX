/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <kernel/scheduler/scheduler.h>
#include "core/cache_cortexMx.h"
#include "peripherals/gpio.h"
#include "core/data_conversion.h"
#include "DmaStream.hpp"
#include <miosix.h>
#include <errno.h>
#include "hwconfig.h"
#include "stm32_dac.h"

#if defined(STM32H743xx)
#include "Lptim.hpp"

#define DAC             DAC1
#define DAC_TRIG_CH1    (11 <<  2)  // lptim1_out
#define DAC_TRIG_CH2    (12 << 18)  // lptim2_out

typedef Lptim          timebase_type;
#define TimebaseCh1    Lptim(LPTIM1_BASE, 168000000)
#define TimebaseCh2    Lptim(LPTIM2_BASE, 168000000)

using DacDmaCh1 = DmaStream< DMA1_BASE, 5, 7, 3 >; // DMA 1, Stream 5, channel 7, very high priority
using DacDmaCh2 = DmaStream< DMA1_BASE, 6, 7, 3 >; // DMA 1, Stream 6, channel 7, very high priority
#define DacDmaCh1_IRQHandler    DMA1_Stream5_IRQHandler
#define DacDmaCh2_IRQHandler    DMA1_Stream6_IRQHandler
#elif defined(STM32G473xx)
#include "Timer.hpp"

/*
 * Trigger selection and DMAMUX request IDs from the STM32G4 HAL,
 * stm32g4xx_hal_dac.h (DAC_TRIGGER_T6_TRGO, DAC_TRIGGER_T7_TRGO) and
 * stm32g4xx_hal_dma.h (DMA_REQUEST_DAC1_CHANNEL1/2).
 * https://github.com/STMicroelectronics/stm32g4xx_hal_driver
 */
#define DAC_TRIG_CH1    (7 << DAC_CR_TSEL1_Pos)     // TIM6_TRGO
#define DAC_TRIG_CH2    (2 << DAC_CR_TSEL2_Pos)     // TIM7_TRGO

typedef Timer          timebase_type;
#define TimebaseCh1    Timer(TIM6_BASE)
#define TimebaseCh2    Timer(TIM7_BASE)

using DacDmaCh1 = DmaStream< DMA1_BASE, 1, 6, 3 >; // DMA 1, Channel 1, DAC1_CH1 request, very high priority
using DacDmaCh2 = DmaStream< DMA1_BASE, 2, 7, 3 >; // DMA 1, Channel 2, DAC1_CH2 request, very high priority
#define DacDmaCh1_IRQHandler    DMA1_Channel1_IRQHandler
#define DacDmaCh2_IRQHandler    DMA1_Channel2_IRQHandler
#else
#include "Timer.hpp"

#define DAC_TRIG_CH1    0x00
#define DAC_TRIG_CH2    DAC_CR_TSEL2_1

typedef Timer          timebase_type;
#define TimebaseCh1    Timer(TIM6_BASE)
#define TimebaseCh2    Timer(TIM7_BASE)

using DacDmaCh1 = DmaStream< DMA1_BASE, 5, 7, 3 >; // DMA 1, Stream 5, channel 7, very high priority
using DacDmaCh2 = DmaStream< DMA1_BASE, 6, 7, 3 >; // DMA 1, Stream 6, channel 7, very high priority
#define DacDmaCh1_IRQHandler    DMA1_Stream5_IRQHandler
#define DacDmaCh2_IRQHandler    DMA1_Stream6_IRQHandler
#endif

struct ChannelState
{
    struct streamCtx  *ctx;         // Current stream context
    uint32_t          idleLevel;    // Output idle level
    StreamHandler     stream;       // DMA stream handler
};

struct DacChannel
{
    volatile uint32_t  *dacReg;     // DAC data register
    timebase_type       tim;        // TIM peripheral for DAC trigger
};


static constexpr DacChannel channels[] =
{
    {&(DAC->DHR12R1), TimebaseCh1},
    {&(DAC->DHR12R2), TimebaseCh2},
};

#pragma GCC diagnostic ignored "-Wpedantic"
struct ChannelState chState[] =
{
    {
        .ctx = NULL,
        .idleLevel = 0,
        .stream = DacDmaCh1::init(10, DataSize::_16BIT, 1)
    },
    {
        .ctx = NULL,
        .idleLevel = 0,
        .stream = DacDmaCh2::init(10, DataSize::_16BIT, 1)
    }
};
#pragma GCC diagnostic pop


/**
 * \internal
 * Stop an ongoing transfer, deactivating timers and DMA stream.
 */
static void stopTransfer(const uint8_t chNum)
{
    channels[chNum].tim.stop();
    chState[chNum].ctx->running = 0;

    // If the data register is written when the external trigger is still
    // enabled, the DAC output may not be udated.
    DAC->CR &= ~(DAC_CR_TEN1 << (chNum * 16));
    *channels[chNum].dacReg = chState[chNum].idleLevel;
}

/**
 * \internal
 * Actual implementation of DMA interrupt handler.
 */
void __attribute__((used)) DMA_Handler(uint32_t chNum)
{
    if(chNum == 0)
        DacDmaCh1::IRQhandleInterrupt(&chState[chNum].stream);
    else
        DacDmaCh2::IRQhandleInterrupt(&chState[chNum].stream);
}

// DMA transfer for DAC channel 1
void __attribute__((used)) DacDmaCh1_IRQHandler()
{
    saveContext();
    asm volatile("mov r0, #0");
    asm volatile("bl _Z11DMA_Handlerm");
    restoreContext();
}

// DMA transfer for DAC channel 2
void __attribute__((used)) DacDmaCh2_IRQHandler()
{
    saveContext();
    asm volatile("mov r0, #1");
    asm volatile("bl _Z11DMA_Handlerm");
    restoreContext();
}



void stm32dac_init(const uint8_t instance, const uint16_t idleLevel)
{
    // Enable peripherals
    #if defined(STM32H743xx)
    RCC->APB1LENR |= RCC_APB1LENR_DAC12EN;
    #elif defined(STM32G473xx)
    RCC->AHB2ENR |= RCC_AHB2ENR_DAC1EN;
    __DSB();

    // The DAC bus interface needs its high frequency mode above 80MHz
    // (RM0440, DAC_MCR register). Mode bits at zero: buffered output on pin.
    if(SystemCoreClock > 160000000)
        DAC->MCR = DAC_MCR_HFSEL_1;
    else if(SystemCoreClock > 80000000)
        DAC->MCR = DAC_MCR_HFSEL_0;
    #else
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    #endif

    switch(instance)
    {
        case STM32_DAC_CH1:
        {
            #if defined(STM32H743xx)
            RCC->APB1LENR |= RCC_APB1LENR_LPTIM1EN;
            RCC->D2CCIP2R |= RCC_D2CCIP2R_LPTIM1SEL_0;
            DMAMUX1_Channel5->CCR = 67;
            #elif defined(STM32G473xx)
            RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;
            #else
            RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
            #endif
            __DSB();

            DAC->DHR12R1 = idleLevel;
            DAC->CR |= DAC_CR_DMAEN1    // Enable DMA
                    | DAC_TRIG_CH1      // Set trigger source for CH1
                    | DAC_CR_EN1;       // Enable CH1

            #if defined(STM32G473xx)
            while((DAC->SR & DAC_SR_DAC1RDY) == 0) ;
            #endif
        }
            break;

        case STM32_DAC_CH2:
        {
            #if defined(STM32H743xx)
            RCC->APB4ENR |= RCC_APB4ENR_LPTIM2EN;
            RCC->D3CCIPR |= RCC_D3CCIPR_LPTIM2SEL_0;
            DMAMUX1_Channel6->CCR = 68;
            #elif defined(STM32G473xx)
            RCC->APB1ENR1 |= RCC_APB1ENR1_TIM7EN;
            #else
            RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
            #endif
            __DSB();

            DAC->DHR12R2 = idleLevel;
            DAC->CR |= DAC_CR_DMAEN2    // Enable DMA
                    | DAC_TRIG_CH2      // Set trigger source for CH2
                    | DAC_CR_EN2;       // Enable CH2

            #if defined(STM32G473xx)
            while((DAC->SR & DAC_SR_DAC2RDY) == 0) ;
            #endif
        }
            break;

        default:
            break;
    }

    chState[instance].idleLevel = idleLevel;
    chState[instance].stream.setEndTransferCallback(std::bind(stopTransfer, instance));
}

void stm32dac_terminate()
{
    // Terminate streams before shutting of the peripherals
    for(int i = 0; i < 2; i++)
    {
        if(chState[i].ctx != NULL)
        {
            if(chState[i].ctx->running != 0)
                chState[i].stream.halt();
        }
    }

    #if defined(STM32H743xx)
    RCC->APB1LENR &= ~(RCC_APB1LENR_DAC12EN | RCC_APB1LENR_LPTIM1EN);
    RCC->APB4ENR  &= ~RCC_APB4ENR_LPTIM2EN;
    #elif defined(STM32G473xx)
    RCC->AHB2ENR  &= ~RCC_AHB2ENR_DAC1EN;
    RCC->APB1ENR1 &= ~(RCC_APB1ENR1_TIM6EN | RCC_APB1ENR1_TIM7EN);
    #else
    RCC->APB1ENR &= ~(RCC_APB1ENR_DACEN  | RCC_APB1ENR_TIM6EN | RCC_APB1ENR_TIM7EN);
    #endif
    __DSB();
}

static int stm32dac_start(const uint8_t instance, const void *config,
                          struct streamCtx *ctx)
{
    (void) config;

    if((ctx == NULL) || (ctx->running != 0))
        return -EINVAL;

    if(chState[instance].stream.running())
        return -EBUSY;

    __disable_irq();
    ctx->running = 1;
    __enable_irq();

    ctx->priv = &chState[instance];
    chState[instance].ctx = ctx;

    /*
     * Convert buffer elements from int16_t to unsigned 12 bit values as required
     * by the DAC. Processing can be done in-place because the API mandates that
     * the function caller does not modify the buffer content once this function
     * has been called.
     */
    S16toU12(ctx->buffer, ctx->bufSize);
    miosix::markBufferBeforeDmaWrite(ctx->buffer, ctx->bufSize*sizeof(int16_t));

    bool circ = false;
    if(ctx->bufMode == BUF_CIRC_DOUBLE)
        circ = true;

    chState[instance].stream.start(channels[instance].dacReg, ctx->buffer,
                                  ctx->bufSize, circ);

    // Configure DAC trigger
    DAC->CR |= (DAC_CR_TEN1 << (instance * 16));
    channels[instance].tim.setUpdateFrequency(ctx->sampleRate);
    channels[instance].tim.start();

    return 0;
}

static int stm32dac_idleBuf(struct streamCtx *ctx, stream_sample_t **buf)
{
    ChannelState *state = reinterpret_cast< ChannelState * >(ctx->priv);
    *buf = reinterpret_cast< stream_sample_t *>(state->stream.idleBuf());

    return ctx->bufSize/2;
}

static int stm32dac_sync(struct streamCtx *ctx, uint8_t dirty)
{
    ChannelState *state = reinterpret_cast< ChannelState * >(ctx->priv);

    if((ctx->bufMode == BUF_CIRC_DOUBLE) && (dirty != 0))
    {
        void *ptr = state->stream.idleBuf();
        size_t writeSize = ctx->bufSize/2;
        S16toU12(reinterpret_cast< int16_t *>(ptr), writeSize);
        miosix::markBufferBeforeDmaWrite(ctx->buffer, writeSize*sizeof(int16_t));
    }

   bool ok = state->stream.sync();
   if(ok) return 0;

   return -1;
}

static void stm32dac_stop(struct streamCtx *ctx)
{
    if(ctx->running == 0)
        return;

    reinterpret_cast< ChannelState * >(ctx->priv)->stream.stop();
}

static void stm32dac_halt(struct streamCtx *ctx)
{
    if(ctx->running == 0)
        return;

    reinterpret_cast< ChannelState * >(ctx->priv)->stream.halt();
}

#pragma GCC diagnostic ignored "-Wpedantic"
const struct audioDriver stm32_dac_audio_driver =
{
    .start     = stm32dac_start,
    .data      = stm32dac_idleBuf,
    .sync      = stm32dac_sync,
    .stop      = stm32dac_stop,
    .terminate = stm32dac_halt
};
#pragma GCC diagnostic pop
