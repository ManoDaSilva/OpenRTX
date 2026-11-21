/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <kernel/scheduler/scheduler.h>
#include "core/cache_cortexMx.h"
#include "interfaces/delays.h"
#include "peripherals/gpio.h"
#include "DmaStream.hpp"
#include "hwconfig.h"
#include <miosix.h>
#include <errno.h>
#include "stm32_adc.h"

#if defined(STM32H743xx)
#include "Lptim.hpp"

typedef Lptim          timebase_type;
#define TimebaseCh1    Lptim(LPTIM3_BASE, 168000000)
#define TimebaseCh2    Lptim(LPTIM3_BASE, 168000000)
#define TimebaseCh3    Lptim(LPTIM3_BASE, 168000000)
#elif defined(STM32G473xx)
#include "Timer.hpp"

typedef Timer          timebase_type;
#define TimebaseCh1    Timer(TIM2_BASE)
#define TimebaseCh2    Timer(TIM2_BASE)
#define TimebaseCh3    Timer(TIM2_BASE)
#else
#include "Timer.hpp"

typedef Timer          timebase_type;
#define TimebaseCh1    Timer(TIM2_BASE)
#define TimebaseCh2    Timer(TIM2_BASE)
#define TimebaseCh3    Timer(TIM2_BASE)
#endif

struct AdcPeriph
{
    ADC_TypeDef   *adc;
    StreamHandler *stream;
    timebase_type  tim;
};

#if defined(STM32G473xx)
/*
 * DMAMUX request IDs from the STM32G4 HAL, stm32g4xx_hal_dma.h
 * (DMA_REQUEST_ADC1/2/3).
 * https://github.com/STMicroelectronics/stm32g4xx_hal_driver
 */
using AdcDma1 = DmaStream< DMA1_BASE, 3,  5, 2 >; // DMA 1, Channel 3, high priority (ADC1)
using AdcDma2 = DmaStream< DMA1_BASE, 4, 36, 2 >; // DMA 1, Channel 4, high priority (ADC2)
using AdcDma3 = DmaStream< DMA1_BASE, 5, 37, 2 >; // DMA 1, Channel 5, high priority (ADC3)
#define AdcDma1_IRQHandler    DMA1_Channel3_IRQHandler
#define AdcDma2_IRQHandler    DMA1_Channel4_IRQHandler
#define AdcDma3_IRQHandler    DMA1_Channel5_IRQHandler

/*
 * Sampling time of 47.5 ADC clock cycles for all the channels (SMPx = 0b100,
 * RM0440 ADC_SMPR1/ADC_SMPR2): with the ADC clocked at HCLK/4 a conversion
 * takes 60 cycles, about 1.4us.
 */
#define ADC_SMPR1_ALL_47CYC    0x24924924
#define ADC_SMPR2_ALL_47CYC    0x04924924
#else
using AdcDma1 = DmaStream< DMA2_BASE, 0, 0, 2 >; // DMA 2, Stream 0, channel 0, high priority (ADC1)
using AdcDma2 = DmaStream< DMA2_BASE, 2, 1, 2 >; // DMA 2, Stream 2, channel 1, high priority (ADC2)
using AdcDma3 = DmaStream< DMA2_BASE, 1, 2, 2 >; // DMA 2, Stream 1, channel 2, high priority (ADC3)
#define AdcDma1_IRQHandler    DMA2_Stream0_IRQHandler
#define AdcDma2_IRQHandler    DMA2_Stream2_IRQHandler
#define AdcDma3_IRQHandler    DMA2_Stream1_IRQHandler
#endif

StreamHandler AdcDma1_hdl = AdcDma1::init(10, DataSize::_16BIT, false);
StreamHandler AdcDma2_hdl = AdcDma2::init(10, DataSize::_16BIT, false);
StreamHandler AdcDma3_hdl = AdcDma3::init(10, DataSize::_16BIT, false);

static struct streamCtx *AdcContext[3];

static constexpr AdcPeriph periph[] =
{
    {(ADC_TypeDef *) ADC1_BASE, &AdcDma1_hdl, TimebaseCh1},
    {(ADC_TypeDef *) ADC2_BASE, &AdcDma2_hdl, TimebaseCh2},
    {(ADC_TypeDef *) ADC3_BASE, &AdcDma3_hdl, TimebaseCh3},
};


