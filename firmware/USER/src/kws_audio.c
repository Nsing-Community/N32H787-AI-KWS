/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kws_audio.h"

#include "main.h"
#include "wm8978_n32.h"
#include "n32h7xx_i2s.h"
#include "n32h7xx_dma.h"
#include "n32h7xx_dmamux.h"

#include <string.h>

#define EVENTS (DMA_CH_EVENT_BLOCK_TRANSFER_COMPLETE | DMA_CH_EVENT_ERROR)

/* The ring, its descriptors and every counter the ISR touches live in
   .kws_audio: AXI SRAM mapped Normal non-cacheable by m7_cache.c, and
   reachable by the DMA engine (TCM is not).  Because it is non-cacheable the
   ISR needs no cache maintenance and the CPU sees DMA writes as soon as the
   __DMB() retires. */
typedef struct {
    volatile int16_t ring[KWS_AUDIO_BLOCKS][KWS_AUDIO_BLOCK_WORDS];
    DMA_LinkListItemType links[KWS_AUDIO_BLOCKS];
    volatile uint32_t produced;      /* blocks the DMA has completed */
    volatile uint32_t consumed;      /* blocks the consumer has taken */
    volatile uint32_t lost;
    volatile uint32_t dma_error;
    volatile uint32_t discard;       /* drop the first, partially written block */
    volatile uint32_t discontinuity;
} kws_audio_state_t;

static kws_audio_state_t st __attribute__((section(".kws_audio"), aligned(32)));

static wm8978_t g_codec;

volatile uint32_t kws_audio_lost_blocks;
volatile uint32_t kws_audio_dma_error;
volatile uint32_t kws_audio_init_status;
volatile int32_t kws_audio_codec_status = -1;
volatile uint32_t kws_audio_codec_mclk_hz;

/* The ISR does one thing per event: clear the flag and advance a counter.  It
   never touches I2S1->DR, never clears an overrun and never restarts the
   channel -- recovery is main-loop work, so a fault here cannot cascade. */
void DMA1_Channel0_IRQHandler(void)
{
    if (DMA_GetChannelIntErrStatus(DMA1, DMA_CHANNEL_0)) {
        DMA_ClearChannelEventStatus(DMA1, DMA_CHANNEL_0, DMA_CH_EVENT_ERROR);
        st.dma_error = 1U;
    }
    if (DMA_GetChannelIntBlockStatus(DMA1, DMA_CHANNEL_0)) {
        DMA_ClearChannelEventStatus(DMA1, DMA_CHANNEL_0,
                                   DMA_CH_EVENT_BLOCK_TRANSFER_COMPLETE);
        __DMB();
        ++st.produced;
    }
}

static void stop_dma(void)
{
    /* Order matters: silence the DMA request before disabling the channel,
       then mask the events and clear the status last, so a masked-but-pending
       block interrupt cannot fire at the next enable. */
    I2S_EnableDma(I2S1, I2S_DMA_RX, DISABLE);
    I2S_Enable(I2S1, DISABLE);
    DMA_ChannelCmd(DMA1, DMA_CHANNEL_0, DISABLE);
    NVIC_DisableIRQ(DMA1_Channel0_IRQn);
    DMA_ChannelEventCmd(DMA1, DMA_CHANNEL_0, EVENTS, DISABLE);
    DMA_ClearChannelEventStatus(DMA1, DMA_CHANNEL_0, EVENTS);
    NVIC_ClearPendingIRQ(DMA1_Channel0_IRQn);
    __DSB();
}

/* Arms the receiver on a WS edge so the first full word captured is a
   complete left-channel sample.  I2S4 keeps supplying MCLK and is never
   touched here; only I2S1 is reset. */
