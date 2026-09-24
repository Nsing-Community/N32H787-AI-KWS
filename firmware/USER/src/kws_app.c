/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kws_app.h"

#include "main.h"
#include "kws_audio.h"
#include "kws_mfcc.h"
#include "kws_classifier.h"
#include "m7_cache.h"
#include "uart_log.h"

#include <string.h>

/* Sliding window geometry, taken from the reference deployment
   (Deployment/Examples/realtime_test/main.cpp). */
#define KWS_RECORDING_WIN    3U   /* MFCC frames added per round */
#define KWS_AVERAGING_WIN    3U   /* softmax outputs averaged */
#define KWS_DETECT_THRESHOLD 0.90f
#define KWS_FIRST_KEYWORD    2    /* index of "yes"; 0 is silence, 1 unknown */
#define KWS_LED_MS           1000U  /* how long a detection holds the red LED */

/* The MFCC input window holds the new block plus the 320-sample carry that
   makes the first frame of the round overlap the previous one:
   recording_win*frame_shift + frame_len - frame_shift = 960 + 320. */
#define KWS_AUDIO_WINDOW \
    (KWS_RECORDING_WIN * KWS_MFCC_FRAME_SHIFT + KWS_MFCC_FRAME_LEN - KWS_MFCC_FRAME_SHIFT)

volatile uint32_t g_kws_diag[KWS_DIAG_WORDS]
    __attribute__((section(".kws_diagnostics"), aligned(32)));

/* DTCM scratch.  .kws_state is collected into .kws_arena, which the reset
   code zeroes before __libc_init_array. */
static int16_t g_window[KWS_AUDIO_WINDOW]
    __attribute__((section(".kws_state"), aligned(32)));
static float g_fingerprint[KWS_INPUT_FEATURES]
    __attribute__((section(".kws_state"), aligned(32)));
static float g_predictions[KWS_AVERAGING_WIN][KWS_NUM_CLASSES]
    __attribute__((section(".kws_state"), aligned(32)));
static float g_averaged[KWS_NUM_CLASSES]
    __attribute__((section(".kws_state"), aligned(32)));

static uint32_t g_real_frames;    /* genuine frames pushed, saturates at 49 */
static int g_last_reported = -1;  /* debounce: last keyword already printed */

/* The red LED on PB3.  Nothing else in this file touches it, so `g_led_lit`
   is only ever false at boot and after a flash has expired. */
static uint32_t g_led_deadline;   /* mwTick value at which the LED goes dark */
static int g_led_lit;

/* Millisecond tick, incremented by SysTick_Handler() and defined in
   n32h7xx_cfg.c. */
extern volatile uint32_t mwTick;

static uint32_t argmax(const float *v, uint32_t n)
{
    uint32_t best = 0U;
    for (uint32_t i = 1U; i < n; ++i) {
        if (v[i] > v[best]) best = i;
    }
    return best;
}

/* The capture stream broke: the fingerprint and the averaging window hold
   samples from before the gap, and splicing them onto post-gap audio would
   hand the model a word that was never spoken.  Start the window over. */
static void reset_stream(void)
{
    memset(g_fingerprint, 0, sizeof(g_fingerprint));
    memset(g_predictions, 0, sizeof(g_predictions));
    g_real_frames = 0U;
    g_last_reported = -1;
}

/* PB3 is the red LED and is active low, so lighting it means driving the pin
   low.  GPIO_Configuration() sets it high before selecting output mode, which
   is the state `g_led_lit == 0` describes. */
static void led_flash(void)
{
    GPIO_ResetBits(GPIOB, GPIO_PIN_3);
    g_led_deadline = mwTick + KWS_LED_MS;
    g_led_lit = 1;
}

/* Expiry, not a toggle: detections arrive at the rate someone speaks, so the
   LED has to be checked against a deadline.  The rounds are ~60 ms apart, so
   the flash lasts KWS_LED_MS to KWS_LED_MS + one round of audio. */
static void led_service(void)
{
    if (g_led_lit && (int32_t)(mwTick - g_led_deadline) >= 0) {
        GPIO_SetBits(GPIOB, GPIO_PIN_3);
        g_led_lit = 0;
    }
}