/**
 * \internal
 * Stop an ongoing transfer, deactivating timers and DMA stream.
 */
static void stopTransfer(const uint8_t instNum)
{
    ADC_TypeDef *adc = periph[instNum].adc;

    #if defined(STM32H743xx) || defined(STM32G473xx)
    adc->CR |= ADC_CR_ADSTP;
    while((adc->CR & ADC_CR_ADSTP) != 0) ;
    #else
    adc->CR2 &= ~ADC_CR2_ADON;
    #endif

    periph[instNum].tim.stop();
    AdcContext[instNum]->running = 0;
}

/**
 * \internal
 * Actual implementation of DMA interrupt handler.
 */
void __attribute__((used)) ADC_DMA_Handler(uint32_t instNum)
{
    switch(instNum)
    {
        case 0:
            AdcDma1::IRQhandleInterrupt(periph[instNum].stream);
            break;

        case 1:
            AdcDma2::IRQhandleInterrupt(periph[instNum].stream);
            break;

        case 2:
            AdcDma3::IRQhandleInterrupt(periph[instNum].stream);
            break;
    }
}

// DMA transfer for ADC1
void __attribute__((used)) AdcDma1_IRQHandler()
{
    saveContext();
    asm volatile("mov r0, #0");
    asm volatile("bl _Z15ADC_DMA_Handlerm");
    restoreContext();
}

// DMA transfer for ADC2
void __attribute__((used)) AdcDma2_IRQHandler()
{
    saveContext();
    asm volatile("mov r0, #1");
    asm volatile("bl _Z15ADC_DMA_Handlerm");
    restoreContext();
}

// DMA transfer for ADC3
void __attribute__((used)) AdcDma3_IRQHandler()
{
    saveContext();
    asm volatile("mov r0, #2");
    asm volatile("bl _Z15ADC_DMA_Handlerm");
    restoreContext();
}



