/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KWS_AUDIO_H
#define KWS_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Continuous 16 kHz microphone capture for the keyword spotter.

   I2S1 runs as a slave receiver fed by the WM8978, which is the I2S master,
   so the bit clock never stops.  A DMA1 channel walks a circular link list
   and raises a block interrupt; the CPU consumes whole blocks.

   One DMA block is deliberately one application round: 60 ms of 16 kHz mono
   audio, which is also exactly what three MFCC frames of hop 320 need.  That
   makes block, round and feature-window advance the same quantity, so the
   consumer never has to reason about partial blocks. */

#define KWS_AUDIO_SAMPLE_RATE  16000U
#define KWS_AUDIO_BLOCK_FRAMES 960U   /* 60 ms of mono samples */
#define KWS_AUDIO_BLOCKS       8U     /* ring depth = 480 ms */
#define KWS_AUDIO_BLOCK_WORDS  (KWS_AUDIO_BLOCK_FRAMES * 2U) /* stereo int16 */

/* Configures the WM8978 over I2C4 and arms the DMA ring.  Must be called
   after SysTick_Config(), I2C_Configuration(), the M4 is held in reset and
   m7_cache_enable() has installed the non-cacheable mapping for the ring.
   Returns 0 on success; 1 WM8978 init, 2 codec paths, 3 DMA channel setup,
   4 WS-edge alignment timeout.  A failure is reported, not fatal. */
uint32_t kws_audio_init(void);

/* Waits up to timeout_ms for the next whole block and writes
   KWS_AUDIO_BLOCK_FRAMES mono samples (left channel only; the codec's
   differential front end puts the same signal on both slots) to dst.
   Returns the frame count written, or 0 on timeout.  The first block after
   kws_audio_init() is discarded, because the DMA may have started writing
   into the middle of it. */
unsigned kws_audio_read(int16_t *dst, uint32_t timeout_ms);

/* Non-zero once if blocks were dropped, or the stream was restarted, since
   the previous call.  The caller must reset its MFCC frame buffer and
   sliding window when this is set: a gap silently spliced into the feature
   window is the one failure a keyword spotter cannot recover from.  Reading
   it clears it. */
int kws_audio_take_discontinuity(void);

/* Blocks skipped because the consumer fell behind.  Should stay 0. */
extern volatile uint32_t kws_audio_lost_blocks;
/* Non-zero once if the DMA engine reported a transfer error. */
extern volatile uint32_t kws_audio_dma_error;
/* Value returned by the last kws_audio_init(). */
extern volatile uint32_t kws_audio_init_status;
/* WM8978 driver result codes, for post-mortem over SWD. */
extern volatile int32_t kws_audio_codec_status;
extern volatile uint32_t kws_audio_codec_mclk_hz;

#ifdef __cplusplus
}
#endif

#endif /* KWS_AUDIO_H */