static void banner(void)
{
    kws_uart_puts("kws: ds-cnn-s float32, 16 kHz, classes:");
    for (uint32_t i = 0; i < KWS_NUM_CLASSES; ++i) {
        kws_uart_putc(' ');
        kws_uart_puts(kws_class_labels[i]);
    }
    kws_uart_puts("\r\n");
    if (kws_audio_init_status != 0U) {
        kws_uart_puts("kws: WARNING audio init status ");
        kws_uart_put_u32(kws_audio_init_status);
        kws_uart_puts("\r\n");
    }
    if (kws_classifier_init_error != 0) {
        kws_uart_puts("kws: WARNING classifier init error ");
        kws_uart_put_u32((uint32_t)kws_classifier_init_error);
        kws_uart_puts("\r\n");
    }
}

/* One round: 60 ms of audio in, three MFCC frames, one inference. */
static void round_once(void)
{
    const uint32_t round_start = DWT_CYCCNT;

    /* The carry has to be moved out of the tail *before* the new block lands
       on top of it: the carry is the last 320 samples of the window and the
       read below fills everything from offset 320 onwards. */
    memmove(g_window, g_window + KWS_AUDIO_BLOCK_FRAMES,
            (KWS_AUDIO_WINDOW - KWS_AUDIO_BLOCK_FRAMES) * sizeof(g_window[0]));

    if (kws_audio_read(g_window + (KWS_AUDIO_WINDOW - KWS_AUDIO_BLOCK_FRAMES),
                       200U) == 0U) {
        /* A missed block is a gap in the stream, so treat it as one rather
           than running the next round against a window with a hole in it. */
        ++g_kws_diag[KWS_DIAG_READ_TIMEOUTS];
        ++g_kws_diag[KWS_DIAG_DISCONTINUITIES];
        reset_stream();
        return;
    }
    if (kws_audio_take_discontinuity()) {
        ++g_kws_diag[KWS_DIAG_DISCONTINUITIES];
        reset_stream();
    }

    /* Shift the fingerprint left by the frames about to be replaced, exactly
       as the reference does.  Until 49 real frames have arrived the leading
       entries are still the zeros this buffer was cleared to, and inference
       is skipped, so the model never sees a padded window. */
    memmove(g_fingerprint, g_fingerprint + KWS_RECORDING_WIN * KWS_MFCC_NUM_COEFFS,
            (KWS_INPUT_FRAMES - KWS_RECORDING_WIN) * KWS_MFCC_NUM_COEFFS *
                sizeof(g_fingerprint[0]));

    const uint32_t mfcc_start = DWT_CYCCNT;
    for (uint32_t f = 0U; f < KWS_RECORDING_WIN; ++f) {
        float *dst = g_fingerprint +
                     (KWS_INPUT_FRAMES - KWS_RECORDING_WIN + f) * KWS_MFCC_NUM_COEFFS;
        kws_mfcc_compute(g_window + f * KWS_MFCC_FRAME_SHIFT, dst);
    }
    const uint32_t mfcc_cycles = DWT_CYCCNT - mfcc_start;
    g_kws_diag[KWS_DIAG_MFCC_CYCLES] = mfcc_cycles;
    if (mfcc_cycles > g_kws_diag[KWS_DIAG_MFCC_MAX])
        g_kws_diag[KWS_DIAG_MFCC_MAX] = mfcc_cycles;

    ++g_kws_diag[KWS_DIAG_ROUNDS];
    if (g_real_frames < KWS_INPUT_FRAMES) {
        g_real_frames += KWS_RECORDING_WIN;
        if (g_real_frames > KWS_INPUT_FRAMES) g_real_frames = KWS_INPUT_FRAMES;
        return;   /* cold start: the window is not full of real audio yet */
    }

    float scores[KWS_NUM_CLASSES];
    if (!kws_classifier_run(g_fingerprint, scores)) {
        ++g_kws_diag[KWS_DIAG_READ_TIMEOUTS];
        return;
    }
    if (kws_invoke_cycles > g_kws_diag[KWS_DIAG_INVOKE_MAX])
        g_kws_diag[KWS_DIAG_INVOKE_MAX] = kws_invoke_cycles;

    memmove(g_predictions, g_predictions + 1,
            (KWS_AVERAGING_WIN - 1U) * KWS_NUM_CLASSES * sizeof(g_predictions[0][0]));
    memcpy(g_predictions[KWS_AVERAGING_WIN - 1U], scores,
           KWS_NUM_CLASSES * sizeof(scores[0]));
    for (uint32_t c = 0U; c < KWS_NUM_CLASSES; ++c) {
        float sum = 0.0f;
        for (uint32_t i = 0U; i < KWS_AVERAGING_WIN; ++i) {
            sum += g_predictions[i][c];
        }
        g_averaged[c] = sum / (float)KWS_AVERAGING_WIN;
    }

    const uint32_t best = argmax(g_averaged, KWS_NUM_CLASSES);
    const uint32_t raw_best = argmax(scores, KWS_NUM_CLASSES);
    g_kws_diag[KWS_DIAG_LAST_CLASS] = best;
    g_kws_diag[KWS_DIAG_LAST_CLASS_RAW] = raw_best;
    g_kws_diag[KWS_DIAG_AVG_MILLI] = (uint32_t)(g_averaged[best] * 1000.0f);
    g_kws_diag[KWS_DIAG_RAW_MILLI] = (uint32_t)(scores[raw_best] * 1000.0f);

    if ((int)best < KWS_FIRST_KEYWORD) {
        /* Silence or the unknown bucket: whatever word was being reported has
           ended, so re-arm and allow the same word to be printed again. */
        g_last_reported = -1;
        return;
    }
    if (g_averaged[best] <= KWS_DETECT_THRESHOLD) return;
    /* Debounce: one line per utterance.  A score that dips below the
       threshold mid-word does not re-arm, so a word is not printed twice. */
    if (g_last_reported == (int)best) return;
    g_last_reported = (int)best;

    ++g_kws_diag[KWS_DIAG_DETECTIONS];
    /* Any keyword, not just "yes": the same condition that puts a line on the
       serial port lights the LED. */
    led_flash();
    kws_uart_puts(kws_class_labels[best]);
    kws_uart_putc(' ');
    kws_uart_put_u32((uint32_t)(g_averaged[best] * 100.0f));
    kws_uart_puts("%\r\n");
    kws_uart_drain();

    const uint32_t round_cycles = DWT_CYCCNT - round_start;
    g_kws_diag[KWS_DIAG_ROUND_CYCLES] = round_cycles;
    if (round_cycles > g_kws_diag[KWS_DIAG_ROUND_MAX])
        g_kws_diag[KWS_DIAG_ROUND_MAX] = round_cycles;
}