void stm32adc_init(const uint8_t instance)
{
    // Enable peripherals
    switch(instance)
    {
        case STM32_ADC_ADC1:
            #ifdef STM32H743xx
            RCC->AHB1ENR     |= RCC_AHB1ENR_ADC12EN;
            ADC12_COMMON->CCR = ADC_CCR_CKMODE_1
                              | ADC_CCR_CKMODE_0;
            DMAMUX1_Channel8->CCR = 9;
            #elif defined(STM32G473xx)
            RCC->AHB2ENR       |= RCC_AHB2ENR_ADC12EN;
            ADC12_COMMON->CCR  |= ADC_CCR_CKMODE_1   // HCLK/4, max is 60MHz
                               |  ADC_CCR_CKMODE_0;
            #else
            RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
            #endif
            break;

        case STM32_ADC_ADC2:
            #ifdef STM32H743xx
            RCC->AHB1ENR     |= RCC_AHB1ENR_ADC12EN;
            ADC12_COMMON->CCR = ADC_CCR_CKMODE_1
                              | ADC_CCR_CKMODE_0;
            DMAMUX1_Channel10->CCR = 10;
            #elif defined(STM32G473xx)
            RCC->AHB2ENR       |= RCC_AHB2ENR_ADC12EN;
            ADC12_COMMON->CCR  |= ADC_CCR_CKMODE_1
                               |  ADC_CCR_CKMODE_0;
            #else
            RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;
            #endif
            break;

        case STM32_ADC_ADC3:
            #ifdef STM32H743xx
            RCC->AHB4ENR    |= RCC_AHB4ENR_ADC3EN;
            ADC3_COMMON->CCR = ADC_CCR_CKMODE_1
                             | ADC_CCR_CKMODE_0;
            DMAMUX1_Channel9->CCR = 115;
            #elif defined(STM32G473xx)
            RCC->AHB2ENR       |= RCC_AHB2ENR_ADC345EN;
            ADC345_COMMON->CCR |= ADC_CCR_CKMODE_1
                               |  ADC_CCR_CKMODE_0;
            #else
            RCC->APB2ENR |= RCC_APB2ENR_ADC3EN;
            #endif
            break;
    }

    /*
     * Use TIM2 as trigger source for all the ADCs.
     * TODO: change this with a dedicated timer for each ADC.
     */
    #ifdef STM32H743xx
    RCC->APB4ENR |= RCC_APB4ENR_LPTIM3EN;
    RCC->D3CCIPR |= RCC_D3CCIPR_LPTIM345SEL_0;
    uint32_t adcTrig = 20 << ADC_CFGR_EXTSEL_Pos;    // lptim3_out as trig. source
    #elif defined(STM32G473xx)
    // TIM2_TRGO as trigger source, from the STM32G4 HAL, stm32g4xx_ll_adc.h
    // (LL_ADC_REG_TRIG_EXT_TIM2_TRGO)
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
    uint32_t adcTrig = 11 << ADC_CFGR_EXTSEL_Pos;
    #else
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    ADC->CCR     |= ADC_CCR_ADCPRE_0;
    uint32_t adcTrig = ADC_CR2_EXTSEL_2     // 0b0110 TIM2_TRGO trig. source
                     | ADC_CR2_EXTSEL_1;
    #endif
    __DSB();

    ADC_TypeDef *adc = periph[instance].adc;

    #ifdef STM32H743xx
    adc->CR = ADC_CR_ADVREGEN
            | ADC_CR_BOOST_1
            | ADC_CR_BOOST_0;

    while((adc->ISR & ADC_ISR_LDORDY) == 0) ;

    adc->ISR   = ADC_ISR_ADRDY;     // Clear the ADRDY flag
    adc->CR   |= ADC_CR_ADEN;
    while((adc->ISR & ADC_ISR_ADRDY) != 0) ;

    adc->SMPR2 = 0x36DB6DB6;
    adc->SMPR1 = 0x36DB6DB6;
    adc->SQR1  = 0;                 // Convert one channel
    adc->CFGR |= ADC_CFGR_DISCEN
              |  ADC_CFGR_EXTEN_0   // Trigger on rising edge
              |  adcTrig            // Trigger source
              |  ADC_CFGR_RES_1     // 12-bit resolution
              |  ADC_CFGR_DMNGT_1   // Continuous DMA requests
              |  ADC_CFGR_DMNGT_0;  // Enable DMA data transfer
    #elif defined(STM32G473xx)
    /*
     * Power up sequence from RM0440, section "ADC on-off control": exit deep
     * power down, enable the voltage regulator and wait for its start-up time
     * (tADCVREG_STUP, 20us max), calibrate in single-ended mode, wait four ADC
     * clock cycles, then enable the ADC.
     */
    adc->CR &= ~ADC_CR_DEEPPWD;
    adc->CR |= ADC_CR_ADVREGEN;
    delayUs(20);

    adc->CR &= ~ADC_CR_ADCALDIF;
    adc->CR |= ADC_CR_ADCAL;
    while((adc->CR & ADC_CR_ADCAL) != 0) ;
    delayUs(1);

    adc->ISR   = ADC_ISR_ADRDY;     // Clear the ADRDY flag
    adc->CR   |= ADC_CR_ADEN;
    while((adc->ISR & ADC_ISR_ADRDY) == 0) ;

    adc->SMPR1 = ADC_SMPR1_ALL_47CYC;
    adc->SMPR2 = ADC_SMPR2_ALL_47CYC;
    adc->SQR1  = 0;                 // Convert one channel
    adc->CFGR |= ADC_CFGR_EXTEN_0   // Trigger on rising edge
              |  adcTrig            // Trigger source, 12-bit resolution
              |  ADC_CFGR_OVRMOD    // Overwrite data on overrun
              |  ADC_CFGR_DMACFG    // Continuous DMA requests
              |  ADC_CFGR_DMAEN;    // Enable DMA data transfer
    #else
    adc->SMPR2 = ADC_SMPR2_SMP2_2
               | ADC_SMPR2_SMP1_2;
    adc->SQR1  = 0;                 // Convert one channel
    adc->CR1  |= ADC_CR1_DISCEN;
    adc->CR2  |= ADC_CR2_EXTEN_0    // Trigger on rising edge
              |  adcTrig            // Trigger source
              |  ADC_CR2_DDS        // Continuous DMA requests
              |  ADC_CR2_DMA;       // Enable DMA data transfer
    #endif

    // Register end-of-transfer callbacks
    periph[instance].stream->setEndTransferCallback(std::bind(stopTransfer, instance));
}

