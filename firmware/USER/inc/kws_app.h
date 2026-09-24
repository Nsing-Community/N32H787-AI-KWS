/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KWS_APP_H
#define KWS_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sliding-window keyword spotting over the continuous audio capture.

   One round consumes KWS_AUDIO_BLOCK_FRAMES (60 ms) of audio, computes three
   overlapping MFCC frames, advances a 49-frame fingerprint by three frames,
   runs the classifier, averages the last three softmax outputs and prints a
   line to USART1 when one keyword holds above the threshold.  The window and
   averaging lengths are the reference implementation's
   (recording_win = 3, averaging_window_len = 3, detection_threshold = 90). */

/* Number of words in g_kws_diag.  The Makefile's `benchmark` target reads
   this macro out of this header to size its OpenOCD mdw, so it must stay a
   plain literal on a line beginning with #define. */
#define KWS_DIAG_WORDS 24U

/* Index map for g_kws_diag.  Indices are the contract with the benchmark
   target; keep the order stable. */
#define KWS_DIAG_ROUNDS           0U  /* completed 60 ms rounds */
#define KWS_DIAG_DETECTIONS       1U  /* lines printed */
#define KWS_DIAG_READ_TIMEOUTS    2U  /* kws_audio_read returned 0 */
#define KWS_DIAG_DISCONTINUITIES  3U  /* stream gaps seen */
#define KWS_DIAG_LOST_BLOCKS      4U  /* audio blocks dropped by overrun */
#define KWS_DIAG_DMA_ERROR        5U  /* DMA transfer error latched */
#define KWS_DIAG_LAST_CLASS       6U  /* argmax of the averaged scores */
#define KWS_DIAG_LAST_CLASS_RAW   7U  /* argmax before averaging */
#define KWS_DIAG_AVG_MILLI        8U  /* averaged score of LAST_CLASS, x1000 */
#define KWS_DIAG_RAW_MILLI        9U  /* unaveraged score of LAST_CLASS_RAW */
#define KWS_DIAG_INVOKE_CYCLES    10U /* last inference, DWT cycles */
#define KWS_DIAG_INVOKE_MAX       11U /* worst inference */
#define KWS_DIAG_MFCC_CYCLES      12U /* last round's three MFCC frames */
#define KWS_DIAG_MFCC_MAX         13U
#define KWS_DIAG_ARENA_BYTES      14U /* TFLM arena_used_bytes() */
#define KWS_DIAG_CLASSIFIER_ERR   15U /* kws_classifier_init_error */
#define KWS_DIAG_AUDIO_STATUS     16U /* kws_audio_init_status */
#define KWS_DIAG_CODEC_STATUS     17U /* WM8978 init/paths result */
#define KWS_DIAG_CLOCK_HZ         18U /* SystemCoreClock */
#define KWS_DIAG_MCLK_HZ          19U /* codec MCLK the driver selected */
#define KWS_DIAG_INPUT_TYPE       20U /* TFLM input tensor type (must be 1=f32) */
#define KWS_DIAG_OUTPUT_TYPE      21U
#define KWS_DIAG_ROUND_CYCLES     22U /* whole round including audio wait */
#define KWS_DIAG_ROUND_MAX        23U

/* Never runs.  Exists so the diagnostics array has a home in DTCM, where it
   is uncached (a debugger reads live values through SWD without a clean) and
   survives a HardFault. */
void kws_app_run(void);

extern volatile uint32_t g_kws_diag[KWS_DIAG_WORDS];

#ifdef __cplusplus
}
#endif

#endif /* KWS_APP_H */