static uint32_t start_dma(void)
{
    st.produced = 0U;
    st.consumed = 0U;
    st.discard = 1U;
    /* Both the M7 and M4 gate bits are set.  The M4 is held in reset so its
       bit is not strictly needed, but the two bits' relationship (shared
       branch or independent) is not documented, and two writes are cheaper
       than debugging a clock that never arrives. */
    RCC_EnableAHB1PeriphClk3(RCC_AHB1_PERIPHEN_M7_DMA1, ENABLE);
    RCC_EnableAHB1PeriphClk1(RCC_AHB1_PERIPHEN_M7_DMAMUX1, ENABLE);
    RCC_EnableAHB1PeriphClk3(RCC_AHB1_PERIPHEN_M4_DMA1, ENABLE);
    RCC_EnableAHB1PeriphClk1(RCC_AHB1_PERIPHEN_M4_DMAMUX1, ENABLE);
    DMA_ControllerCmd(DMA1, ENABLE);
    stop_dma();

    /* I2S_Reset() clears I2SCFGR/I2SPR/CR2, so preserve the codec-derived
       configuration across it and re-apply after. */
    uint32_t cfg = I2S1->I2SCFGR & ~1U, div = I2S1->I2SPR;
    I2S_Reset(I2S1);
    I2S1->I2SCFGR = cfg;
    I2S1->I2SPR = div;

    DMA_ChInitType init;
    DMA_ChannelStructInit(&init);
    init.SrcAddr = (uint32_t)&I2S1->DR;
    init.DstAddr = (uint32_t)st.ring[0];
    init.SrcTfrWidth = init.DstTfrWidth = DMA_CH_TRANSFER_WIDTH_16;
    init.SrcAddrCountMode = DMA_CH_ADDRESS_COUNT_MODE_NO_CHANGE;
    init.DstAddrCountMode = DMA_CH_ADDRESS_COUNT_MODE_INCREMENT;
    /* Burst 1: I2S1->DR is a single 32-bit slot and a larger burst overruns
       it. */
    init.SrcBurstLen = init.DstBurstLen = DMA_CH_BURST_LENGTH_1;
    init.TfrTypeFlowCtrl = DMA_CH_TRANSFER_FLOW_P2M_DMA;
    init.BlkTfrSize = KWS_AUDIO_BLOCK_WORDS;
    init.TfrType = DMA_CH_TRANSFER_TYPE_MULTI_BLOCK_SRCADR_LINKED_DSTADR_LINKED;
    init.SrcHandshaking = DMA_CH_SRC_HANDSHAKING_HARDWARE;
    init.SrcHsInterface = DMA_CH_HARDWARE_HANDSHAKING_IF_0;
    init.ChannelPriority = DMA_CH_PRIORITY_7;
    memset(st.links, 0, sizeof(st.links));
    for (unsigned i = 0; i < KWS_AUDIO_BLOCKS; ++i) {
        st.links[i].SrcAddr = init.SrcAddr;
        st.links[i].DstAddr = (uint32_t)st.ring[i];
        st.links[i].pNext = &st.links[(i + 1U) % KWS_AUDIO_BLOCKS];
        st.links[i].ChCtrlL32 = (uint32_t)init.ChCtrl;
        st.links[i].SrcLinkedListEn = st.links[i].DstLinkedListEn = 1U;
        st.links[i].BlkTfrSize = KWS_AUDIO_BLOCK_WORDS;
    }
    /* DstAddr already points at block 0, so starting the list at item 1 makes
       the engine write block 0 first and then walk the ring in order. */
    init.pLinkListItem = &st.links[1];
    if (DMA_ChannelInit(DMA1, &init, DMA_CHANNEL_0) != DMA_CH_STS_OK) return 3U;

    DMAMUX_SetRequestID(DMAMUX1_ID, DMAMUX_CHANNEL_0, DMAMUX1_REQUEST_I2S1_RX);
    DMA_ChannelEventCmd(DMA1, DMA_CHANNEL_0, EVENTS, ENABLE);
    NVIC_SetPriority(DMA1_Channel0_IRQn, 2U);
    NVIC_EnableIRQ(DMA1_Channel0_IRQn);
    __DMB();
    DMA_ChannelCmd(DMA1, DMA_CHANNEL_0, ENABLE);
    I2S_EnableDma(I2S1, I2S_DMA_RX, ENABLE);

    /* The codec's bit clock runs continuously, so arming has to be aligned to
       a word boundary by hand: wait for WS to fall, then -- with interrupts
       briefly off so nothing can delay the re-enable -- wait for it to rise
       and enable the receiver on that edge.  The first wait is long and keeps
       interrupts on so SysTick is not lost. */
    uint32_t started = DWT_CYCCNT;
    /* 20 ms at 600 MHz, or 320 WS periods: long enough that a healthy codec is
       never mistaken for a stalled one, short enough that a dead codec costs
       only a fifth of a second at startup. */
    const uint32_t timeout = SystemCoreClock / 50U;
    while (GPIO_ReadInputDataBit(GPIOG, GPIO_PIN_10)) {
        if ((uint32_t)(DWT_CYCCNT - started) >= timeout) return 4U;
    }
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    while (!GPIO_ReadInputDataBit(GPIOG, GPIO_PIN_10)) {
        if ((uint32_t)(DWT_CYCCNT - started) >= timeout) {
            __set_PRIMASK(primask);
            return 4U;
        }
    }
    I2S_Enable(I2S1, ENABLE);
    __set_PRIMASK(primask);
    return 0U;
}