void stm32adc_terminate()
{
    // Terminate streams before shutting of the peripherals
    for(int i = 0; i < 2; i++)
    {
        if(AdcContext[i] != NULL)
        {
            if(AdcContext[i]->running != 0)
                periph[i].stream->halt();
        }
    }

    // TODO: turn off peripherals
    #ifdef STM32H743xx
    RCC->APB1LENR &= ~RCC_APB1LENR_TIM2EN;
    #elif defined(STM32G473xx)
    RCC->APB1ENR1 &= ~RCC_APB1ENR1_TIM2EN;
    #else
    RCC->APB1ENR &= ~RCC_APB1ENR_TIM2EN;
    #endif
    __DSB();
}

static int stm32adc_start(const uint8_t instance, const void *config, struct streamCtx *ctx)
{
    if(ctx == NULL)
        return -EINVAL;

    if((ctx->running != 0) || (periph[instance].stream->running()))
        return -EBUSY;

    __disable_irq();
    ctx->running = 1;
    __enable_irq();

    ctx->priv = (void *) &periph[instance];
    AdcContext[instance] = ctx;

    // Set conversion channel and enable the ADC
    ADC_TypeDef *adc = periph[instance].adc;

    #ifdef STM32H743xx
    uint32_t channel = (uint32_t) config;
    adc->SQR1  = channel << ADC_SQR1_SQ1_Pos;
    adc->PCSEL = 1 << channel;
    adc->CR   |= ADC_CR_ADSTART;
    #elif defined(STM32G473xx)
    uint32_t channel = (uint32_t) config;
    adc->SQR1  = channel << ADC_SQR1_SQ1_Pos;
    adc->CR   |= ADC_CR_ADSTART;
    #else
    adc->SQR3 = (uint32_t) config;
    adc->CR2 |= ADC_CR2_ADON;
    #endif

    // Start DMA stream
    bool circ = false;
    if(ctx->bufMode == BUF_CIRC_DOUBLE)
        circ = true;

    periph[instance].stream->start(&(periph[instance].adc->DR), ctx->buffer,
                                   ctx->bufSize, circ);

    // Configure ADC trigger
    periph[instance].tim.setUpdateFrequency(ctx->sampleRate);
    periph[instance].tim.start();

    return 0;
}

static int stm32adc_data(struct streamCtx *ctx, stream_sample_t **buf)
{
    AdcPeriph *p = reinterpret_cast< AdcPeriph * >(ctx->priv);

    // Default case: linear mode
    *buf = ctx->buffer;
    int size = ctx->bufSize;

    if(ctx->bufMode == BUF_CIRC_DOUBLE)
    {
        *buf = reinterpret_cast< stream_sample_t *>(p->stream->idleBuf());
        size = ctx->bufSize/2;
    }

    miosix::markBufferAfterDmaRead(*buf, size*sizeof(int16_t));
    return size;
}

static int stm32adc_sync(struct streamCtx *ctx, uint8_t dirty)
{
    (void) dirty;

    AdcPeriph *p = reinterpret_cast< AdcPeriph * >(ctx->priv);
    bool ok = p->stream->sync();
    if(ok)
        return 0;

   return -1;
}

static void stm32adc_stop(struct streamCtx *ctx)
{
    if(ctx->running == 0)
        return;

    reinterpret_cast< AdcPeriph * >(ctx->priv)->stream->stop();
}

static void stm32adc_halt(struct streamCtx *ctx)
{
    if(ctx->running == 0)
        return;

    reinterpret_cast< AdcPeriph * >(ctx->priv)->stream->halt();
}

#pragma GCC diagnostic ignored "-Wpedantic"
const struct audioDriver stm32_adc_audio_driver =
{
    .start     = stm32adc_start,
    .data      = stm32adc_data,
    .sync      = stm32adc_sync,
    .stop      = stm32adc_stop,
    .terminate = stm32adc_halt
};
#pragma GCC diagnostic pop
