/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DMA_STREAM_H
#define DMA_STREAM_H

#include <kernel/scheduler/scheduler.h>
#include "stm32g4xx.h"
#include <functional>
#include <miosix.h>

/*
 * DMA driver for STM32G4. The G4 DMA is organized in channels, with the
 * peripheral requests routed through the DMAMUX, instead of the streams of the
 * F4 and H7. This file provides the same StreamHandler/DmaStream interface as
 * the F4 and H7 versions, so that the STM32 audio drivers can be shared.
 * Register description: RM0440, sections "Direct memory access controller" and
 * "DMA request multiplexer".
 * https://www.st.com/resource/en/reference_manual/rm0440-stm32g4-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
 */

/**
 * Enumerating type describing the memory and peripheral data sizes allowed
 * for a DMA transfer.
 */
enum class DataSize {
    _8BIT = 0,  ///< 8-bit data size
    _16BIT = 1, ///< 16-bit data size
    _32BIT = 2, ///< 32-bit data size
};

/**
 * Stream handler class for a STM32G4 DMA channel.
 */
class StreamHandler
{
public:
    /**
     * Constructor.
     *
     * @param ch: pointer to the DMA channel peripheral.
     * @param IRQn: DMA channel IRQ number.
     */
    StreamHandler(DMA_Channel_TypeDef *ch, IRQn_Type IRQn)
        : stopTransfer(false)
        , transferSize(0)
        , IRQn(IRQn)
        , channel(ch)
        , waiting(nullptr)
    {
    }

    /**
     * Destructor.
     */
    ~StreamHandler()
    {
        channel->CCR = 0;
        NVIC_DisableIRQ(IRQn);
    }

    /**
     * Start a DMA stream.
     *
     * @param periph: pointer to source/target peripheral.
     * @param memory: pointer to source/target memory.
     * @param size: transfer size, in elements.
     * @param circ: if true the stream runs in circular double buffered mode.
     */
    void start(volatile void *periph, void *memory, const size_t size,
               const bool circ = false)
    {
        const uint32_t circFlags = DMA_CCR_CIRC  // Circular buffer mode
                                 | DMA_CCR_HTIE; // Half transfer interrupt

        // Address and size registers can be written only with the channel off
        if (circ)
            channel->CCR |= circFlags;
        else
            channel->CCR &= ~circFlags;

        transferSize = size;
        channel->CNDTR = size;
        channel->CPAR = reinterpret_cast<uint32_t>(periph);
        channel->CMAR = reinterpret_cast<uint32_t>(memory);
        channel->CCR |= DMA_CCR_EN;
    }

    /**
     * Get a pointer to the currently "idle" section of a DMA stream that is,
     * the section not being read by the DMA.
     * A call to this function is meaningful only if the stream is running in
     * circular double buffered mode.
     *
     * @return address of the idle section or NULL if the stream is not in
     * circular mode.
     */
    void *idleBuf()
    {
        if ((channel->CCR & DMA_CCR_CIRC) == 0)
            return NULL;

        miosix::FastInterruptDisableLock dLock;
        const uint32_t curPos = channel->CNDTR;
        const uint32_t sizeCode = (channel->CCR & DMA_CCR_MSIZE)
                               >> DMA_CCR_MSIZE_Pos;
        const uint32_t elemSize = 1 << sizeCode;
        uint32_t idle = channel->CMAR;

        // The DMA is in the first half: the idle half is the second one
        if (curPos > (transferSize / 2))
            idle += (transferSize / 2) * elemSize;

        return reinterpret_cast<void *>(idle);
    }

    /**
     * Syncronize the execution flow with the stream transfer, blocking
     * function. For cirular buffer mode the calling thread is put to sleep
     * until the DMA reaches either the half or the end of the transfer,
     * whichever comes first. For linear buffer mode the calling thread is put
     * to sleep until the DMA reaches the end of the transfer.
     *
     * @return true if thread was effectively put to sleep, false otherwise.
     */
    bool sync()
    {
        using namespace miosix;

        // Enter in critical section until the end of the function
        FastInterruptDisableLock dLock;

        Thread *curThread = Thread::IRQgetCurrentThread();
        if ((waiting != nullptr) && (waiting != curThread))
            return false;

        waiting = curThread;

        do {
            Thread::IRQwait();
            {
                // Re-enable interrupts while waiting for IRQ
                FastInterruptEnableLock eLock(dLock);
                Thread::yield();
            }
        } while (waiting != nullptr);

        return true;
    }

