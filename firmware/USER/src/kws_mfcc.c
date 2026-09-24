/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kws_mfcc.h"

#include <math.h>
#include <string.h>
#include <float.h>

/* Kept in DTCM with the arena: RW_SRAM is only 8 KiB and also holds the stack.
   The reset code zeroes this before the C runtime starts. */
#define KWS_MFCC_STATE __attribute__((section(".kws_state"), aligned(16)))

/* Number of non-zero FFT bins any one mel filter touches.  The widest filter
   is the last one: 2 * (mel_span / 41) converted back to Hz at 4 kHz is about
   28 bins wide, so 64 leaves room to raise the band count without a silent
   truncation.  kws_mfcc_init() fails loudly if this is ever too small. */
#define KWS_MFCC_MAX_SPAN 64U

#define M_2PI 6.283185307179586476925286766559005

typedef struct {
    float window[KWS_MFCC_FRAME_LEN];
    float dct[KWS_MFCC_NUM_COEFFS][KWS_MFCC_NUM_BINS];
    float fbank[KWS_MFCC_NUM_BINS][KWS_MFCC_MAX_SPAN];
    float cos_tab[KWS_MFCC_FFT_LEN / 2U];
    float sin_tab[KWS_MFCC_FFT_LEN / 2U];
    int32_t fbank_first[KWS_MFCC_NUM_BINS];
    int32_t fbank_last[KWS_MFCC_NUM_BINS];
    /* Scratch, kept here so the per-frame path touches no stack. */
    float re[KWS_MFCC_FFT_LEN];
    float im[KWS_MFCC_FFT_LEN];
    float power[KWS_MFCC_FFT_LEN / 2U + 1U];
    float mel_energy[KWS_MFCC_NUM_BINS];
} kws_mfcc_state_t;

static KWS_MFCC_STATE kws_mfcc_state_t g_mfcc;

volatile uint32_t kws_mfcc_init_error;

static float mel_scale(float freq)
{
    return 1127.0f * logf(1.0f + freq / 700.0f);
}

/* Iterative radix-2 Cooley-Tukey, in place.  The input is real (im all zero)
   but no real-input trick is used: a 1024-point complex transform is about
   0.1 ms at 480 MHz and three of them per 60 ms round is not worth the
   extra failure modes. */