void kws_app_run(void)
{
    g_kws_diag[KWS_DIAG_ARENA_BYTES] = kws_arena_bytes;
    g_kws_diag[KWS_DIAG_CLASSIFIER_ERR] = (uint32_t)kws_classifier_init_error;
    g_kws_diag[KWS_DIAG_AUDIO_STATUS] = kws_audio_init_status;
    g_kws_diag[KWS_DIAG_CODEC_STATUS] = (uint32_t)kws_audio_codec_status;
    g_kws_diag[KWS_DIAG_CLOCK_HZ] = SystemCoreClock;
    g_kws_diag[KWS_DIAG_MCLK_HZ] = kws_audio_codec_mclk_hz;
    g_kws_diag[KWS_DIAG_INPUT_TYPE] = (uint32_t)kws_classifier_input_type;
    g_kws_diag[KWS_DIAG_OUTPUT_TYPE] = (uint32_t)kws_classifier_output_type;
    banner();

    /* An audio or model failure is reported, not fatal: the board must stay
       alive so the flasher can still reach it over SWD. */
    if (kws_audio_init_status != 0U || kws_classifier_init_error != 0) {
        uint32_t heartbeat = mwTick;
        for (;;) {
            m7_flash_poll();
            if ((uint32_t)(mwTick - heartbeat) >= 500U) {
                heartbeat = mwTick;
                GPIO_TogglePin(GPIOI, GPIO_PIN_8);
            }
        }
    }

    uint32_t heartbeat = mwTick;
    for (;;) {
        /* Keeps the USB-MSC reflash handshake alive between rounds. */
        m7_flash_poll();
        led_service();
        round_once();
        g_kws_diag[KWS_DIAG_LOST_BLOCKS] = kws_audio_lost_blocks;
        g_kws_diag[KWS_DIAG_DMA_ERROR] = kws_audio_dma_error;
        if ((uint32_t)(mwTick - heartbeat) >= 500U) {
            heartbeat = mwTick;
            GPIO_TogglePin(GPIOI, GPIO_PIN_8);
        }
    }
}