    /**
     * Stop an ongoing DMA stream.
     * The stream does not stop immediately but only when it reaches the half
     * or the end of the transfer.
     */
    inline void stop()
    {
        stopTransfer = true;
    }

    /**
     * Forcefully stop an ongoing DMA stream.
     * Calling this function causes the immediate stop of an ongoing stream.
     */
    inline void halt()
    {
        stopTransfer = true;
        NVIC_SetPendingIRQ(IRQn);
    }

    /**
     * Query che the current status of the stream.
     *
     * @return true if the stream is active, false otherwise.
     */
    inline bool running()
    {
        return (channel->CCR & DMA_CCR_EN) != 0;
    }

    /**
     * Register a function to be called on stream end.
     *
     * @param callback: std::function for the stream end callback.
     */
    inline void setEndTransferCallback(std::function<void()> &&callback)
    {
        streamEndCallback = callback;
    }

    /**
     * Stream interrupt handler function.
     *
     * @param irqFlags: IRQ status flags.
     */
    void IRQhandler(const uint32_t irqFlags)
    {
        (void)irqFlags;

        using namespace miosix;

        if (((channel->CCR & DMA_CCR_CIRC) == 0) || (stopTransfer == true)) {
            channel->CCR &= ~DMA_CCR_EN;
            stopTransfer = false;

            if (streamEndCallback)
                streamEndCallback();
        }

        // Wake up eventual pending threads
        if (waiting == nullptr)
            return;

        waiting->IRQwakeup();
        if (waiting->IRQgetPriority()
            > Thread::IRQgetCurrentThread()->IRQgetPriority())
            Scheduler::IRQfindNextThread();

        waiting = nullptr;
    }

private:
    bool stopTransfer;
    size_t transferSize;
    const IRQn_Type IRQn;
    std::function<void()> streamEndCallback;
    DMA_Channel_TypeDef *channel;
    miosix::Thread *waiting;
};

/**
 * Stream class for a STM32G4 DMA channel.
 *
 * @tparam DMA: base address of the DMA peripheral.
 * @tparam CH: DMA channel number, from 1 to 8.
 * @tparam REQ: DMAMUX request ID of the peripheral, see RM0440 table
 * "DMAMUX: assignment of multiplexer inputs to resources".
 * @tparam PL: priority level of the DMA channel.
 */
template <uint32_t DMA, uint8_t CH, uint8_t REQ, uint8_t PL>
class DmaStream
{
public:
    /**
     * Initialize a DMA stream without starting it.
     *
     * @param irqPrio: priority of the DMA stream interrupts.
     * @param ds: data size for both source and destination locations.
     * @param memToPer: if set to true, transfer goes from memory to peripheral.
     * @return a StreamHandler object to manage the DMA stream.
     */
    static StreamHandler init(const uint8_t irqPrio, const DataSize ds,
                              const bool memToPer = false)
    {
        static_assert((CH >= 1) && (CH <= 8), "Invalid DMA channel");

        enableClock();

        const uint32_t dir = memToPer ? DMA_CCR_DIR : 0;
        const uint32_t TS = static_cast<uint8_t>(ds);
        DMA_Channel_TypeDef *channel = getChannel();

        getMuxChannel()->CCR = REQ;
        channel->CCR = (PL << DMA_CCR_PL_Pos)    // Priority
                     | (TS << DMA_CCR_MSIZE_Pos) // Memory transfer size
                     | (TS << DMA_CCR_PSIZE_Pos) // Periph. transfer size
                     | DMA_CCR_MINC              // Increment memory ptr.
                     | dir                       // Direction
                     | DMA_CCR_TCIE              // Transfer complete IRQ
                     | DMA_CCR_TEIE;             // Transfer error IRQ

        // Enable DMA interrupts
        const IRQn_Type irq = IRQn();
        NVIC_ClearPendingIRQ(irq);
        NVIC_SetPriority(irq, irqPrio);
        NVIC_EnableIRQ(irq);

        return StreamHandler(channel, irq);
    }