static void fft_forward(float *re, float *im)
{
    const unsigned n = KWS_MFCC_FFT_LEN;
    for (unsigned i = 1, j = 0; i < n; ++i) {
        unsigned bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (unsigned len = 2; len <= n; len <<= 1) {
        const unsigned half = len >> 1;
        const unsigned step = n / len;
        for (unsigned base = 0; base < n; base += len) {
            for (unsigned k = 0; k < half; ++k) {
                const unsigned t = k * step;
                const float wr = g_mfcc.cos_tab[t];
                const float wi = g_mfcc.sin_tab[t];
                const unsigned a = base + k;
                const unsigned b = a + half;
                const float xr = re[b] * wr - im[b] * wi;
                const float xi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - xr;
                im[b] = im[a] - xi;
                re[a] += xr;
                im[a] += xi;
            }
        }
    }
}

static void build_window(void)
{
    for (unsigned i = 0; i < KWS_MFCC_FRAME_LEN; ++i) {
        /* Periodic Hann: the divisor is frame_len, not frame_len - 1.  This
           matches both Arm's window_func[] and TensorFlow's hann_window
           (periodic=True in the training script). */
        g_mfcc.window[i] =
            0.5f - 0.5f * cosf((float)(M_2PI * (double)i / (double)KWS_MFCC_FRAME_LEN));
    }
}

static void build_dct(void)
{
    const float normalizer = sqrtf(2.0f / (float)KWS_MFCC_NUM_BINS);
    for (unsigned k = 0; k < KWS_MFCC_NUM_COEFFS; ++k) {
        for (unsigned n = 0; n < KWS_MFCC_NUM_BINS; ++n) {
            g_mfcc.dct[k][n] = normalizer *
                cosf((float)((double)M_PI / (double)KWS_MFCC_NUM_BINS *
                             ((double)n + 0.5) * (double)k));
        }
    }
}

static void build_fbank(void)
{
    /* num_fft_bins is half the padded length: bins 0..511.  Bin 512 (the
       8 kHz Nyquist term) is outside every filter's range, so it is not
       scanned -- the same slice the Arm reference uses. */
    const int32_t num_fft_bins = (int32_t)(KWS_MFCC_FFT_LEN / 2U);
    const float fft_bin_width =
        (float)KWS_MFCC_SAMP_FREQ / (float)KWS_MFCC_FFT_LEN;
    const float mel_low = mel_scale((float)KWS_MFCC_MEL_LOW_HZ);
    const float mel_high = mel_scale((float)KWS_MFCC_MEL_HIGH_HZ);
    /* 41, not 40: TensorFlow's MelFilterbank spaces its 41 centre
       frequencies by mel_span / (num_channels + 1). */
    const float mel_delta = (mel_high - mel_low) / (float)(KWS_MFCC_NUM_BINS + 1U);

    for (unsigned bin = 0; bin < KWS_MFCC_NUM_BINS; ++bin) {
        const float left_mel = mel_low + (float)bin * mel_delta;
        const float center_mel = mel_low + ((float)bin + 1.0f) * mel_delta;
        const float right_mel = mel_low + ((float)bin + 2.0f) * mel_delta;
        int32_t first = -1, last = -1;
        unsigned kept = 0;
        for (int32_t i = 0; i < num_fft_bins; ++i) {
            const float mel = mel_scale(fft_bin_width * (float)i);
            float weight = 0.0f;
            if (mel > left_mel && mel < right_mel) {
                weight = (mel <= center_mel)
                    ? (mel - left_mel) / (center_mel - left_mel)
                    : (right_mel - mel) / (right_mel - center_mel);
            }
            if (weight == 0.0f) continue;
            if (kept == KWS_MFCC_MAX_SPAN) {
                /* Would drop part of the filter; refuse rather than compute a
                   silently wrong feature for the rest of the session. */
                kws_mfcc_init_error = 2U;
                return;
            }
            if (first < 0) first = i;
            last = i;
            /* Stored packed from `first`, so an interior zero weight (which
               cannot occur here, but would be a hole otherwise) would still
               line up with the index arithmetic in kws_mfcc_compute(). */
            g_mfcc.fbank[bin][kept++] = weight;
        }
        if (first < 0) {
            kws_mfcc_init_error = 1U;
            return;
        }
        g_mfcc.fbank_first[bin] = first;
        g_mfcc.fbank_last[bin] = last;
    }
}

void kws_mfcc_init(void)
{
    kws_mfcc_init_error = 0U;
    memset(&g_mfcc, 0, sizeof(g_mfcc));
    build_window();
    build_dct();
    build_fbank();
    if (kws_mfcc_init_error != 0U) return;
    for (unsigned i = 0; i < KWS_MFCC_FFT_LEN / 2U; ++i) {
        const double angle = -M_2PI * (double)i / (double)KWS_MFCC_FFT_LEN;
        g_mfcc.cos_tab[i] = (float)cos(angle);
        g_mfcc.sin_tab[i] = (float)sin(angle);
    }
}

void kws_mfcc_compute(const int16_t *frame, float out[KWS_MFCC_NUM_COEFFS])
{
    const uint32_t half = KWS_MFCC_FFT_LEN / 2U;

    /* TensorFlow's normalisation of 16-bit PCM to (-1, 1). */
    for (unsigned i = 0; i < KWS_MFCC_FRAME_LEN; ++i) {
        g_mfcc.re[i] = ((float)frame[i] / 32768.0f) * g_mfcc.window[i];
    }
    for (unsigned i = KWS_MFCC_FRAME_LEN; i < KWS_MFCC_FFT_LEN; ++i) {
        g_mfcc.re[i] = 0.0f;
    }
    memset(g_mfcc.im, 0, sizeof(g_mfcc.im));

    fft_forward(g_mfcc.re, g_mfcc.im);

    /* Power spectrum.  Re[0] is DC and Re[half] is the Nyquist term, so the
       array holds 513 bins while the transform only produces 512 complex
       outputs.  magnitude_squared=True in the training script means power,
       not magnitude, at this stage. */
    g_mfcc.power[0] = g_mfcc.re[0] * g_mfcc.re[0];
    for (uint32_t i = 1; i < half; ++i) {
        g_mfcc.power[i] = g_mfcc.re[i] * g_mfcc.re[i] +
                          g_mfcc.im[i] * g_mfcc.im[i];
    }
    g_mfcc.power[half] = g_mfcc.re[half] * g_mfcc.re[half];

    for (unsigned bin = 0; bin < KWS_MFCC_NUM_BINS; ++bin) {
        const int32_t first = g_mfcc.fbank_first[bin];
        const int32_t last = g_mfcc.fbank_last[bin];
        float energy = 0.0f;
        unsigned j = 0;
        for (int32_t i = first; i <= last; ++i) {
            /* sqrt here, before weighting, is what TensorFlow's mel filterbank
               does internally -- see docs/KWS_DEPLOYMENT.md.  Moving it out
               would change every coefficient. */
            energy += sqrtf(g_mfcc.power[i]) * g_mfcc.fbank[bin][j++];
        }
        /* Any real microphone self-noise keeps this well above zero; the
           guard is only for a fully silent (all-zero) input frame. */
        g_mfcc.mel_energy[bin] = logf(energy == 0.0f ? FLT_MIN : energy);
    }

    for (unsigned k = 0; k < KWS_MFCC_NUM_COEFFS; ++k) {
        float sum = 0.0f;
        for (unsigned n = 0; n < KWS_MFCC_NUM_BINS; ++n) {
            sum += g_mfcc.dct[k][n] * g_mfcc.mel_energy[n];
        }
        /* Arm scales by 2^mfcc_dec_bits and clamps to q7 for the quantised
           models.  DS_CNN_S.tflite is float32, so the coefficients go out
           unscaled. */
        out[k] = sum;
    }
}