uint32_t kws_audio_init(void)
{
    /* .kws_audio is NOLOAD, so the reset code does not zero it: clear it here
       before the DMA is armed and before the ISR can observe any of it. */
    memset((void *)&st, 0, sizeof(st));
    kws_audio_lost_blocks = 0U;
    kws_audio_dma_error = 0U;

    wm8978_status_t rc = wm8978_n32_init(&g_codec, KWS_AUDIO_SAMPLE_RATE, 16);
    kws_audio_codec_status = (int32_t)rc;
    kws_audio_codec_mclk_hz = g_codec.config.mclk_hz;
    uint32_t status = 0U;
    if (rc != WM8978_OK) {
        status = 1U;
    } else {
        /* Electret microphone: differential input, PGA +24 dB and the +20 dB
           boost, which is the gain the training corpus' levels assume.  Keep
           the DAC and output amplifiers off. */
        const wm8978_paths_t paths = {
            .input = WM8978_INPUT_MIC_DIFFERENTIAL, .mic_bias = true,
            .pga_gain = 48, .boost_20db = true,
            .headphone = false, .speaker = false
        };
        rc = wm8978_set_paths(&g_codec, &paths);
        if (rc == WM8978_OK) rc = wm8978_set_adc_volume(&g_codec, 255, 255);
        kws_audio_codec_status = (int32_t)rc;
        if (rc != WM8978_OK) {
            status = 2U;
        } else {
            /* Let the ADC bias network settle before trusting any samples. */
            SysTick_Delayms(300);
            status = start_dma();
        }
    }
    if (status != 0U && g_codec.bus.write) wm8978_power_down(&g_codec);
    kws_audio_init_status = status;
    return status;
}

/* Blocks the DMA has completed but the consumer has not taken, after the
   one-block discard is honoured.  Returns 0 when nothing is ready yet. */
static uint32_t blocks_ready(uint32_t *first)
{
    uint32_t produced = st.produced;
    uint32_t consumed = st.consumed;
    __DMB();
    if (st.discard) {
        /* Block 0 was written from wherever the DMA pointer happened to be,
           so it is not guaranteed to start on a left-channel word.  Take it
           and throw it away once block 1 has also completed. */
        if (produced - consumed < 2U) return 0U;
        st.consumed = ++consumed;
        st.discard = 0U;
    }
    if (produced == consumed) return 0U;
    uint32_t ready = produced - consumed;
    if (ready > KWS_AUDIO_BLOCKS - 1U) {
        /* The consumer fell behind by more than the ring holds.  The producer
           never stops, so resync on the newest data and report the gap rather
           than returning audio that is hours stale. */
        uint32_t lost = ready - (KWS_AUDIO_BLOCKS - 1U);
        consumed += lost;
        st.consumed = consumed;
        st.lost += lost;
        st.discontinuity = 1U;
        ready = KWS_AUDIO_BLOCKS - 1U;
    }
    *first = consumed;
    return ready;
}

unsigned kws_audio_read(int16_t *dst, uint32_t timeout_ms)
{
    if (dst == 0) return 0U;
    const uint32_t started = DWT_CYCCNT;
    const uint32_t timeout = (SystemCoreClock / 1000U) * timeout_ms;
    uint32_t first = 0U;
    while (blocks_ready(&first) == 0U) {
        if (st.dma_error) {
            kws_audio_dma_error = 1U;
            st.discontinuity = 1U;
            return 0U;
        }
        if ((uint32_t)(DWT_CYCCNT - started) >= timeout) return 0U;
    }
    const volatile int16_t *block = st.ring[first % KWS_AUDIO_BLOCKS];
    /* Both slots carry the same signal; take the left one, matching the
       capture convention the recording tools used. */
    for (unsigned i = 0; i < KWS_AUDIO_BLOCK_FRAMES; ++i) {
        dst[i] = block[i * 2U];
    }
    ++st.consumed;
    kws_audio_lost_blocks = st.lost;
    return KWS_AUDIO_BLOCK_FRAMES;
}

int kws_audio_take_discontinuity(void)
{
    if (!st.discontinuity) return 0;
    st.discontinuity = 0U;
    return 1;
}