    /**
     * Low-level shutdown of a DMA stream.
     */
    static inline void terminate()
    {
        NVIC_DisableIRQ(IRQn());
        getChannel()->CCR = 0;
        getMuxChannel()->CCR = 0;
    }

    /**
     * Stream IRQ handler, forwards the call to the handler inside a
     * StreamHandler class.
     *
     * @param hdl: pointer to the StreamHandler class managing the stream.
     */
    static inline void IRQhandleInterrupt(StreamHandler *hdl)
    {
        const uint32_t flags = readIrqFlags();
        hdl->IRQhandler(flags);
        clearIrqFlags(flags);
    }

private:
    /**
     * Each channel has four flags in the ISR and IFCR registers: global,
     * transfer complete, half transfer and transfer error.
     */
    static constexpr uint32_t IRQ_FLAGS_MASK = 0x0F;
    static constexpr uint32_t IRQ_FLAGS_SHIFT = (CH - 1) * 4;

    static inline DMA_TypeDef *dma()
    {
        return reinterpret_cast<DMA_TypeDef *>(DMA);
    }

    /**
     * Enable DMA and DMAMUX AHB clocks.
     */
    static inline void enableClock()
    {
        if (dma() == DMA1)
            RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN | RCC_AHB1ENR_DMAMUX1EN;
        else
            RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN | RCC_AHB1ENR_DMAMUX1EN;

        __DSB();
    }

    /**
     * Obtain pointer to the DMA channel peripheral given the DMA and the
     * channel number.
     */
    static inline DMA_Channel_TypeDef *getChannel()
    {
        const uint32_t offset = (CH - 1)
                              * (DMA1_Channel2_BASE - DMA1_Channel1_BASE);

        if (dma() == DMA1)
            return reinterpret_cast<DMA_Channel_TypeDef *>(DMA1_Channel1_BASE
                                                           + offset);

        return reinterpret_cast<DMA_Channel_TypeDef *>(DMA2_Channel1_BASE
                                                       + offset);
    }

    /**
     * Obtain pointer to the DMAMUX channel feeding the DMA channel. DMAMUX
     * channels 0 to 7 go to DMA1, 8 to 15 go to DMA2.
     */
    static inline DMAMUX_Channel_TypeDef *getMuxChannel()
    {
        uint32_t index = CH - 1;
        if (dma() == DMA2)
            index += 8;

        return DMAMUX1_Channel0 + index;
    }

    /**
     * Clear the IRQ flags of a DMA channel according to a given mask.
     */
    static inline void clearIrqFlags(const uint32_t mask)
    {
        dma()->IFCR = (mask & IRQ_FLAGS_MASK) << IRQ_FLAGS_SHIFT;
    }

    /**
     * Get the current IRQ flags of a DMA channel.
     */
    static inline uint32_t readIrqFlags()
    {
        return (dma()->ISR >> IRQ_FLAGS_SHIFT) & IRQ_FLAGS_MASK;
    }

    /**
     * Get the IRQ number of a DMA channel. The IRQ numbers of channels 6 to 8
     * are not contiguous with the others.
     */
    static inline IRQn_Type IRQn()
    {
        static constexpr IRQn_Type dma1Irq[] = {
            DMA1_Channel1_IRQn, DMA1_Channel2_IRQn, DMA1_Channel3_IRQn,
            DMA1_Channel4_IRQn, DMA1_Channel5_IRQn, DMA1_Channel6_IRQn,
            DMA1_Channel7_IRQn, DMA1_Channel8_IRQn,
        };

        static constexpr IRQn_Type dma2Irq[] = {
            DMA2_Channel1_IRQn, DMA2_Channel2_IRQn, DMA2_Channel3_IRQn,
            DMA2_Channel4_IRQn, DMA2_Channel5_IRQn, DMA2_Channel6_IRQn,
            DMA2_Channel7_IRQn, DMA2_Channel8_IRQn,
        };

        if (dma() == DMA1)
            return dma1Irq[CH - 1];

        return dma2Irq[CH - 1];
    }
};

#endif /* DMA_STREAM_H */
